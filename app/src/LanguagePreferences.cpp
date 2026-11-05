#include "cycode/LanguagePreferences.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include "cycode/RustAnalysis.h"
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace cycode {
using Json = nlohmann::json;
namespace {
constexpr uint64_t maxRevision = 9007199254740991ULL;
bool validBackend(const std::string &key) {
    return !key.empty() && key.size() <= 160 && std::all_of(key.begin(), key.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
               c == '_' || c == '-';
    });
}
void argument(const Json &value) {
    if (!value.is_string() || value.get_ref<const std::string &>().size() > 8192 ||
        value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid language server argument");
}
// Keep the lock file in place: unlinking it could give another writer a different inode.
class PreferenceLock {
  public:
    explicit PreferenceLock(const std::filesystem::path &path) {
#ifdef _WIN32
        handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot open language preferences lock");
        OVERLAPPED overlap{};
        if (!LockFileEx(handle_, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlap)) {
            CloseHandle(handle_);
            throw std::runtime_error(
                "Language preferences are being saved by another CyCode instance; retry");
        }
#else
        handle_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (handle_ < 0)
            throw std::runtime_error("Cannot open language preferences lock");
        if (::flock(handle_, LOCK_EX | LOCK_NB) != 0) {
            ::close(handle_);
            throw std::runtime_error(
                "Language preferences are being saved by another CyCode instance; retry");
        }
#endif
    }
    ~PreferenceLock() {
#ifdef _WIN32
        CloseHandle(handle_);
#else
        ::close(handle_);
#endif
    }
    PreferenceLock(const PreferenceLock &) = delete;
    PreferenceLock &operator=(const PreferenceLock &) = delete;

  private:
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int handle_ = -1;
#endif
};
} // namespace

uint64_t LanguagePreferences::revisionValue(const Json &value) {
    if (!value.is_number_integer() ||
        (value.is_number_integer() && !value.is_number_unsigned() && value.get<int64_t>() < 0) ||
        value.get<uint64_t>() > maxRevision)
        throw std::runtime_error("Invalid language preferences revision");
    return value.get<uint64_t>();
}
void LanguagePreferences::validateConfiguration(const Json &configuration) {
    if (!configuration.is_object() || !configuration.contains("enabled") ||
        !configuration.at("enabled").is_boolean() || !configuration.contains("arguments") ||
        !configuration.at("arguments").is_array() || configuration.at("arguments").size() > 128 ||
        !configuration.contains("settings") || !configuration.at("settings").is_object() ||
        configuration.at("settings").dump().size() > 65536)
        throw std::runtime_error("Invalid language server configuration");
    for (const auto &[key, value] : configuration.items())
        if (key != "enabled" && key != "arguments" && key != "settings" && key != "executable" && key != "cargoProject")
            throw std::runtime_error("Unknown language server configuration field");
    if(configuration.contains("cargoProject"))(void)normalizeRustAnalysisProfile(configuration.at("cargoProject"));
    for (const auto &value : configuration.at("arguments"))
        argument(value);
    if (!configuration.at("arguments").empty() && configuration.at("arguments")[0] == "")
        throw std::runtime_error("Language server executable cannot be empty");
    if (configuration.contains("executable")) {
        argument(configuration.at("executable"));
        if (!configuration.at("arguments").empty() || !configuration.at("settings").empty())
            throw std::runtime_error("Use executable or arguments/settings, not both");
    }
    const auto &settings = configuration.at("settings");
    if (settings.contains("$workspaceReadyNotification")) {
        const auto &value = settings.at("$workspaceReadyNotification");
        if (!value.is_string() ||
            (value != "" && value != "experimental/serverStatus" && value != "language/status"))
            throw std::runtime_error("Unsupported language server readiness notification");
    }
}
LanguagePreferences::LanguagePreferences(const std::filesystem::path &workspace,
                                         const std::filesystem::path &directory) {
    const auto canonical = std::filesystem::canonical(workspace);
#ifdef _WIN32
    const auto &wide = canonical.native();
    const auto count = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), int(wide.size()),
                                     nullptr, 0, nullptr, nullptr, 0);
    if (!count)
        throw std::runtime_error("Cannot identify workspace language preferences");
    std::wstring folded(count, L'\0');
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), int(wide.size()), folded.data(),
                       count, nullptr, nullptr, 0))
        throw std::runtime_error("Cannot identify workspace language preferences");
    workspace_ = pathUtf8(std::filesystem::path(folded));
#else
    workspace_ = pathUtf8(canonical);
#endif
    if (!directory.empty()) {
        if (!directory.is_absolute())
            throw std::runtime_error("Language preferences require an application-owned absolute directory");
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : workspace_) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        path_ = directory / (std::to_string(hash) + ".json");
    }
    read();
}
void LanguagePreferences::read() {
    baseline_.reset();
    state_ = {{"available", !path_.empty()},
              {"revision", uint64_t(0)},
              {"profiles", Json::object()},
              {"error", ""}};
    if (path_.empty())
        return;
    try {
        if (!std::filesystem::exists(path_))
            return;
        baseline_ = readTextFileBytes(path_);
        const auto saved = Json::parse(*baseline_);
        if (!saved.is_object() || saved.value("schemaVersion", 0) != 1 ||
            saved.at("workspace") != workspace_ || !saved.at("revision").is_number_unsigned() ||
            saved.at("revision").get<uint64_t>() == 0 || saved.at("revision").get<uint64_t>() > maxRevision ||
            !saved.at("profiles").is_object() || saved.at("profiles").size() > 42)
            throw std::runtime_error("Invalid or mismatched language preferences");
        for (const auto &[key, configuration] : saved.at("profiles").items()) {
            if (!validBackend(key))
                throw std::runtime_error("Invalid saved language backend");
            validateConfiguration(configuration);
            if(key!="rust"&&configuration.contains("cargoProject"))throw std::runtime_error("Cargo analysis profile belongs to Rust");
        }
        state_["revision"] = saved.at("revision");
        state_["profiles"] = saved.at("profiles");
    } catch (const std::exception &e) {
        state_["error"] = e.what();
    }
}
Json LanguagePreferences::snapshot(bool refresh) {
    std::lock_guard lock(mutex_);
    if (refresh)
        read();
    return state_;
}
Json LanguagePreferences::status() {
    std::lock_guard lock(mutex_);
    Json backends = Json::object();
    for (const auto &[key, profile] : state_.at("profiles").items())
        backends[key] = profile.at("enabled");
    return {{"available", state_.at("available")},
            {"revision", state_.at("revision")},
            {"error", state_.at("error")},
            {"backends", std::move(backends)}};
}
void LanguagePreferences::update(const std::string &backend, const std::optional<Json> &configuration,
                                 uint64_t expectedRevision) {
    if (!validBackend(backend))
        throw std::runtime_error("Invalid language backend");
    if (configuration)
        validateConfiguration(*configuration);
    if(configuration&&backend!="rust"&&configuration->contains("cargoProject"))throw std::runtime_error("Cargo analysis profile belongs to Rust");
    std::lock_guard lock(mutex_);
    if (path_.empty())
        throw std::runtime_error("Language preferences are not enabled in this host");
    std::filesystem::create_directories(path_.parent_path());
    auto lockPath = path_;
    lockPath += ".lock";
    PreferenceLock fileLock(lockPath);
    read();
    if (!state_.at("error").get_ref<const std::string &>().empty())
        throw std::runtime_error("Cannot overwrite invalid language preferences: " +
                                 state_.at("error").get<std::string>());
    const auto revision = state_.at("revision").get<uint64_t>();
    if (revision != expectedRevision)
        throw std::runtime_error("Language preferences changed in another instance; reload before saving");
    if (revision == maxRevision)
        throw std::runtime_error("Language preferences revision limit reached");
    auto profiles = state_.at("profiles");
    if (configuration)
        profiles[backend] = *configuration;
    else
        profiles.erase(backend);
    if (profiles.size() > 42)
        throw std::runtime_error("At most 42 language profiles are supported per workspace");
    Json saved = {
        {"schemaVersion", 1}, {"workspace", workspace_}, {"revision", revision + 1}, {"profiles", profiles}};
    const auto bytes = saved.dump(2) + "\n";
    if (bytes.size() > maxTextFileBytes)
        throw std::runtime_error("Language preferences exceed 4 MiB");
    writeTextFileAtomic(path_, bytes, baseline_);
    baseline_ = bytes;
    state_["revision"] = revision + 1;
    state_["profiles"] = std::move(profiles);
}
} // namespace cycode
