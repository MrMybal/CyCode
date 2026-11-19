#include "Internal.h"
#include <cycode/Tools.h>
#include <cycode/Paths.h>
#include <cymcp/http.hpp>
#include <iostream>
#include <thread>

namespace cycode::automation {
Json downloadArtifact(const Json& request) {
    const auto url = request.at("url").get<std::string>();
    const auto hash = request.at("sha256").get<std::string>();
    const auto size = request.at("size").get<std::uint64_t>();
    const auto target = utf8Path(request.at("destination").get<std::string>());
    if (cymcp::parse_url(url).scheme != "https" || !target.is_absolute() || size == 0 ||
        size > 16ULL * 1024 * 1024 * 1024 || !std::regex_match(hash, std::regex("[a-f0-9]{64}")))
        throw cymcp::Error("InvalidParameters", "A trusted HTTPS artifact, exact size/hash and absolute destination are required");
    if (std::filesystem::is_symlink(target)) throw cymcp::Error("AccessDenied", "Redirected artifact destination");
    std::filesystem::create_directories(target.parent_path());
    if (std::filesystem::exists(target)) {
        if (std::filesystem::file_size(target) == size && cymcp::sha256_file(target) == hash)
            return {{"state", "Completed"}, {"path", pathUtf8(target)}, {"cached", true}};
        throw cymcp::Error("IntegrityFailure", "A changed cached artifact is preserved; choose a new destination");
    }
    auto partial = target; partial += ".part";
    auto metadata = target; metadata += ".download.json";
    const Json identity{{"url", url}, {"sha256", hash}, {"size", size}};
    std::uint64_t offset = 0;
    if (std::filesystem::exists(partial)) {
        if (!std::filesystem::exists(metadata) || readJson(metadata) != identity || std::filesystem::is_symlink(partial))
            throw cymcp::Error("IntegrityFailure", "Incompatible partial download is preserved");
        offset = std::filesystem::file_size(partial);
        if (offset > size) throw cymcp::Error("IntegrityFailure", "Partial artifact exceeds its expected size");
    }
    saveJson(metadata, identity);
    if (std::filesystem::space(target.parent_path()).available < size - offset)
        throw cymcp::Error("CapacityExceeded", "Not enough disk space for the trusted artifact");
    // Commit only complete, validated range responses. An ignored range, error,
    // timeout or cancellation cannot append unverified bytes to the valid prefix.
    cymcp::Cancellation cancellation;
    const auto cancelPath = request.value("cancelFile", "");
    std::jthread monitor;
    if (!cancelPath.empty()) monitor = std::jthread([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::error_code error;
            const bool requested = std::filesystem::exists(utf8Path(cancelPath), error);
            if (requested || error) { cancellation.cancel(); return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    std::string effectiveUrl = url;
    while (offset < size) {
        const auto count = (std::min)(size - offset, std::uint64_t(4 * 1024 * 1024));
        const auto end = offset + count - 1;
        if (!cancelPath.empty() && std::filesystem::exists(utf8Path(cancelPath)))
            throw cymcp::Error("Cancelled", "Download cancelled; the last complete range is retained");
        cymcp::HttpRequest download;
        download.url = effectiveUrl; download.timeout = std::chrono::milliseconds(0);
        download.idle_timeout = std::chrono::seconds(30);
        download.max_response_bytes = static_cast<std::size_t>(count);
        download.headers["Accept-Encoding"] = "identity";
        download.headers["Range"] = "bytes=" + std::to_string(offset) + "-" + std::to_string(end);
        cymcp::HttpResponse response;
        for (unsigned redirects = 0; ; ++redirects) {
            response = cymcp::HttpClient().perform(download, cancellation);
            if (response.status != 301 && response.status != 302 && response.status != 303 && response.status != 307 && response.status != 308) break;
            const auto location = response.headers.find("location");
            if (redirects >= 5 || location == response.headers.end() || location->second.size() > 16384)
                throw cymcp::Error("DownloadFailed", "Artifact redirect chain is invalid; partial prefix retained");
            const auto origin = cymcp::parse_url(url), next = cymcp::parse_url(location->second);
            const bool githubAsset = origin.host == "github.com" &&
                (next.host == "github.com" || next.host == "release-assets.githubusercontent.com" || next.host == "objects.githubusercontent.com");
            if (next.scheme != "https" || next.loopback || (!githubAsset && next.host != origin.host))
                throw cymcp::Error("AccessDenied", "Artifact redirect is outside its trusted HTTPS origin/CDN");
            download.url = location->second; // No credentials/body are forwarded.
        }
        effectiveUrl = download.url; // Signed URLs remain in memory only, never in logs/metadata.
        const auto expectedRange = "bytes " + std::to_string(offset) + "-" + std::to_string(end) + "/" + std::to_string(size);
        const auto range = response.headers.find("content-range");
        const bool full = offset == 0 && count == size && response.status == 200;
        if ((!full && (response.status != 206 || range == response.headers.end() || range->second != expectedRange)) || response.body.size() != count)
            throw cymcp::Error("DownloadFailed", "Server did not honor the bounded artifact range; the valid partial prefix is retained");
        if (std::filesystem::exists(partial) && std::filesystem::file_size(partial) != offset)
            throw cymcp::Error("Conflict", "Partial artifact changed during download");
        if (!std::filesystem::exists(partial)) {
            AtomicFileOptions creation; creation.access = AtomicAccess::PrivateUser;
            writeFileAtomic(partial, "", creation);
        }
        std::ofstream append(partial, std::ios::binary | std::ios::app);
        append.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
        append.flush();
        if (!append) throw cymcp::Error("FileError", "Cannot append the verified range; resume from the retained prefix");
        append.close();
        offset += count;
        std::cout << Json({{"event", "progress"}, {"value", {{"phase", "download"},
            {"downloaded", offset}, {"total", size}}}}).dump() << '\n' << std::flush;
    }
    if (std::filesystem::file_size(partial) != size || cymcp::sha256_file(partial) != hash)
        throw cymcp::Error("IntegrityFailure", "Artifact size/SHA-256 mismatch; no final file was published");
    std::filesystem::rename(partial, target);
    std::filesystem::remove(metadata);
    return {{"state", "Completed"}, {"path", pathUtf8(target)}, {"sha256Verified", true}};
}
} // namespace cycode::automation
