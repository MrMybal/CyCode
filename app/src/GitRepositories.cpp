#include "cycode/GitRepositories.h"
#include "cycode/FileTransactions.h"
#include "cycode/Paths.h"
#include "cycode/Recovery.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
bool redirected(const fs::path &p) {
    if (fs::is_symlink(fs::symlink_status(p)))
        return true;
#ifdef _WIN32
    const auto flags = GetFileAttributesW(p.c_str());
    if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT))
        return true;
#endif
    return false;
}
GitRepositorySelection exact(const fs::path &workspace, const fs::path &directory) {
    if (!containsPath(workspace, directory) || !fs::is_directory(directory) || redirected(directory))
        throw std::runtime_error("Git selection must be an ordinary workspace directory");
    GitRepositorySelection result;
    result.directory = directory;
    result.root = pathUtf8(directory.lexically_relative(workspace));
    if (result.root.empty())
        result.root = ".";
    auto identity = directoryIdentity(directory);
    const auto marker = directory / ".git";
    const auto status = fs::symlink_status(marker);
    if (redirected(marker))
        throw std::runtime_error("Redirected Git markers are not accepted");
    if (fs::is_directory(status)) {
        result.marker = true;
        identity += "\n" + directoryIdentity(marker);
    } else if (fs::is_regular_file(status)) {
        if (fs::file_size(marker) > 4096)
            throw std::runtime_error("Git pointer exceeds its limit");
        const auto pointer = readTextFileBytes(marker);
        if (!pointer.starts_with("gitdir: ") || pointer.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Git directory pointer");
        result.marker = true;
        result.linked = true;
        identity += "\n" + pointer;
    } else if (fs::exists(status))
        throw std::runtime_error("Invalid Git marker type");
    result.identity = RecoveryStore::fingerprint(identity);
    return result;
}
} // namespace
GitRepositorySelection gitRepositoryAt(const fs::path &workspace, const fs::path &directory, bool nearest) {
    auto point = fs::weakly_canonical(directory);
    if (!containsPath(workspace, point))
        throw std::runtime_error("Git selection is outside the workspace");
    while (!fs::is_directory(point) && point != workspace)
        point = point.parent_path();
    const auto requested = point;
    for (size_t depth = 0; depth < 512; ++depth) {
        auto value = exact(workspace, point);
        if (value.marker || !nearest)
            return value;
        if (point == workspace)
            break;
        const auto parent = point.parent_path();
        if (parent == point || !containsPath(workspace, parent))
            break;
        point = parent;
    }
    return exact(workspace, requested);
}
Json discoverGitRepositories(const fs::path &workspace) {
    Json entries = Json::array(), errors = Json::array();
    size_t visited = 0, inspected = 0;
    bool truncated = false;
    std::vector<std::pair<fs::path, size_t>> pending{{workspace, 0}};
    const std::set<std::string> excluded = {".git",         ".cycode",  "runtime",      "data",
                                            "node_modules", ".vs",      ".idea",        ".cache",
                                            "target",       "binaries", "intermediate", "saved"};
    while (!pending.empty() && visited < 20000 && entries.size() < 256) {
        const auto [directory, depth] = pending.back();
        pending.pop_back();
        ++visited;
        try {
            const auto selected = exact(workspace, directory);
            if (selected.marker)
                entries.push_back(
                    {{"root", selected.root}, {"identity", selected.identity}, {"linked", selected.linked}});
            std::error_code error;
            for (fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, error),
                 end;
                 !error && it != end; it.increment(error)) {
                if (++inspected > 200000) {
                    truncated = true;
                    pending.clear();
                    break;
                }
                const auto &item = *it;
                auto name = pathUtf8(item.path().filename());
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char value) { return char(std::tolower(value)); });
                if (excluded.contains(name) || redirected(item.path()))
                    continue;
                if (item.is_directory()) {
                    if (depth >= 32 || pending.size() >= 20000) {
                        truncated = true;
                        continue;
                    }
                    pending.push_back({item.path(), depth + 1});
                }
            }
            if (error && errors.size() < 16)
                errors.push_back({{"root", selected.root}, {"error", error.message()}});
        } catch (const std::exception &e) {
            if (errors.size() < 16)
                errors.push_back(
                    {{"root", pathUtf8(directory.lexically_relative(workspace))}, {"error", e.what()}});
        }
    }
    return {{"repositories", entries},
            {"truncated", truncated || !pending.empty()},
            {"visitedDirectories", visited},
            {"errors", errors}};
}
std::string gitFileArgument(const fs::path &workspace, const GitRepositorySelection &selection,
                            const fs::path &file, bool allowDirectories) {
    if (!containsPath(selection.directory, file) || file == selection.directory)
        throw std::runtime_error("File is outside the selected Git directory");
    const auto owner = gitRepositoryAt(workspace, file.parent_path());
    if (owner.marker && owner.directory != selection.directory)
        throw std::runtime_error(
            "File belongs to another nested Git repository; select that repository first");
    if (fs::is_directory(file) && !allowDirectories)
        throw std::runtime_error("Select a file rather than a whole folder for this Git action");
    return pathUtf8(file.lexically_relative(selection.directory));
}
Json parseGitWorktrees(const std::string &text) {
    if (text.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Git worktree catalogue exceeds 2 MiB");
    Json entries = Json::array(), current;
    std::set<std::string> seen;
    auto finish = [&] {
        if (current.is_null())
            return;
        if (!current.contains("path") || entries.size() >= 256)
            throw std::runtime_error("Invalid/oversized worktree catalogue");
        entries.push_back(current);
        current = nullptr;
    };
    size_t at = 0;
    while (at < text.size()) {
        const auto end = text.find('\0', at);
        if (end == std::string::npos)
            throw std::runtime_error("Git worktree catalogue must be NUL terminated");
        const auto field = text.substr(at, end - at);
        at = end + 1;
        if (field.empty()) {
            finish();
            continue;
        }
        if (field.starts_with("worktree ")) {
            if (!current.is_null())
                throw std::runtime_error("Missing Git worktree record separator");
            seen = {"worktree"};
            const auto path = field.substr(9);
            if (path.empty() || path.size() > 32768)
                throw std::runtime_error("Invalid Git worktree path");
            current = {{"path", path},      {"head", ""},
                       {"branch", ""},      {"bare", false},
                       {"detached", false}, {"locked", false},
                       {"lockReason", ""},  {"prunable", false},
                       {"pruneReason", ""}, {"primary", entries.empty()}};
        } else {
            if (current.is_null())
                throw std::runtime_error("Git worktree record has no path");
            if (!seen.insert(field.substr(0, field.find(' '))).second)
                throw std::runtime_error("Duplicate Git worktree catalogue field");
            if (field.starts_with("HEAD "))
                current["head"] = field.substr(5);
            else if (field.starts_with("branch "))
                current["branch"] = field.substr(7);
            else if (field == "bare")
                current["bare"] = true;
            else if (field == "detached")
                current["detached"] = true;
            else if (field == "locked" || field.starts_with("locked ")) {
                current["locked"] = true;
                current["lockReason"] = field.size() > 6 ? field.substr(7) : "";
            } else if (field == "prunable" || field.starts_with("prunable ")) {
                current["prunable"] = true;
                current["pruneReason"] = field.size() > 8 ? field.substr(9) : "";
            } else
                throw std::runtime_error("Unsupported Git worktree catalogue field");
        }
    }
    finish();
    if (entries.empty())
        throw std::runtime_error("Git returned no worktree records");
    return entries;
}
} // namespace cycode
