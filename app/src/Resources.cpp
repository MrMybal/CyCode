#include "cycode/Resources.h"
#include "cycode/FileTransactions.h"
#include "cycode/Recovery.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
bool equal(const fs::path &a, const fs::path &b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
bool inside(const fs::path &folder, const fs::path &path) {
    auto b = path.begin();
    for (auto a = folder.begin(); a != folder.end(); ++a, ++b)
        if (b == path.end() || !equal(*a, *b))
            return false;
    return true;
}
void ordinary(const fs::path &path) {
    if (!equal(fs::absolute(path).lexically_normal(), fs::weakly_canonical(path)))
        throw std::runtime_error("Resource operations cannot traverse links or redirected directories");
    if (fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Resource operations cannot modify symbolic links");
}
bool present(const fs::path &path) {
    return fs::symlink_status(path).type() != fs::file_type::not_found;
}
uint64_t now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
}
std::string identifier() {
    std::ostringstream out;
    out << "item-" << std::hex << std::setfill('0');
    std::random_device random;
    for (int i = 0; i < 4; ++i)
        out << std::setw(8) << uint32_t(random());
    return out.str();
}
bool validId(const std::string &id) {
    return id.size() == 37 && id.starts_with("item-") &&
           std::all_of(id.begin() + 5, id.end(),
                       [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
Json fingerprint(const fs::path &root) {
    ordinary(root);
    if (!present(root))
        throw std::runtime_error("Resource disappeared");
    std::vector<fs::path> paths{root};
    size_t bytes = 0, files = 0;
    if (fs::is_directory(root))
        for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
            if (paths.size() >= 4096 || it.depth() >= 64)
                throw std::runtime_error("Resource operation limit: 4096 entries / 64 levels");
            ordinary(it->path());
            for (const auto &part : it->path().lexically_relative(root))
                if (equal(part, fs::path(".git")) || equal(part, fs::path(".cycode")))
                    throw std::runtime_error("Resource tree contains protected metadata");
            paths.push_back(it->path());
        }
    std::sort(paths.begin(), paths.end());
    Json entries = Json::array();
    std::array<char, 65536> buffer;
    for (const auto &path : paths) {
        ordinary(path);
        if (fs::is_directory(path)) {
            entries.push_back({{"path", pathUtf8(path.lexically_relative(root))}, {"kind", "directory"}});
            continue;
        }
        if (!fs::is_regular_file(path))
            throw std::runtime_error("Only ordinary files and directories can be moved");
        const auto size = fs::file_size(path);
        const auto modified = fs::last_write_time(path);
        if (size > 256 * 1024 * 1024 || bytes + size > 256 * 1024 * 1024)
            throw std::runtime_error("Resource operation limit: 256 MiB");
        bytes += size;
        ++files;
        uint64_t hash = 14695981039346656037ULL;
        size_t read = 0;
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot read resource");
        while (input) {
            input.read(buffer.data(), buffer.size());
            const auto count = input.gcount();
            read += size_t(count);
            if (read > size)
                throw std::runtime_error("Resource changed during inspection");
            for (std::streamsize i = 0; i < count; ++i) {
                hash ^= static_cast<unsigned char>(buffer[size_t(i)]);
                hash *= 1099511628211ULL;
            }
        }
        if (input.bad() || read != size || fs::file_size(path) != size ||
            fs::last_write_time(path) != modified)
            throw std::runtime_error("Resource changed during inspection");
        entries.push_back({{"path", pathUtf8(path.lexically_relative(root))},
                           {"kind", "file"},
                           {"bytes", size},
                           {"hash", hash}});
    }
    return {{"guard", RecoveryStore::fingerprint(entries.dump())},
            {"entries", paths.size()},
            {"files", files},
            {"bytes", bytes},
            {"directory", fs::is_directory(root)}};
}
} // namespace
bool resourceContains(const std::string &folder, const std::string &path) {
    return !folder.empty() && inside(utf8Path(folder), utf8Path(path));
}
std::string resourceRebase(const std::string &path, const std::string &from, const std::string &to) {
    if (!resourceContains(from, path))
        return path;
    auto remainder = utf8Path(path).lexically_relative(utf8Path(from));
    if (remainder == ".")
        return to;
    return pathUtf8(utf8Path(to) / remainder);
}
static Resources::Json installSnapshot(const fs::path &path) {
    ordinary(path);
    if (!present(path))
        return {{"exists", false}, {"guard", "missing"}, {"bytes", 0}, {"permissions", 0}};
    if (!fs::is_regular_file(path) || fs::file_size(path) > 16 * 1024 * 1024)
        throw std::runtime_error("Working-file replacement requires an ordinary file up to 16 MiB");
    const auto data = fingerprint(path);
    if (data.at("bytes").get<uint64_t>() > 16 * 1024 * 1024)
        throw std::runtime_error("Working file grew beyond the 16 MiB replacement limit");
    return {{"exists", true},
            {"guard", data.at("guard")},
            {"bytes", data.at("bytes")},
            {"permissions", static_cast<uint32_t>(fs::status(path).permissions())}};
}
struct Resources::State {
    const fs::path root, entry;
    std::map<std::string, Json> plans;
    std::map<std::string, std::string> installPayloads;
    std::vector<std::string> order;
    explicit State(fs::path root, fs::path entry) : root(fs::canonical(root)), entry(std::move(entry)) {}
    fs::path path(const std::string &name) const {
        if (name.empty() || name.size() > 4096 || name.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid resource path");
        auto input = utf8Path(name);
        if (input.is_absolute() || input.has_root_name())
            throw std::runtime_error("Use a workspace-relative resource path");
        for (const auto &part : input)
            if (part == "..")
                throw std::runtime_error("Parent traversal is not allowed");
        const auto result = (root / input).lexically_normal();
        ordinary(result);
        if (!inside(root, result) || equal(root, result))
            throw std::runtime_error("Cannot change the workspace root");
        const auto relative = result.lexically_relative(root);
        for (const auto &part : relative)
            if (equal(part, fs::path(".git")) || equal(part, fs::path(".cycode")))
                throw std::runtime_error("Protected workspace metadata");
        const auto first = *relative.begin();
        if (equal(first, fs::path("runtime")) || equal(first, fs::path("data")))
            throw std::runtime_error("Protected installation data");
        return result;
    }
    fs::path trashBase(bool create = false) const {
        const auto base = root / ".cycode/trash";
        ordinary(root);
        ordinary(root / ".cycode");
        ordinary(base);
        if (create) {
            fs::create_directory(root / ".cycode");
            fs::create_directory(base);
            ordinary(base);
        }
        return base;
    }
    std::pair<fs::path, Json> trashed(const std::string &id) const {
        if (!validId(id))
            throw std::runtime_error("Invalid trash reference");
        const auto directory = trashBase() / id;
        ordinary(directory);
        ordinary(directory / "record.json");
        ordinary(directory / "payload");
        const auto raw = readTextFileBytes(directory / "record.json");
        if (raw.size() > 16384)
            throw std::runtime_error("Trash metadata exceeds its limit");
        const auto record = Json::parse(raw);
        if (record.at("schemaVersion") != 1 || record.at("id") != id ||
            !record.at("createdAt").is_number_unsigned() || !record.at("guard").is_string() ||
            record.size() != 5)
            throw std::runtime_error("Invalid trash metadata");
        path(record.at("path"));
        if (!present(directory / "payload"))
            throw std::runtime_error("Trash payload is unavailable");
        return {directory, record};
    }
    std::string issue(Json plan) {
        const auto id = identifier();
        plan["id"] = id;
        plan["state"] = "preview";
        plan["createdAt"] = now();
        plans[id] = std::move(plan);
        order.push_back(id);
        while (order.size() > 32) {
            installPayloads.erase(order.front());
            plans.erase(order.front());
            order.erase(order.begin());
        }
        return id;
    }
};
Resources::Resources(fs::path root, fs::path entry)
    : state_(std::make_unique<State>(std::move(root), std::move(entry))) {}
Resources::~Resources() = default;
Resources::Json Resources::preview(const Json &params, const Json &documents) {
    const std::string operation = params.at("operation");
    if (operation != "createFile" && operation != "createDirectory" && operation != "move" &&
        operation != "delete" && operation != "restore")
        throw std::runtime_error("Unsupported resource operation");
    Json plan = {
        {"operation", operation},
        {"path", ""},
        {"destination", ""},
        {"trash", ""},
        {"openDocuments", Json::array()},
        {"blocked", false},
        {"warning",
         "Physical paths only: includes, project manifests and debugger breakpoints are not rewritten."}};
    fs::path source, target;
    if (operation == "restore") {
        const std::string id = params.at("trash");
        const auto [directory, record] = state_->trashed(id);
        plan["trash"] = id;
        plan["path"] = record.at("path");
        source = directory / "payload";
        target = state_->path(params.value("destination", record.at("path").get<std::string>()));
        plan["trashGuard"] = RecoveryStore::fingerprint(record.dump());
        plan["expectedTrashContent"] = record.at("guard");
    } else {
        source = state_->path(params.at("path"));
        plan["path"] = pathUtf8(source.lexically_relative(state_->root));
        if (operation == "move" || operation == "delete") {
            if (!present(source))
                throw std::runtime_error("Source does not exist");
            if (!equal(state_->entry, state_->root) && inside(source, state_->entry))
                throw std::runtime_error("Cannot move or delete the active project entry");
        }
        if (operation == "move")
            target = state_->path(params.at("destination"));
        if (operation == "createFile" || operation == "createDirectory") {
            target = source;
            source.clear();
        }
    }
    if (!target.empty()) {
        if (present(target))
            throw std::runtime_error("Destination already exists (case-only renames are not supported)");
        if (!fs::is_directory(target.parent_path()))
            throw std::runtime_error("Destination parent must already exist");
        if (!source.empty() && inside(source, target))
            throw std::runtime_error("Cannot move a directory into itself");
        plan["destination"] = pathUtf8(target.lexically_relative(state_->root));
    }
    plan["snapshot"] = source.empty() ? Json{{"guard", "empty"},
                                             {"entries", 0},
                                             {"files", 0},
                                             {"bytes", 0},
                                             {"directory", operation == "createDirectory"}}
                                      : fingerprint(source);
    for (const auto &doc : documents)
        if ((operation != "restore" && resourceContains(plan.at("path"), doc.at("path"))) ||
            resourceContains(plan.at("destination"), doc.at("path"))) {
            plan["openDocuments"].push_back(doc);
            const bool reattach = operation == "restore" && doc.value("missing", false) &&
                                  doc.value("diskConflict", std::string{}) == "missing";
            if (doc.at("dirty").get<bool>() ||
                (!doc.at("diskConflict").get<std::string>().empty() && !reattach) ||
                (operation == "restore" && !reattach) || operation == "createFile" ||
                operation == "createDirectory")
                plan["blocked"] = true;
        }
    if (plan.at("openDocuments").size() > 128)
        throw std::runtime_error("Resource operation limit: 128 open documents");
    if (operation == "restore" && plan.at("snapshot").at("guard") != plan.at("expectedTrashContent"))
        throw std::runtime_error("Trash content has changed; inspect its payload manually");
    const auto id = state_->issue(std::move(plan));
    return state_->plans.at(id);
}
Resources::Json Resources::apply(const std::string &id, const Commit &commit) {
    const auto found = state_->plans.find(id);
    if (found == state_->plans.end())
        throw std::runtime_error("Resource plan expired; prepare a new preview");
    auto &plan = found->second;
    if (plan.at("state") != "preview" || now() - plan.at("createdAt").get<uint64_t>() > 600000)
        throw std::runtime_error("Resource plan is stale");
    if (plan.at("blocked").get<bool>())
        throw std::runtime_error("Save or close affected documents, then preview again");
    const std::string operation = plan.at("operation"), path = plan.at("path"),
                      destination = plan.at("destination");
    if (operation == "installFile" || operation == "removeFile")
        throw std::runtime_error("Use the owning Git working-file service for this resource plan");
    fs::path source, target;
    if (operation == "restore") {
        const auto [directory, record] = state_->trashed(plan.at("trash"));
        if (RecoveryStore::fingerprint(record.dump()) != plan.at("trashGuard").get<std::string>())
            throw std::runtime_error("Trash metadata changed since preview");
        source = directory / "payload";
    } else if (operation == "move" || operation == "delete")
        source = state_->path(path);
    if (!source.empty() && fingerprint(source).at("guard") != plan.at("snapshot").at("guard"))
        throw std::runtime_error("DiskConflict: resource changed since preview");
    if (operation != "delete") {
        target = state_->path(destination);
        if (present(target) || !fs::is_directory(target.parent_path()))
            throw std::runtime_error("Destination changed since preview");
    }
    std::optional<std::string> deletionRecord;
    fs::path deletionDirectory;
    // Write the deletion record before moving: an interrupted move remains manually recoverable.
    if (operation == "delete") {
        const auto directory = state_->trashBase(true) / id;
        ordinary(directory);
        if (!fs::create_directory(directory))
            throw std::runtime_error("Trash entry already exists");
        deletionDirectory = directory;
        deletionRecord = Json({{"schemaVersion", 1},
                               {"id", id},
                               {"path", path},
                               {"createdAt", now()},
                               {"guard", plan.at("snapshot").at("guard")}})
                             .dump() +
                         "\n";
        writeTextFileAtomic(directory / "record.json", *deletionRecord, std::nullopt);
        target = directory / "payload";
        plan["trash"] = id;
    }
    Json afterGuard = plan.at("snapshot").at("guard");
    if (operation == "createFile")
        afterGuard = RecoveryStore::fingerprint(Json::array({{{"path", "."},
                                                              {"kind", "file"},
                                                              {"bytes", 0},
                                                              {"hash", uint64_t(14695981039346656037ULL)}}})
                                                    .dump());
    if (operation == "createDirectory")
        afterGuard = RecoveryStore::fingerprint(Json::array({{{"path", "."}, {"kind", "directory"}}}).dump());
    Json effect;
    try {
        effect = commit(plan, [&] {
            if (operation == "createFile")
                writeTextFileAtomic(target, "", std::nullopt);
            else if (operation == "createDirectory") {
                if (!fs::create_directory(target))
                    throw std::runtime_error("Directory already exists");
            } else
                moveExclusive(source, target);
        });
    } catch (...) {
        // Remove only this failed action's own empty metadata, never a payload or source tree.
        if (deletionRecord)
            try {
                ordinary(deletionDirectory);
                if (!present(deletionDirectory / "payload") &&
                    readTextFileBytes(deletionDirectory / "record.json") == *deletionRecord) {
                    fs::remove(deletionDirectory / "record.json");
                    fs::remove(deletionDirectory);
                }
            } catch (...) {
            }
        throw;
    }
    plan["state"] = "applied";
    plan["effect"] = effect;
    plan["appliedAt"] = now();
    // Snapshot is invariant under a rename. Creates start empty; avoid a fallible read after commit.
    plan["afterGuard"] = afterGuard;
    if (operation == "restore")
        try {
            const auto directory = state_->trashBase() / plan.at("trash").get<std::string>();
            ordinary(directory);
            if (!present(directory / "payload") &&
                RecoveryStore::fingerprint(
                    Json::parse(readTextFileBytes(directory / "record.json")).dump()) ==
                    plan.at("trashGuard").get<std::string>()) {
                ordinary(directory / "record.json");
                fs::remove(directory / "record.json");
                fs::remove(directory);
            }
        } catch (...) {
        } // The restored payload is safe; leftover empty metadata is harmless.
    return plan;
}
Resources::Json Resources::undo(const std::string &id, const Json &documents, const Commit &commit) {
    const auto found = state_->plans.find(id);
    if (found == state_->plans.end() || found->second.at("state") != "applied")
        throw std::runtime_error("No applied resource operation to undo");
    const auto original = found->second;
    const std::string operation = original.at("operation");
    if (operation == "installFile" || operation == "removeFile")
        throw std::runtime_error("Use the owning Git working-file service to undo this resource plan");
    Json params;
    if (operation == "delete")
        params = {{"operation", "restore"}, {"trash", original.at("trash")}};
    else if (operation == "move")
        params = {{"operation", "move"},
                  {"path", original.at("destination")},
                  {"destination", original.at("path")}};
    else
        params = {{"operation", "delete"}, {"path", original.at("destination")}};
    const auto inverse = preview(params, documents);
    if (inverse.at("snapshot").at("guard") != original.at("afterGuard"))
        throw std::runtime_error("DiskConflict: resource changed after the operation");
    auto result = apply(inverse.at("id"), commit);
    // issue() can evict an old entry, so do not retain map references across preview().
    if (auto it = state_->plans.find(id); it != state_->plans.end())
        it->second["state"] = "undone";
    result["undone"] = id;
    return result;
}
Resources::Json Resources::previewFileInstall(const std::string &path, std::optional<std::string> bytes,
                                              bool executable, const Json &documents) {
    const auto target = state_->path(path);
    if (equal(target, state_->entry))
        throw std::runtime_error("Cannot replace the active project entry");
    if (!fs::is_directory(target.parent_path()))
        throw std::runtime_error("Working-file parent must already exist");
    const auto before = installSnapshot(target);
    if (!bytes && !before.at("exists").get<bool>())
        throw std::runtime_error("Working file is already absent");
    if (bytes && bytes->size() > 16 * 1024 * 1024)
        throw std::runtime_error("Staged blob exceeds the 16 MiB working-file limit");
    size_t retained = bytes ? bytes->size() : 0;
    for (const auto &[id, payload] : state_->installPayloads)
        retained += payload.size();
    for (const auto &old : state_->order) {
        if (retained <= 64 * 1024 * 1024)
            break;
        if (auto payload = state_->installPayloads.find(old); payload != state_->installPayloads.end()) {
            retained -= payload->second.size();
            state_->installPayloads.erase(payload);
            state_->plans.at(old)["state"] = "expired";
        }
    }
    Json affected = Json::array();
    for (const auto &doc : documents)
        if (resourceContains(path, doc.at("path")) || resourceContains(".cycode/trash", doc.at("path")))
            affected.push_back(doc);
    Json plan = {{"operation", bytes ? "installFile" : "removeFile"},
                 {"path", path},
                 {"destination", path},
                 {"snapshot", before},
                 {"parentIdentity", directoryIdentity(target.parent_path())},
                 {"replacementBytes", bytes ? bytes->size() : 0},
                 {"executable", executable},
                 {"payloadGuard", bytes ? RecoveryStore::fingerprint(*bytes) : ""},
                 {"openDocuments", affected},
                 {"blocked", !affected.empty()},
                 {"warning", "Close affected documents. Existing bytes move to persistent project trash "
                             "before installation. No Git filters or EOL conversion."}};
    const auto id = state_->issue(std::move(plan));
    if (bytes)
        state_->installPayloads[id] = std::move(*bytes);
    return state_->plans.at(id);
}
Resources::Json Resources::applyFileInstall(const std::string &id, const Commit &commit) {
    const auto found = state_->plans.find(id);
    if (found == state_->plans.end())
        throw std::runtime_error("Working-file preview expired");
    auto &plan = found->second;
    const auto operation = plan.at("operation").get<std::string>();
    if ((operation != "installFile" && operation != "removeFile") || plan.at("state") != "preview" ||
        now() - plan.at("createdAt").get<uint64_t>() > 600000)
        throw std::runtime_error("Working-file preview is stale");
    if (plan.at("blocked").get<bool>())
        throw std::runtime_error("Close affected documents, then review the working-file action again");
    const auto target = state_->path(plan.at("path"));
    auto unchanged = [&] {
        (void)state_->path(plan.at("path"));
        if (directoryIdentity(target.parent_path()) != plan.at("parentIdentity").get<std::string>() ||
            installSnapshot(target) != plan.at("snapshot"))
            throw std::runtime_error("Working file, permissions or parent changed since preview");
    };
    unchanged();
    const auto directory = state_->trashBase(true) / id;
    if (!fs::create_directory(directory))
        throw std::runtime_error("Working-file backup already exists; review a fresh action");
    ordinary(directory);
    const auto replacement = directory / "replacement";
    if (operation == "installFile") {
        const auto payload = state_->installPayloads.find(id);
        if (payload == state_->installPayloads.end() ||
            RecoveryStore::fingerprint(payload->second) != plan.at("payloadGuard").get<std::string>())
            throw std::runtime_error("Working-file preview payload is unavailable");
        writeFileExclusive(replacement, payload->second);
#ifndef _WIN32
        auto permissions = plan.at("snapshot").at("exists").get<bool>()
                               ? static_cast<fs::perms>(plan.at("snapshot").at("permissions").get<uint32_t>())
                               : fs::perms::owner_read | fs::perms::owner_write;
        permissions &= ~(fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec);
        if (plan.at("executable").get<bool>()) {
            if ((permissions & fs::perms::owner_read) != fs::perms::none)
                permissions |= fs::perms::owner_exec;
            if ((permissions & fs::perms::group_read) != fs::perms::none)
                permissions |= fs::perms::group_exec;
            if ((permissions & fs::perms::others_read) != fs::perms::none)
                permissions |= fs::perms::others_exec;
        }
        permissions &= ~(fs::perms::set_uid | fs::perms::set_gid | fs::perms::sticky_bit);
        fs::permissions(replacement, permissions, fs::perm_options::replace);
#endif
        plan["afterSnapshot"] = installSnapshot(replacement);
    } else
        plan["afterSnapshot"] = {{"exists", false}, {"guard", "missing"}, {"bytes", 0}, {"permissions", 0}};
    const bool existed = plan.at("snapshot").at("exists");
    if (existed)
        writeTextFileAtomic(directory / "record.json",
                            Json({{"schemaVersion", 1},
                                  {"id", id},
                                  {"path", plan.at("path")},
                                  {"createdAt", now()},
                                  {"guard", plan.at("snapshot").at("guard")}})
                                    .dump() +
                                "\n",
                            std::nullopt);
    writeTextFileAtomic(directory / "install.json", plan.dump() + "\n", std::nullopt);
    std::string error;
    plan["backup"] = existed ? id : "";
    plan["state"] = "applying";
    try {
        plan["effect"] = commit(plan, [&] {
            unchanged();
            try {
                if (existed)
                    moveExclusive(target, directory / "payload");
                if (operation == "installFile")
                    moveExclusive(replacement, target);
            } catch (const std::exception &failure) {
                error = failure.what();
            }
        });
    } catch (...) {
        plan["state"] = "failed";
        state_->installPayloads.erase(id);
        throw;
    }
    state_->installPayloads.erase(id);
    plan["backup"] = existed && present(directory / "payload") ? id : "";
    try {
        if (!error.empty())
            throw std::runtime_error(error);
        if (installSnapshot(target) != plan.at("afterSnapshot"))
            throw std::runtime_error("Installed working file could not be confirmed");
        if (existed && installSnapshot(directory / "payload") != plan.at("snapshot"))
            throw std::runtime_error("Original backup could not be confirmed");
        plan["state"] = "applied";
    } catch (const std::exception &failure) {
        plan["state"] = "partial";
        error = failure.what();
    }
    plan["error"] = error;
    // The pre-mutation journal and payloads remain recoverable even if this update fails.
    try {
        writeTextFileAtomic(directory / "install.json", plan.dump() + "\n",
                            readTextFileBytes(directory / "install.json"));
    } catch (const std::exception &failure) {
        plan["state"] = "partial";
        plan["error"] = std::string("Backup journal update failed: ") + failure.what();
    }
    return plan;
}
Resources::Json Resources::undoFileInstall(const std::string &id, const Json &documents,
                                           const Commit &commit) {
    const auto found = state_->plans.find(id);
    if (found == state_->plans.end() || found->second.at("state") != "applied")
        throw std::runtime_error("No completed working-file action to undo");
    const auto original = found->second;
    const auto path = original.at("path").get<std::string>();
    const auto target = state_->path(path);
    if (installSnapshot(target) != original.at("afterSnapshot"))
        throw std::runtime_error("Working file changed; Undo will not overwrite newer content");
    for (const auto &doc : documents)
        if (resourceContains(path, doc.at("path")))
            throw std::runtime_error("Close affected documents before working-file Undo");
    if (original.at("snapshot").at("exists").get<bool>()) {
        const auto [directory, record] = state_->trashed(original.at("backup"));
        if (record.at("path") != path || installSnapshot(directory / "payload") != original.at("snapshot"))
            throw std::runtime_error("Original backup changed; inspect project trash");
    }
    Json result = {{"id", id},         {"state", "undone"}, {"path", path}, {"backup", original.at("backup")},
                   {"undoBackup", ""}, {"error", ""}};
    try {
        if (present(target)) {
            const auto removal = preview({{"operation", "delete"}, {"path", path}}, documents);
            if (removal.at("snapshot").at("guard") != original.at("afterSnapshot").at("guard"))
                throw std::runtime_error("Working file changed during Undo preview");
            const auto removed = apply(removal.at("id"), commit);
            result["undoBackup"] = removed.at("trash");
        }
        if (original.at("snapshot").at("exists").get<bool>()) {
            const auto restore =
                preview({{"operation", "restore"}, {"trash", original.at("backup")}}, documents);
            (void)apply(restore.at("id"), commit);
        }
        if (installSnapshot(target) != original.at("snapshot"))
            throw std::runtime_error("Restored working file could not be confirmed");
        result["backup"] =
            ""; // The original payload is back at its path; only displaced content stays in trash.
    } catch (const std::exception &failure) {
        result["state"] = "partial";
        result["error"] = failure.what();
    }
    if (auto current = state_->plans.find(id); current != state_->plans.end())
        current->second["state"] = result.at("state");
    return result;
}
Resources::Json Resources::trash(size_t limit) const {
    if (limit < 1 || limit > 256)
        throw std::runtime_error("Invalid trash limit");
    Json items = Json::array(), errors = Json::array();
    bool truncated = false;
    const auto base = state_->trashBase();
    if (!fs::is_directory(base))
        return {{"items", items}, {"errors", errors}, {"truncated", false}};
    size_t count = 0;
    for (const auto &entry : fs::directory_iterator(base)) {
        if (++count > 2048) {
            truncated = true;
            break;
        }
        const auto id = pathUtf8(entry.path().filename());
        if (!validId(id))
            continue;
        try {
            const auto [directory, record] = state_->trashed(id);
            auto item = record;
            item["directory"] = fs::is_directory(directory / "payload");
            items.push_back(item);
        } catch (const std::exception &e) {
            if (fs::exists(entry.path() / "payload"))
                errors.push_back({{"id", id}, {"error", e.what()}});
        }
    }
    std::sort(items.begin(), items.end(),
              [](const auto &a, const auto &b) { return a.at("createdAt") > b.at("createdAt"); });
    if (items.size() > limit) {
        items.erase(items.begin() + limit, items.end());
        truncated = true;
    }
    return {{"items", items}, {"errors", errors}, {"truncated", truncated}};
}
Resources::Json Resources::children(const std::string &path) const {
    const auto folder = path.empty() || path == "." ? state_->root : state_->path(path);
    ordinary(folder);
    if (!fs::is_directory(folder))
        throw std::runtime_error("Folder is unavailable");
    Json items = Json::array(), errors = Json::array();
    bool truncated = false;
    size_t scanned = 0;
    for (const auto &entry : fs::directory_iterator(folder)) {
        if (++scanned > 8192 || items.size() >= 2048) {
            truncated = true;
            break;
        }
        try {
            const auto relative = pathUtf8(entry.path().lexically_relative(state_->root));
            try {
                (void)state_->path(relative);
            } catch (const std::exception &) {
                continue;
            }
            if (!entry.is_directory() && !entry.is_regular_file())
                continue;
            items.push_back({{"path", relative},
                             {"name", pathUtf8(entry.path().filename())},
                             {"directory", entry.is_directory()}});
        } catch (const std::exception &e) {
            errors.push_back({{"error", e.what()}});
        }
    }
    std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
        if (a.at("directory") != b.at("directory"))
            return a.at("directory").template get<bool>();
        return a.at("name") < b.at("name");
    });
    return {{"path", pathUtf8(folder.lexically_relative(state_->root))},
            {"items", items},
            {"errors", errors},
            {"truncated", truncated}};
}
} // namespace cycode
