#include "UpdateDownload.h"
#include "cycode/update/detail/UpdateManifest.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include "cycode/Updates.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace cycode::updates {
namespace {
std::string apiRoot() {
    return "https://api.github.com/repos/" + std::string(repository) + "/releases";
}
#ifdef _WIN32
struct Internet {
    HINTERNET value;
    explicit Internet(HINTERNET v) : value(v) {
        if (!value)
            throw DownloadError(DownloadIssue::network, "Cannot open update connection");
    }
    ~Internet() {
        WinHttpCloseHandle(value);
    }
    Internet(const Internet &) = delete;
};
std::wstring wide(const std::string &text) {
    return utf8Path(text).wstring();
}
std::string token() {
    for (const auto *name : {"CYCODE_UPDATE_TOKEN", "GH_TOKEN", "GITHUB_TOKEN"})
        if (const auto *value = std::getenv(name); value && *value) {
            const std::string result = value;
            if (result.size() > 8192 ||
                std::any_of(result.begin(), result.end(), [](unsigned char c) { return c <= 32 || c > 126; }))
                throw DownloadError(DownloadIssue::access, "Invalid update token environment value");
            return result;
        }
    return {};
}
std::string header(HINTERNET request, DWORD kind) {
    DWORD bytes = 0, index = 0;
    WinHttpQueryHeaders(request, kind, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &bytes, &index);
    if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
        return {};
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !bytes || bytes > 2048)
        throw DownloadError(DownloadIssue::response, "Invalid update response header");
    std::wstring value(bytes / sizeof(wchar_t), 0);
    index = 0;
    if (!WinHttpQueryHeaders(request, kind, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &bytes, &index))
        throw DownloadError(DownloadIssue::response, "Cannot read update response header");
    value.resize(bytes / sizeof(wchar_t));
    bytes = 0;
    WinHttpQueryHeaders(request, kind, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &bytes, &index);
    if (GetLastError() != ERROR_WINHTTP_HEADER_NOT_FOUND ||
        std::any_of(value.begin(), value.end(), [](wchar_t c) { return c < 32 || c > 126; }))
        throw DownloadError(DownloadIssue::response, "Ambiguous update response header");
    std::string result;
    result.reserve(value.size());
    for (const auto c : value)
        result.push_back(static_cast<char>(c)); // ASCII was checked above.
    return result;
}
void rejectCheckpoint(const std::filesystem::path &marker, const std::string &metadata) {
    auto rejected = nlohmann::json::parse(metadata);
    rejected["rejected"] = true;
    writeTextFileAtomic(marker, rejected.dump(2) + "\n", metadata);
}
struct PartialFile {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint64_t received = 0;
    PartialFile(const std::filesystem::path &file, uint64_t offset) {
        detail::safe(file);
        handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                             std::filesystem::exists(file) ? OPEN_EXISTING : CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw DownloadError(DownloadIssue::storage, "Cannot open private update download");
        BY_HANDLE_FILE_INFORMATION info{};
        LARGE_INTEGER size{}, position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        const bool valid =
            GetFileInformationByHandle(handle, &info) && GetFileSizeEx(handle, &size) &&
            !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) &&
            info.nNumberOfLinks == 1 && GetFileType(handle) == FILE_TYPE_DISK && size.QuadPart >= 0 &&
            static_cast<uint64_t>(size.QuadPart) == offset &&
            SetFilePointerEx(handle, position, nullptr, FILE_BEGIN);
        if (!valid) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            throw DownloadError(DownloadIssue::storage, "Partial update changed before resuming");
        }
        received = offset;
    }
    PartialFile(const PartialFile &) = delete;
    ~PartialFile() {
        if (handle != INVALID_HANDLE_VALUE) {
            // Best effort on interruption. A future attempt uses the actual file size,
            // and only the complete GitHub digest can make these bytes installable.
            FlushFileBuffers(handle);
            CloseHandle(handle);
        }
    }
    void write(const char *data, size_t size, uint64_t maximum) {
        if (size > maximum - received)
            throw DownloadError(DownloadIssue::response, "Update exceeds its recorded size");
        DWORD written = 0;
        if (!WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr) || written != size)
            throw DownloadError(DownloadIssue::storage, "Cannot write update download");
        received += written;
    }
    void flush() {
        if (!FlushFileBuffers(handle))
            throw DownloadError(DownloadIssue::storage, "Cannot flush update download");
    }
};
bool http(const std::string &initial, const std::string &accept, uint64_t maximum,
          const std::atomic<bool> &cancel, const std::function<void(const char *, size_t)> &consume,
          uint64_t offset = 0, uint64_t packageSize = 0) {
    Internet session(WinHttpOpen(L"CyCode-updates/"
                                 L"1",
                                 WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0));
    if (!WinHttpSetTimeouts(session.value, 5000, 5000, 5000, 10000))
        throw DownloadError(DownloadIssue::network, "Cannot set update connection timeouts");
    std::string url = initial;
    const auto credential = token();
    const auto deadline =
        std::chrono::steady_clock::now() +
        (accept == "application/octet-stream" ? std::chrono::seconds(300) : std::chrono::seconds(20));
    for (unsigned redirect = 0; redirect < 6; ++redirect) {
        if (cancel)
            throw std::runtime_error("Update cancelled");
        if (std::chrono::steady_clock::now() > deadline)
            throw DownloadError(DownloadIssue::network, "Update connection timed out");
        const auto address = wide(url);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength =
            parts.dwPasswordLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(address.c_str(), static_cast<DWORD>(address.size()), 0, &parts) ||
            parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 || parts.dwUserNameLength ||
            parts.dwPasswordLength)
            throw DownloadError(DownloadIssue::response, "Untrusted update URL");
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        if (host != L"api.github.com" && host != L"release-assets.githubusercontent.com" &&
            host != L"objects.githubusercontent.com" && host != L"github.com")
            throw DownloadError(DownloadIssue::response, "Untrusted update host");
        if (!redirect && !url.starts_with(apiRoot()))
            throw DownloadError(DownloadIssue::response, "Untrusted GitHub API endpoint");
        const auto resource =
            std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) +
            (parts.dwExtraInfoLength ? std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength) : L"");
        Internet connection(WinHttpConnect(session.value, host.c_str(), 443, 0));
        Internet request(WinHttpOpenRequest(connection.value, L"GET", resource.c_str(), nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE));
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER,
              disabled = WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) ||
            !WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))
            throw DownloadError(DownloadIssue::network, "Cannot secure update connection");
        std::wstring headers = L"Accept: " + wide(accept) + L"\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        if (packageSize) {
            headers += L"Accept-Encoding: identity\r\n";
            if (offset)
                headers += L"Range: bytes=" + std::to_wstring(offset) + L"-\r\n";
        }
        // Credentials never follow asset redirects, enter argv, logs or saved settings.
        if (host == L"api.github.com" && url.starts_with(apiRoot() + "/") && !credential.empty())
            headers += L"Authorization: Bearer " + wide(credential) + L"\r\n";
        if (!WinHttpSendRequest(request.value, headers.c_str(), static_cast<DWORD>(headers.size()),
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.value, nullptr))
            throw DownloadError(DownloadIssue::network, "GitHub update connection failed");
        DWORD status = 0, length = sizeof(status);
        if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX))
            throw DownloadError(DownloadIssue::response, "Invalid GitHub update response");
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            DWORD bytes = 0;
            WinHttpQueryHeaders(request.value, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, nullptr,
                                &bytes, WINHTTP_NO_HEADER_INDEX);
            if (!bytes || bytes > 32768)
                throw DownloadError(DownloadIssue::response, "Invalid update redirect");
            std::wstring location(bytes / sizeof(wchar_t), 0);
            if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                     location.data(), &bytes, WINHTTP_NO_HEADER_INDEX))
                throw DownloadError(DownloadIssue::response, "Invalid update redirect");
            location.resize(wcslen(location.c_str()));
            url = pathUtf8(std::filesystem::path(location));
            continue;
        }
        if (status == 404)
            throw DownloadError(DownloadIssue::access,
                                "No accessible release. For a private repository, provide "
                                "CYCODE_UPDATE_TOKEN with repository Contents read access.");
        if (status == 401 || status == 403 || status == 429)
            throw DownloadError(
                DownloadIssue::access,
                "GitHub access denied or rate limited. Check update credentials or retry later.");
        if (packageSize) {
            if (!detail::downloadResponse(status, header(request.value, WINHTTP_QUERY_CONTENT_RANGE),
                                          header(request.value, WINHTTP_QUERY_CONTENT_ENCODING),
                                          header(request.value, WINHTTP_QUERY_CONTENT_LENGTH), offset,
                                          packageSize))
                return false;
        } else if (status != 200)
            throw DownloadError(DownloadIssue::response,
                                "GitHub update request failed (HTTP " + std::to_string(status) + ")");
        uint64_t total = 0;
        std::array<char, 65536> buffer{};
        while (true) {
            if (cancel)
                throw std::runtime_error("Update cancelled");
            if (std::chrono::steady_clock::now() > deadline)
                throw DownloadError(DownloadIssue::network, "Update connection timed out");
            DWORD count = 0;
            if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count))
                throw DownloadError(DownloadIssue::network, "Update download interrupted");
            if (!count)
                return true;
            total += count;
            if (total > maximum)
                throw DownloadError(DownloadIssue::response, "Update exceeds its size limit");
            consume(buffer.data(), count);
        }
    }
    throw DownloadError(DownloadIssue::response, "Too many update redirects");
}
#endif
} // namespace
std::optional<Release> check(const std::atomic<bool> &cancel, bool installed) {
#ifdef _WIN32
    std::string body;
    http(apiRoot() + "/latest", "application/vnd.github+json", 4 * 1024 * 1024, cancel,
         [&](const char *data, size_t size) { body.append(data, size); });
    return parseRelease(nlohmann::json::parse(body), CYCODE_VERSION, platform(), installed);
#else
    (void)cancel;
    throw std::runtime_error("Automatic application updates currently require Windows x64; open GitHub "
                             "Releases for this platform.");
#endif
}
static Prepared performDownload(const Release &release, const std::filesystem::path &installation,
                                const std::atomic<bool> &cancel, std::atomic<uint64_t> &progress,
                                DownloadMode mode, const std::shared_ptr<DownloadStatus> &status) {
#ifdef _WIN32
    namespace fs = std::filesystem;
    detail::downloadBinding(release);
    const auto root = fs::absolute(installation).lexically_normal();
    if (!packageArchiveName(release.asset.name, release.version, platform(), installedInstallation(root)))
        throw DownloadError(DownloadIssue::installation, "Update package differs from the installation edition");
    if (!portableInstallation(root))
        throw DownloadError(DownloadIssue::installation,
                            "Install an update-enabled portable distribution before updating in place");
    auto instances = installationUse(root);
    const auto manifest = nlohmann::json::parse(detail::read(root / "manifest.json", 2 * 1024 * 1024));
    const auto managed = detail::managedManifest(manifest);
    uint64_t helperBytes = 0;
    for (const auto &[name, file] : managed)
        if (name == "CyCodeUpdater.exe" || (name.find('/') == std::string::npos && name.ends_with(".dll"))) {
            if (file.size > 64 * 1024 * 1024 || helperBytes > maximumPackage - file.size)
                throw std::runtime_error("Update helper runtime exceeds its size limit");
            helperBytes += file.size;
        }
    if (!newer(release.version, CYCODE_VERSION) ||
        !newer(release.version, manifest.at("version").get<std::string>()))
        throw DownloadError(DownloadIssue::installation,
                            "The requested update is no longer newer than this installation");
    auto cache = detail::cacheLease(root, NativeFileLeaseMode::shared);
    const auto lockPath = root / "data/updates/download.lock";
    detail::safe(lockPath);
    NativeFileLease downloadLease(lockPath, NativeFileLeaseMode::exclusive);
    if (!downloadLease.acquired())
        throw DownloadError(DownloadIssue::busy,
                            "Another instance is downloading an update; retry after it finishes");
    if (cancel)
        throw std::runtime_error("Update cancelled");
    const auto previous = detail::selectPartial(root, release, mode, cancel);
    const auto helper = managed.find("CyCodeUpdater.exe");
    if (helper == managed.end() || !helper->second.size || helper->second.size > 64 * 1024 * 1024)
        throw DownloadError(DownloadIssue::installation, "Invalid installed update helper size");
    const auto initialOffset = previous ? previous->received : uint64_t{0};
    progress = initialOffset;
    if (status)
        status->resumed = initialOffset;
    detail::requireDownloadSpace(root / "data/updates", release.asset.size, initialOffset,
                                 helperBytes, status);
    auto directory = previous ? previous->directory : detail::work(root, "download-");
    if (!previous)
        detail::createPartialMetadata(root, directory, release);
    auto offset = previous ? previous->received : uint64_t{0};
    progress = offset;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        const auto archive = directory / release.asset.name;
        if (!fs::exists(archive) && offset < release.asset.size) {
            bool accepted = false;
            {
                PartialFile out(directory / "download.partial", offset);
                if (status)
                    status->phase = DownloadPhase::receiving;
                accepted = http(
                    release.asset.url, "application/octet-stream", release.asset.size - offset, cancel,
                    [&](const char *data, size_t size) {
                        if (status)
                            status->networkBytes += size;
                        out.write(data, size, release.asset.size);
                        progress = out.received;
                    },
                    offset, release.asset.size);
                out.flush();
            }
            if (!accepted) {
                // Never append a full 200 response to existing bytes, or truncate
                // that file. Retain it and use a new owned directory for fallback.
                if (status) {
                    status->fullFallback = true;
                    status->phase = DownloadPhase::preparing;
                }
                detail::requireDownloadSpace(root / "data/updates", release.asset.size, 0,
                                             helperBytes, status);
                directory = detail::work(root, "download-");
                detail::createPartialMetadata(root, directory, release);
                offset = 0;
                progress = 0;
                continue;
            }
        }
        if (cancel)
            throw std::runtime_error("Update cancelled");
        const auto observed = detail::partialMetadata(root, directory);
        if (!detail::sameDownload(observed, release) || observed.received != release.asset.size)
            throw DownloadError(DownloadIssue::network,
                                "Update download interrupted; received bytes were retained for retry");
        const auto payload = fs::exists(archive) ? archive : directory / "download.partial";
        const auto marker = directory / "resume.json";
        const auto metadata = detail::read(marker, 8192);
        if (status)
            status->phase = DownloadPhase::verifying;
        if (detail::hashFile(payload, release.asset.size, &cancel) != release.asset.digest) {
            // Keep corrupt bytes for inspection, but never automatically reuse them.
            rejectCheckpoint(marker, metadata);
            throw DownloadError(DownloadIssue::integrity,
                                "Update SHA-256 verification failed; retained archive excluded from retry");
        }
        if (status)
            status->phase = DownloadPhase::staging;
        detail::requireDownloadSpace(root / "data/updates", release.asset.size, release.asset.size,
                                     helperBytes, status);
        if (payload != archive)
            detail::moveFile(payload, archive);
        Prepared result{archive, directory / "CyCodeUpdater.exe", root, release.version,
                        release.asset.digest};
        if (helper == managed.end() || !helper->second.size || helper->second.size > 64 * 1024 * 1024 ||
            !fs::is_regular_file(root / "CyCodeUpdater.exe") ||
            fs::hard_link_count(root / "CyCodeUpdater.exe") != 1 ||
            fs::file_size(root / "CyCodeUpdater.exe") != helper->second.size ||
            detail::hashFile(root / "CyCodeUpdater.exe", 64 * 1024 * 1024, &cancel) != helper->second.digest)
            throw DownloadError(DownloadIssue::installation,
                                "Installed update helper changed; reinstall the application");
        detail::safe(result.helper);
        if (!fs::exists(result.helper))
            fs::copy_file(root / "CyCodeUpdater.exe", result.helper);
        if (!fs::is_regular_file(result.helper) || fs::hard_link_count(result.helper) != 1 ||
            fs::file_size(result.helper) != helper->second.size ||
            detail::hashFile(result.helper, 64 * 1024 * 1024, &cancel) != helper->second.digest) {
            rejectCheckpoint(marker, metadata);
            throw DownloadError(DownloadIssue::integrity,
                                "Staged update helper changed; retained and excluded from retry");
        }
        for (const auto &[name, file] : managed)
            if (name.find('/') == std::string::npos && name.ends_with(".dll")) {
                const auto source = root / utf8Path(name), target = directory / utf8Path(name);
                detail::safe(source); detail::safe(target);
                if (detail::hashFile(source, 64 * 1024 * 1024, &cancel) != file.digest)
                    throw std::runtime_error("Installed update runtime changed");
                if (!fs::exists(target)) fs::copy_file(source, target);
                if (!fs::is_regular_file(target) || fs::hard_link_count(target) != 1 || fs::file_size(target) != file.size ||
                    detail::hashFile(target, 64 * 1024 * 1024, &cancel) != file.digest)
                    throw std::runtime_error("Staged update runtime changed");
            }
        const auto messages = directory / "updater-messages.json";
        if (!fs::exists(messages)) {
            try {
                const auto catalogue =
                    nlohmann::json::parse(detail::read(root / "app/locales/ui.json", 4 * 1024 * 1024));
                detail::jsonFile(messages, catalogue);
            } catch (...) {
                // The catalogue is optional; installation and archive guards are not.
            }
        }
        if (cancel)
            throw std::runtime_error("Update cancelled");
        if (detail::read(marker, 8192) != metadata)
            throw DownloadError(DownloadIssue::storage, "Partial update metadata changed during staging");
        nlohmann::json files = nlohmann::json::object();
        for (const auto &file : {result.package, result.helper, messages}) {
            detail::safe(file);
            if (fs::exists(file)) {
                if (!fs::is_regular_file(file) || fs::hard_link_count(file) != 1)
                    throw DownloadError(DownloadIssue::storage, "Unsafe staged update file");
                files[pathUtf8(file.filename())] = {
                    {"size", fs::file_size(file)},
                    {"sha256", file == result.package  ? result.digest
                               : file == result.helper ? helper->second.digest
                                                       : detail::hashFile(file, 4 * 1024 * 1024, &cancel)}};
            }
        }
        detail::jsonFile(directory / "prepared.json", {{"format", 2},
                                                       {"files", files},
                                                       {"version", result.version},
                                                       {"sha256", result.digest},
                                                       {"installation", pathUtf8(root)},
                                                       {"package", release.asset.name}});
        verifyPrepared(result, cancel);
        // Only our unchanged metadata is disposable. Failed cleanup merely leaves
        // an extra protected record; it never invalidates a verified archive.
        try {
            if (detail::read(marker, 8192) == metadata) {
                detail::safe(marker);
                if (fs::is_regular_file(marker) && fs::hard_link_count(marker) == 1) {
                    std::error_code error;
                    fs::remove(marker, error);
                }
            }
        } catch (...) {
        }
        progress = release.asset.size;
        return result;
    }
    throw DownloadError(DownloadIssue::response,
                        "Update server did not provide a usable download; partial files were retained");
#else
    (void)release;
    (void)installation;
    (void)cancel;
    (void)progress;
    (void)mode;
    (void)status;
    throw std::runtime_error("Native application update installation currently requires Windows x64");
#endif
}
Prepared download(const Release &release, const std::filesystem::path &installation,
                  const std::atomic<bool> &cancel, std::atomic<uint64_t> &progress, DownloadMode mode,
                  const std::shared_ptr<DownloadStatus> &status) {
    if (status) {
        status->phase = DownloadPhase::preparing;
        status->resumed = status->networkBytes = status->requiredBytes = status->availableBytes = 0;
        status->spaceKnown = status->fullFallback = false;
    }
    try {
        if (status && (status->totalBytes != release.asset.size || status->version != release.version))
            throw DownloadError(DownloadIssue::installation,
                                "Update progress context differs from the requested release");
        auto result = performDownload(release, installation, cancel, progress, mode, status);
        if (status)
            status->phase = DownloadPhase::ready;
        return result;
    } catch (...) {
        if (status)
            status->phase = cancel.load() ? DownloadPhase::cancelled : DownloadPhase::failed;
        throw;
    }
}
void launch(const Prepared &update) {
#ifdef _WIN32
    auto cache = detail::cacheLease(update.installation, NativeFileLeaseMode::shared);
    const std::atomic<bool> cancel{false};
    verifyPrepared(update, cancel);
    auto quote = [](const std::filesystem::path &path) {
        auto value = path.wstring();
        if (value.find(L'"') != value.npos)
            throw std::runtime_error("Invalid update path");
        return L"\"" + value + L"\"";
    };
    std::wstring command = quote(update.helper) + L" --package " + quote(update.package) +
                           L" --installation " + quote(update.installation) + L" --sha256 " +
                           wide(update.digest) + L" --version " + wide(update.version);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(update.helper.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, update.helper.parent_path().c_str(), &startup, &process))
        throw std::runtime_error("Cannot start the update helper");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    (void)update;
    throw std::runtime_error("Native application update installation currently requires Windows x64");
#endif
}
} // namespace cycode::updates
