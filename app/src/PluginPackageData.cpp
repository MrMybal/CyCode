#include "PluginPackageData.h"
#include "cycode/Paths.h"
#include "cycode/PluginManifest.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <monocypher-ed25519.h>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace cycode::package_data {
namespace {
std::string encode(const uint8_t *bytes, size_t count) {
    std::string result;
    result.reserve(count * 2);
    constexpr auto digits = "0123456789abcdef";
    for (size_t i = 0; i < count; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}
void regular(const fs::path &path) {
    const auto status = fs::symlink_status(path);
    if (redirected(path) || !fs::is_regular_file(status))
        throw std::runtime_error("Package files must be regular files without symbolic links");
}
void validateIndex(const Json &p) {
    fields(p, {"schemaVersion", "entries"});
    if (p.at("schemaVersion") != 1 || !p.at("entries").is_object() || p.at("entries").size() > 64)
        throw std::runtime_error("Invalid managed plugin index");
    for (const auto &[id, entry] : p.at("entries").items()) {
        fields(entry, {"current", "history"});
        if (!packageId(id) || !hex(entry.at("current").get<std::string>(), 64) ||
            !entry.at("history").is_array() || entry.at("history").size() > 4)
            throw std::runtime_error("Invalid managed plugin selection");
        std::set<std::string> seen{entry.at("current").get<std::string>()};
        for (const auto &item : entry.at("history"))
            if (!item.is_string() || !hex(item.get<std::string>(), 64) ||
                !seen.insert(item.get<std::string>()).second)
                throw std::runtime_error("Invalid plugin rollback history");
    }
}
void validateTrust(const Json &p) {
    fields(p, {"schemaVersion", "publishers"});
    if (p.at("schemaVersion") != 1 || !p.at("publishers").is_object() || p.at("publishers").size() > 128)
        throw std::runtime_error("Invalid plugin publisher store");
    for (const auto &[key, value] : p.at("publishers").items()) {
        fields(value, {"label", "enabled"});
        if (!hex(key, 32) || !value.at("enabled").is_boolean() || !value.at("label").is_string() ||
            value.at("label").get_ref<const std::string &>().empty() ||
            value.at("label").get_ref<const std::string &>().size() > 256 ||
            value.at("label").get_ref<const std::string &>().find('\0') != std::string::npos)
            throw std::runtime_error("Invalid plugin publisher identity");
    }
}
} // namespace
std::string sha512(const std::string &bytes) {
    std::array<uint8_t, 64> digest{};
    crypto_sha512(digest.data(), reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    return encode(digest.data(), digest.size());
}
bool hex(const std::string &text, size_t bytes) {
    return text.size() == bytes * 2 && std::all_of(text.begin(), text.end(), [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
std::vector<uint8_t> unhex(const std::string &text, size_t bytes) {
    if (!hex(text, bytes))
        throw std::runtime_error("Invalid canonical hexadecimal package value");
    std::vector<uint8_t> result(bytes);
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (size_t i = 0; i < bytes; ++i)
        result[i] = uint8_t(digit(text[i * 2]) * 16 + digit(text[i * 2 + 1]));
    return result;
}
bool portablePath(const std::string &text) {
    if (text.empty() || text.size() > 1024 || text.find('\0') != std::string::npos ||
        text.find_first_of("\\:\r\n\t<>\"|?*") != std::string::npos)
        return false;
    const auto path = utf8Path(text);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory() || text.back() == '/')
        return false;
    std::string normalized;
    size_t depth = 0;
    for (const auto &item : path) {
        auto part = pathUtf8(item);
        if (++depth > 32 || part.empty() || part == "." || part == ".." || part.size() > 160 ||
            part.back() == '.' || part.back() == ' ')
            return false;
        for (unsigned char c : part)
            if (c < 32 || c == 127)
                return false;
        std::transform(part.begin(), part.end(), part.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        const auto stem = part.substr(0, part.find('.'));
        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" || stem == "conin$" ||
            stem == "conout$" ||
            (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' &&
             stem[3] <= '9'))
            return false;
        if (!normalized.empty())
            normalized += '/';
        normalized += pathUtf8(item);
    }
    return normalized == text;
}
bool packageId(const std::string &text) {
    return text.size() <= 100 && portablePath(text) && text.find('/') == std::string::npos &&
           text.front() >= 'a' && text.front() <= 'z' &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
           });
}
bool redirected(const fs::path &path) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(path, error)))
        return true;
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return false;
#endif
}
void directory(const fs::path &path) {
    std::error_code error;
    auto status = fs::symlink_status(path, error);
    if (redirected(path))
        throw std::runtime_error("Managed plugin directories must not be symbolic links");
    if (!fs::exists(status)) {
        fs::create_directory(path);
#ifndef _WIN32
        fs::permissions(path, fs::perms::owner_all);
#endif
        status = fs::symlink_status(path);
    }
    if (!fs::is_directory(status))
        throw std::runtime_error("Managed plugin directory is unavailable");
}
fs::path join(const fs::path &root, const std::string &name) {
    if (!portablePath(name))
        throw std::runtime_error("Invalid portable package path");
    fs::path result = root;
    if (redirected(root))
        throw std::runtime_error("Package roots must not be redirected");
    for (const auto &part : utf8Path(name)) {
        result /= part;
        std::error_code error;
        if (redirected(result))
            throw std::runtime_error("Symbolic links are not allowed inside plugin packages");
    }
    return result;
}
std::string read(const fs::path &path, size_t maximum) {
    regular(path);
    const auto size = fs::file_size(path);
    if (size > maximum)
        throw std::runtime_error("Package metadata exceeds its byte limit");
    std::ifstream input(path, std::ios::binary);
    std::string bytes(size, '\0');
    if (!input.read(bytes.data(), std::streamsize(size)) || input.peek() != EOF)
        throw std::runtime_error("Package metadata changed or cannot be read");
    return bytes;
}
Json parse(const std::string &bytes) {
    std::vector<std::set<std::string>> keys;
    return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json &value) {
        if (depth > 16)
            throw std::runtime_error("Package JSON nesting exceeds its limit");
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("Duplicate package JSON key");
        if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    });
}
void fields(const Json &p, std::initializer_list<const char *> names) {
    if (!p.is_object())
        throw std::runtime_error("Expected package JSON object");
    for (const auto &[key, value] : p.items())
        if (std::none_of(names.begin(), names.end(), [&](const char *name) { return key == name; }))
            throw std::runtime_error("Unknown package field: " + key);
}
void copyChecked(const fs::path &source, const fs::path *destination, const Json &file) {
    regular(source);
    const auto size = file.at("size").get<uint64_t>();
    if (fs::file_size(source) != size)
        throw std::runtime_error("Plugin payload size mismatch: " + file.at("path").get<std::string>());
    std::ifstream input(source, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read plugin payload");
    struct Sink {
        int descriptor = -1;
        void close() {
            if (descriptor < 0)
                return;
            const auto file = descriptor;
            descriptor = -1;
#ifdef _WIN32
            const auto status = _close(file);
#else
            const auto status = ::close(file);
#endif
            if (status)
                throw std::runtime_error("Plugin payload close failed");
        }
        ~Sink() {
            try {
                close();
            } catch (...) {
            }
        }
    } output;
    if (destination) {
#ifdef _WIN32
        output.descriptor =
            _wopen(destination->c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY | _O_NOINHERIT,
                   _S_IREAD | _S_IWRITE);
#else
        output.descriptor =
            ::open(destination->c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600);
#endif
        if (output.descriptor < 0)
            throw std::runtime_error("Cannot exclusively create plugin payload");
    }
    crypto_sha512_ctx context;
    crypto_sha512_init(&context);
    std::array<char, 65536> bytes{};
    uint64_t count = 0;
    while (input) {
        input.read(bytes.data(), bytes.size());
        const auto got = input.gcount();
        if (got > 0) {
            count += uint64_t(got);
            if (count > size)
                throw std::runtime_error("Plugin payload grew while copying");
            crypto_sha512_update(&context, reinterpret_cast<const uint8_t *>(bytes.data()), size_t(got));
            if (destination) {
                size_t written = 0;
                while (written < size_t(got)) {
#ifdef _WIN32
                    const auto amount =
                        _write(output.descriptor, bytes.data() + written, unsigned(size_t(got) - written));
#else
                    const auto amount =
                        ::write(output.descriptor, bytes.data() + written, size_t(got) - written);
#endif
                    if (amount < 0 && errno == EINTR)
                        continue;
                    if (amount <= 0)
                        throw std::runtime_error("Plugin payload write failed");
                    written += size_t(amount);
                }
            }
        }
    }
    std::array<uint8_t, 64> digest{};
    crypto_sha512_final(&context, digest.data());
    if (!input.eof() || count != size ||
        encode(digest.data(), digest.size()) != file.at("sha512").get<std::string>())
        throw std::runtime_error("Plugin payload checksum mismatch: " + file.at("path").get<std::string>());
    if (destination) {
        output.close();
#ifndef _WIN32
        auto permissions =
            fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read;
        if (file.value("executable", false))
            permissions |= fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
        fs::permissions(*destination, permissions);
#endif
    }
}
Package package(const fs::path &source, bool verifyPayload, bool installed) {
    Package result;
    result.bytes = read(join(source, "package.json"), 256 * 1024);
    result.signatureBytes = read(join(source, "signature.json"), 2048);
    result.document = parse(result.bytes);
    result.signature = parse(result.signatureBytes);
    fields(result.document, {"schemaVersion", "platform", "plugin", "files"});
    fields(result.signature, {"algorithm", "publicKey", "signature"});
    if (result.document.at("schemaVersion") != 1 || result.signature.at("algorithm") != "Ed25519")
        throw std::runtime_error("Unsupported plugin package/signature version");
    result.publicKey = result.signature.at("publicKey").get<std::string>();
    const auto key = unhex(result.publicKey, 32),
               signature = unhex(result.signature.at("signature").get<std::string>(), 64);
    const auto message = "CyCode plugin package v1\n" + result.bytes;
    if (crypto_ed25519_check(signature.data(), key.data(), reinterpret_cast<const uint8_t *>(message.data()),
                             message.size()) != 0)
        throw std::runtime_error("Plugin package signature is invalid");
    result.digest = sha512("CyCode package identity v1\n" + result.publicKey + "\n" + result.bytes);
    result.plugin = result.document.at("plugin");
    fields(result.plugin, {"id", "name", "version", "abi", "library", "permissions", "dependencies",
                           "hostVersion", "platforms", "description", "homepage", "license"});
    validatePluginManifest(result.plugin);
    if (result.plugin.value("name", std::string{}).size() > 256 ||
        result.plugin.value("permissions", Json::array()).size() > 32 ||
        result.plugin.value("dependencies", Json::array()).size() > 32)
        throw std::runtime_error("Managed package name/permission/dependency limit exceeded");
    for (const auto &dependency : result.plugin.value("dependencies", Json::array()))
        fields(dependency, {"id", "minimum", "maximumExclusive", "optional"});
    (void)pluginVersion(result.plugin.at("version").get<std::string>());
    if (!packageId(result.plugin.at("id")))
        throw std::runtime_error("Managed plugin ID must be a portable lowercase identifier");
    if (!portablePath(result.plugin.at("library")) ||
        result.plugin.at("library").get_ref<const std::string &>().find('/') != std::string::npos)
        throw std::runtime_error("Invalid managed plugin library basename");
    const auto platform = result.document.at("platform").get<std::string>();
    if (!std::set<std::string>{"windows-x64", "windows-arm64", "windows-x86", "linux-x64", "linux-arm64",
                               "linux-x86", "macos-x64", "macos-arm64"}
             .contains(platform))
        throw std::runtime_error("Invalid plugin package platform");
    const auto &files = result.document.at("files");
    if (!files.is_array() || files.empty() || files.size() > 256)
        throw std::runtime_error("Plugin package requires 1 to 256 payload files");
    std::set<std::string> names, allNames{"package.json", "signature.json", "manifest.json"},
        requiredDirectories;
    bool library = false;
    for (const auto &file : files) {
        fields(file, {"path", "size", "sha512", "executable"});
        const auto name = file.at("path").get<std::string>();
        auto folded = name;
        std::transform(folded.begin(), folded.end(), folded.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (!portablePath(name) || !allNames.insert(folded).second ||
            !hex(file.at("sha512").get<std::string>(), 64) || !file.at("size").is_number_unsigned() ||
            file.at("size") > maxPayloadFile ||
            (file.contains("executable") && !file.at("executable").is_boolean()))
            throw std::runtime_error("Invalid, duplicate or excessive package payload entry");
        names.insert(folded);
        auto parent = utf8Path(name).parent_path();
        while (!parent.empty()) {
            requiredDirectories.insert(pathUtf8(parent));
            parent = parent.parent_path();
        }
        result.totalBytes += file.at("size").get<uint64_t>();
        if (result.totalBytes > maxPayload)
            throw std::runtime_error("Plugin package exceeds 256 MiB");
        const auto base = result.plugin.at("library").get<std::string>();
        if (name == base + (platform.starts_with("windows-") ? ".dll"
                            : platform.starts_with("macos-") ? ".dylib"
                                                             : ".so") ||
            (platform.starts_with("macos-") && name == base + ".so")) {
            if (file.at("size") == 0)
                throw std::runtime_error("Plugin library is empty");
            library = true;
        }
        if (verifyPayload)
            copyChecked(join(source, name), nullptr, file);
    }
    for (const auto &name : names) {
        auto parent = utf8Path(name).parent_path();
        while (!parent.empty()) {
            if (allNames.contains(pathUtf8(parent)))
                throw std::runtime_error("Package file/directory path collision");
            parent = parent.parent_path();
        }
    }
    if (!library)
        throw std::runtime_error("Package does not contain its declared native library");
    if (installed) {
        if (read(join(source, "manifest.json"), 256 * 1024) != result.plugin.dump(2) + "\n")
            throw std::runtime_error("Installed plugin manifest differs from signed metadata");
        size_t visited = 0;
        for (auto it = fs::recursive_directory_iterator(source); it != fs::recursive_directory_iterator();
             ++it) {
            if (++visited > 4096 || it.depth() > 32 || redirected(it->path()))
                throw std::runtime_error("Invalid installed package tree");
            const auto relative = pathUtf8(it->path().lexically_relative(source));
            if (it->is_directory()) {
                if (!requiredDirectories.contains(relative))
                    throw std::runtime_error("Unexpected directory in installed plugin package");
                continue;
            }
            if (!it->is_regular_file() ||
                (!std::set<std::string>{"package.json", "signature.json", "manifest.json"}.contains(
                     relative) &&
                 std::none_of(files.begin(), files.end(),
                              [&](const Json &f) { return f.at("path") == relative; })))
                throw std::runtime_error("Unexpected file in installed plugin package");
        }
    }
    return result;
}
Json summary(const Package &p) {
    return {{"id", p.plugin.at("id")},
            {"name", p.plugin.value("name", p.plugin.at("id").get<std::string>())},
            {"version", p.plugin.value("version", "0.0.0")},
            {"platform", p.document.at("platform")},
            {"digest", p.digest},
            {"publicKey", p.publicKey},
            {"fingerprint",
             sha512(std::string(reinterpret_cast<const char *>(unhex(p.publicKey, 32).data()), 32))},
            {"permissions", p.plugin.value("permissions", Json::array())},
            {"dependencies", p.plugin.value("dependencies", Json::array())},
            {"fileCount", p.document.at("files").size()},
            {"bytes", p.totalBytes},
            {"compatibility", pluginCompatibility(p.plugin)}};
}
Store::Store(const fs::path &installation, NativeFileLeaseMode mode) {
    root = fs::canonical(installation) / "extensions-managed";
    directory(root);
    directory(root / "packages");
    directory(root / "leases");
    directory(root / ".staging");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
        lease = std::make_unique<NativeFileLease>(root / "manager.lock", mode);
        if (lease->acquired())
            break;
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Plugin package manager is busy in another instance; retry");
        lease.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (const auto *name : {"index.json", "publishers.json"}) {
        std::error_code error;
        const auto status = fs::symlink_status(root / name, error);
        if (fs::is_symlink(status))
            throw std::runtime_error("Managed plugin state must not be a symbolic link");
        if (fs::exists(status)) {
            auto bytes = read(root / name, 1024 * 1024);
            if (std::string(name) == "index.json")
                indexBytes = bytes;
            else
                trustBytes = bytes;
        }
    }
    index = indexBytes ? parse(*indexBytes) : Json{{"schemaVersion", 1}, {"entries", Json::object()}};
    trust = trustBytes ? parse(*trustBytes) : Json{{"schemaVersion", 1}, {"publishers", Json::object()}};
    validateIndex(index);
    validateTrust(trust);
}
std::string Store::revision() const {
    return indexBytes ? sha512(*indexBytes) : "none";
}
std::string Store::trustRevision() const {
    return trustBytes ? sha512(*trustBytes) : "none";
}
fs::path Store::packagePath(const std::string &id, const std::string &digest) const {
    if (!packageId(id) || !hex(digest, 64))
        throw std::runtime_error("Invalid managed package identity");
    return join(root, "packages/" + digest.substr(0, 64));
}
fs::path Store::leasePath(const std::string &digest) const {
    if (!hex(digest, 64))
        throw std::runtime_error("Invalid managed package digest");
    return root / "leases" / utf8Path(digest + ".lock");
}
bool Store::trusted(const std::string &key) const {
    return trust.at("publishers").contains(key) && trust.at("publishers").at(key).at("enabled").get<bool>();
}
void Store::saveIndex() {
    validateIndex(index);
    const auto bytes = index.dump(2) + "\n";
    writeTextFileAtomic(root / "index.json", bytes, indexBytes);
    indexBytes = bytes;
}
void Store::saveTrust() {
    validateTrust(trust);
    const auto bytes = trust.dump(2) + "\n";
    writeTextFileAtomic(root / "publishers.json", bytes, trustBytes);
    trustBytes = bytes;
}
} // namespace cycode::package_data
