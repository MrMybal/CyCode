#include "cycode/WorkspaceSession.h"
#include "cycode/DocumentReference.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <set>
namespace cycode {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
bool identifier(const std::string &s) {
    return !s.empty() && s.size() <= 180 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
               c == '_';
    });
}
std::string string(const Json &p, const char *key, size_t limit) {
    auto s = p.at(key).get<std::string>();
    if (s.size() > limit || s.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid workspace session string");
    return s;
}
bool path(const std::string &s) {
    if (isUntitledDocument(s))
        return true;
    const auto p = utf8Path(s);
    if (s.empty() || s.find('\0') != std::string::npos || p.is_absolute() || p.has_root_name() ||
        s.find_first_of("\\:\r\n") != std::string::npos)
        return false;
    for (const auto &part : p)
        if (part == ".." || part == ".")
            return false;
    return true;
}
size_t coordinate(const Json &p, const char *key, size_t limit) {
    const auto &n = p.at(key);
    if (!n.is_number_integer() || n < 0 || n > limit)
        throw std::runtime_error("Invalid workspace session coordinate");
    return n.get<size_t>();
}
Json read(const fs::path &path) {
    const auto bytes = readTextFileBytes(path);
    if (bytes.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Workspace session exceeds 2 MiB");
    return Json::parse(bytes, [](int depth, Json::parse_event_t, Json &) {
        if (depth > 12)
            throw std::runtime_error("Workspace session nesting exceeds its limit");
        return true;
    });
}
} // namespace
namespace {
void fields(const Json &value, const std::set<std::string> &names) {
    if (!value.is_object() || !value.at("version").is_number_integer() || value.at("version") != 1)
        throw std::runtime_error("Invalid workspace view version");
    for (const auto &[key, entry] : value.items())
        if (!names.contains(key))
            throw std::runtime_error("Unknown workspace view field");
}
double number(const Json &value, const char *key, double low, double high) {
    const auto &item = value.at(key);
    if (!item.is_number())
        throw std::runtime_error("Invalid workspace image coordinate");
    const auto result = item.get<double>();
    if (!std::isfinite(result) || result < low || result > high)
        throw std::runtime_error("Workspace image coordinate exceeds its bounds");
    return result;
}
} // namespace
Json validateSessionEditor(const Json &value) {
    fields(value, {"version", "wrap", "anchorLine", "folds", "foldsComplete", "fingerprint"});
    if (!value.at("wrap").is_boolean() || !value.at("foldsComplete").is_boolean())
        throw std::runtime_error("Invalid workspace editor flags");
    (void)coordinate(value, "anchorLine", 10000000);
    const auto fingerprint = string(value, "fingerprint", 128);
    const auto &folds = value.at("folds");
    if (!folds.is_array() || folds.size() > sessionMaxFolds ||
        (!folds.empty() && (!value.at("foldsComplete").get<bool>() || fingerprint.empty())))
        throw std::runtime_error("Invalid workspace fold collection");
    size_t previous = 0;
    bool first = true;
    for (const auto &entry : folds) {
        const auto line = coordinate(Json{{"line", entry}}, "line", 10000000);
        if (!first && line <= previous)
            throw std::runtime_error("Workspace fold lines must be unique and sorted");
        previous = line;
        first = false;
    }
    return value;
}
Json validateSessionImage(const Json &value) {
    fields(value, {"version", "zoom", "centerX", "centerY", "fit", "pixelated", "width", "height"});
    (void)number(value, "zoom", .001, 1000.);
    (void)number(value, "centerX", -10000000., 10000000.);
    (void)number(value, "centerY", -10000000., 10000000.);
    if (!value.at("fit").is_boolean() || !value.at("pixelated").is_boolean())
        throw std::runtime_error("Invalid workspace image flags");
    const auto width = coordinate(value, "width", 32768), height = coordinate(value, "height", 32768);
    if ((!width != !height) || (!value.at("fit").get<bool>() && !width))
        throw std::runtime_error("Invalid workspace image dimensions");
    return value;
}
Json validateSessionGit(const Json &value) {
    fields(value, {"version", "root", "identity"});
    const auto root = string(value, "root", 4096), identity = string(value, "identity", 256);
    if (root.empty() != identity.empty() || (!root.empty() && root != "." && !path(root)) ||
        isUntitledDocument(root))
        throw std::runtime_error("Invalid saved Git repository selection");
    for (const auto &part : utf8Path(root)) {
        auto name = pathUtf8(part);
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (name == ".git" || name == ".cycode")
            throw std::runtime_error("Saved Git selection crosses metadata");
    }
    return value;
}
nlohmann::json validateWorkspaceSession(const Json &value, const std::string &workspace) {
    if (!value.is_object() || value.value("schemaVersion", 0) != 1 ||
        string(value, "workspace", 32768) != workspace)
        throw std::runtime_error("Workspace session identity/version mismatch");
    if (value.contains("git"))
        (void)validateSessionGit(value.at("git"));
    const auto &tabs = value.at("tabs"), &recent = value.at("recent"), &panels = value.at("panels");
    if (!tabs.is_array() || tabs.size() > 256 || !recent.is_array() || recent.size() > 100 ||
        !panels.is_object() || panels.size() > 512)
        throw std::runtime_error("Invalid workspace session collection limit");
    std::set<std::string> seen, windows;
    size_t foldCount = 0;
    for (const auto &tab : tabs) {
        const auto file = string(tab, "path", 8192), window = string(tab, "windowKey", 8192);
        if (!path(file) || !seen.insert(file).second || (!window.empty() && !windows.insert(window).second))
            throw std::runtime_error("Invalid or duplicate workspace session tab");
        (void)coordinate(tab, "line", 10000000);
        (void)coordinate(tab, "column", 1000000);
        (void)coordinate(tab, "scroll", 10000000);
        if (tab.contains("editor") && tab.contains("image"))
            throw std::runtime_error("Workspace tab cannot be both text and image");
        if (tab.contains("editor")) {
            const auto editor = validateSessionEditor(tab.at("editor"));
            foldCount += editor.at("folds").size();
            if (foldCount > sessionMaxTotalFolds)
                throw std::runtime_error("Workspace session fold budget exceeded");
        }
        if (tab.contains("image"))
            (void)validateSessionImage(tab.at("image"));
    }
    for (const auto &item : recent)
        if (!item.is_string() || item.get_ref<const std::string &>().size() > 8192 ||
            !path(item.get<std::string>()))
            throw std::runtime_error("Invalid workspace session recent file");
    for (const auto &[key, visible] : panels.items())
        if (key.empty() || key.size() > 256 || key.find('\0') != std::string::npos || !visible.is_boolean())
            throw std::runtime_error("Invalid workspace session panel");
    const auto active = string(value, "active", 8192);
    if (!active.empty() && !seen.contains(active))
        throw std::runtime_error("Workspace session active tab is missing");
    (void)string(value, "layout", 512 * 1024);
    if (value.dump().size() > 2 * 1024 * 1024)
        throw std::runtime_error("Workspace session exceeds 2 MiB");
    return value;
}
WorkspaceSession::WorkspaceSession(fs::path directory, std::string workspace, std::string instance)
    : directory_(std::move(directory)), workspace_(std::move(workspace)), instance_(std::move(instance)) {
    if (!identifier(instance_))
        throw std::runtime_error("Invalid workspace session instance ID");
    fs::create_directories(directory_);
    directory_ = fs::canonical(directory_);
    file_ = directory_ / utf8Path(instance_ + ".json");
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(file_, error)))
        throw std::runtime_error("Workspace session record must not be a symbolic link");
    lease_ = std::make_unique<NativeFileLease>(directory_ / utf8Path(instance_ + ".lock"));
    if (!lease_->acquired())
        throw std::runtime_error("Workspace session instance is already active");
    if (fs::exists(file_))
        baseline_ = readTextFileBytes(file_);
}
std::filesystem::path WorkspaceSession::layoutPath() const {
    return directory_ / utf8Path(instance_ + ".ini");
}
nlohmann::json WorkspaceSession::latest() {
    struct Entry {
        fs::path file;
        fs::file_time_type time;
    };
    std::vector<Entry> candidates;
    size_t visited = 0;
    for (const auto &item : fs::directory_iterator(directory_)) {
        if (++visited > 16384)
            throw std::runtime_error("Workspace session directory exceeds its entry limit");
        if (item.path() == file_ || item.path().extension() != ".json" || item.is_symlink() ||
            !item.is_regular_file() || !identifier(pathUtf8(item.path().stem())))
            continue;
        candidates.push_back({item.path(), item.last_write_time()});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        return a.time != b.time ? a.time > b.time : a.file > b.file;
    });
    Json latest = nullptr;
    size_t retained = 0;
    for (const auto &candidate : candidates) {
        try {
            const auto id = pathUtf8(candidate.file.stem());
            NativeFileLease lease(directory_ / utf8Path(id + ".lock"));
            if (!lease.acquired())
                continue;
            const auto snapshot = validateWorkspaceSession(read(candidate.file), workspace_);
            if (latest.is_null())
                latest = snapshot;
            if (++retained > 32) {
                // Only validated, inactive files in this canonical owned directory.
                // Keep the lock inode; it must never be replaced under another reader.
                std::error_code error;
                fs::remove(candidate.file, error);
                fs::remove(directory_ / utf8Path(id + ".ini"), error);
            }
        } catch (const std::exception
                     &) { /* Corrupt/foreign/inaccessible snapshots cannot block valid older ones. */
        }
    }
    return latest;
}
void WorkspaceSession::save(const Json &snapshot, bool closed) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(file_, error)))
        throw std::runtime_error("Workspace session record must not be a symbolic link");
    auto value = validateWorkspaceSession(snapshot, workspace_);
    value["session"] = instance_;
    value["closed"] = closed;
    value["savedAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const auto bytes = value.dump() + "\n";
    if (bytes.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Workspace session exceeds 2 MiB");
    writeTextFileAtomic(file_, bytes, baseline_);
    baseline_ = bytes;
}
} // namespace cycode
