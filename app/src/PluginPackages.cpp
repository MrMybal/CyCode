#include "PluginPackageData.h"
#include "cycode/FileTransactions.h"
#include "cycode/Paths.h"
#include "cycode/PluginManifest.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using namespace package_data;
struct OwnedStaging {
    fs::path path;
    std::string identity;
    Json files;
    bool published = false;
    OwnedStaging(fs::path p, Json list)
        : path(std::move(p)), identity(directoryIdentity(path)), files(std::move(list)) {}
    ~OwnedStaging() {
        if (published)
            return;
        try {
            const auto status = fs::symlink_status(path);
            if (redirected(path) || !fs::is_directory(status) || directoryIdentity(path) != identity)
                return;
            std::set<fs::path> directories;
            for (const auto &file : files) {
                const auto name = file.at("path").get<std::string>();
                fs::remove(join(path, name));
                auto parent = utf8Path(name).parent_path();
                while (!parent.empty()) {
                    directories.insert(parent);
                    parent = parent.parent_path();
                }
            }
            for (const auto *name : {"manifest.json", "signature.json", "package.json"})
                fs::remove(join(path, name));
            std::vector<fs::path> ordered(directories.begin(), directories.end());
            std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
                return std::distance(a.begin(), a.end()) > std::distance(b.begin(), b.end());
            });
            for (const auto &directory : ordered)
                fs::remove(join(path, pathUtf8(directory)));
            fs::remove(path);
        } catch (
            const std::exception &) { /* Unknown/locked paths remain for manual inspection; never recurse. */
        }
    }
};
void expected(const Store &s, const std::string &revision, const std::string *trust = nullptr) {
    if (revision != s.revision())
        throw std::runtime_error("Managed plugin selection changed; refresh before editing");
    if (trust && *trust != s.trustRevision())
        throw std::runtime_error("Plugin publisher trust changed; review the package again");
}
void compatible(const Package &p) {
    if (p.document.at("platform") != pluginHostInfo().at("platform"))
        throw std::runtime_error("Package binary targets a different host platform");
    const auto result = pluginCompatibility(p.plugin);
    if (!result.at("compatible").get<bool>())
        throw std::runtime_error(result.at("reason").get<std::string>());
}
Package selected(const Store &s, const std::string &id, const std::string &digest, bool payload = false) {
    auto p = package(s.packagePath(id, digest), payload, true);
    if (p.plugin.at("id") != id || p.digest != digest)
        throw std::runtime_error("Installed package identity mismatch");
    return p;
}
std::map<std::string, Json> legacy(const fs::path &installation) {
    std::map<std::string, Json> result;
    size_t visited = 0;
    for (const auto &directory : {installation / "runtime/plugins", installation / "extensions"}) {
        if (!fs::is_directory(directory))
            continue;
        for (const auto &item : fs::directory_iterator(directory)) {
            if (++visited > 4096)
                throw std::runtime_error("Legacy plugin inventory exceeds its limit");
            if (item.path().extension() != ".json" || item.is_symlink())
                continue;
            try {
                auto p = parse(read(item.path(), 256 * 1024));
                validatePluginManifest(p);
                const auto id = p.at("id").get<std::string>();
                if (result.contains(id))
                    throw std::runtime_error("Duplicate legacy plugin ID: " + id);
                result[id] = p;
            } catch (const std::exception &) { /* Existing loader reports malformed legacy manifests. */
            }
        }
    }
    return result;
}
void selection(const Store &s, const fs::path &installation, const Json &entries,
               const std::string &changed) {
    auto pool = legacy(installation);
    std::set<std::string> managed;
    for (const auto &[id, entry] : entries.items()) {
        if (pool.contains(id))
            throw std::runtime_error("Managed package cannot replace a bundled/legacy plugin ID: " + id);
        const auto p = selected(s, id, entry.at("current"));
        if (!s.trusted(p.publicKey))
            throw std::runtime_error("Selected package publisher is not trusted: " + id);
        compatible(p);
        pool[id] = p.plugin;
        managed.insert(id);
    }
    const auto dependency = [&](const Json &d) {
        const auto id = d.at("id").get<std::string>();
        if (!pool.contains(id)) {
            if (!d.value("optional", false))
                throw std::runtime_error("Missing required plugin dependency: " + id);
            return false;
        }
        const auto version = pluginVersion(pool.at(id).value("version", std::string("0.0.0")));
        if ((d.contains("minimum") && version < pluginVersion(d.at("minimum"))) ||
            (d.contains("maximumExclusive") && version >= pluginVersion(d.at("maximumExclusive"))))
            throw std::runtime_error("Incompatible plugin dependency: " + id);
        if (!pluginCompatibility(pool.at(id)).at("compatible").get<bool>())
            throw std::runtime_error("Plugin dependency is incompatible with this host: " + id);
        return true;
    };
    std::map<std::string, int> colors;
    std::function<void(const std::string &, size_t)> visit = [&](const std::string &id, size_t depth) {
        if (depth > 128)
            throw std::runtime_error("Plugin dependency graph exceeds its depth limit");
        if (colors[id] == 2)
            return;
        if (colors[id] == 1)
            throw std::runtime_error("Plugin dependency cycle: " + id);
        colors[id] = 1;
        for (const auto &d : pool.at(id).value("dependencies", Json::array()))
            if (dependency(d))
                visit(d.at("id"), depth + 1);
        colors[id] = 2;
    };
    for (const auto &id : managed)
        visit(id, 0);
    // An unmanaged extension may depend on a managed plugin too.
    for (const auto &[id, p] : pool)
        if (!managed.contains(id))
            for (const auto &d : p.value("dependencies", Json::array()))
                if (d.at("id") == changed)
                    (void)dependency(d);
}
Json histories(const Json &old, const std::string &next) {
    Json history = Json::array();
    std::set<std::string> seen{next};
    if (old.is_object()) {
        if (old.contains("current") && seen.insert(old.at("current").get<std::string>()).second)
            history.push_back(old.at("current"));
        for (const auto &item : old.value("history", Json::array()))
            if (history.size() < 4 && seen.insert(item.get<std::string>()).second)
                history.push_back(item);
    }
    return history;
}
Json catalog(Store &s) {
    Json items = Json::array(), garbage = Json::array(), publishers = Json::array(),
         storageErrors = Json::array();
    std::set<std::string> referenced;
    for (const auto &[id, entry] : s.index.at("entries").items()) {
        Json item = {{"id", id}, {"digest", entry.at("current")}, {"history", entry.at("history")}};
        referenced.insert(id + "/" + entry.at("current").get<std::string>());
        for (const auto &d : entry.at("history"))
            referenced.insert(id + "/" + d.get<std::string>());
        try {
            const auto p = selected(s, id, entry.at("current"));
            item.update(summary(p));
            item["trusted"] = s.trusted(p.publicKey);
            item["payloadVerified"] = false;
        } catch (const std::exception &e) {
            item["error"] = e.what();
        }
        item["versions"] = Json::array();
        for (const auto &digest : entry.at("history")) {
            Json version = {{"id", id}, {"digest", digest}};
            try {
                auto data = summary(selected(s, id, digest));
                data.erase("permissions");
                data.erase("dependencies");
                version.update(data);
            } catch (const std::exception &e) {
                version["error"] = e.what();
            }
            item["versions"].push_back(version);
        }
        items.push_back(item);
    }
    size_t visited = 0;
    bool truncated = false;
    for (const auto &stored : fs::directory_iterator(s.root / "packages")) {
        if (++visited > 4096) {
            truncated = true;
            break;
        }
        if (stored.is_symlink() || !stored.is_directory() || !hex(pathUtf8(stored.path().filename()), 32))
            continue;
        try {
            const auto p = package(stored.path(), false, true);
            const auto id = p.plugin.at("id").get<std::string>();
            if (pathUtf8(stored.path().filename()) != p.digest.substr(0, 64))
                throw std::runtime_error("Managed package directory identity mismatch");
            if (referenced.contains(id + "/" + p.digest))
                continue;
            if (garbage.size() >= 128) {
                truncated = true;
                continue;
            }
            auto data = summary(p);
            data.erase("permissions");
            data.erase("dependencies");
            garbage.push_back(data);
        } catch (const std::exception &e) {
            if (storageErrors.size() < 16)
                storageErrors.push_back(
                    {{"path", pathUtf8(stored.path().filename())},
                     {"error", e.what()}}); /* Unverifiable files are never cleanup candidates. */
        }
    }
    for (const auto &[key, value] : s.trust.at("publishers").items()) {
        auto item = value;
        item["publicKey"] = key;
        publishers.push_back(item);
    }
    return {{"revision", s.revision()},
            {"trustRevision", s.trustRevision()},
            {"entries", items},
            {"publishers", publishers},
            {"garbage", garbage},
            {"garbageTruncated", truncated},
            {"storageErrors", storageErrors}};
}
Json catalogResult(Store &s) {
    try {
        return catalog(s);
    } catch (const std::exception &e) {
        Json entries = Json::array();
        for (const auto &[id, value] : s.index.at("entries").items())
            entries.push_back({{"id", id},
                               {"digest", value.at("current")},
                               {"history", value.at("history")},
                               {"error", e.what()}});
        return {{"revision", s.revision()}, {"trustRevision", s.trustRevision()},
                {"entries", entries},       {"publishers", Json::array()},
                {"garbage", Json::array()}, {"garbageTruncated", false},
                {"catalogError", e.what()}};
    }
}
} // namespace
PluginPackages::PluginPackages(fs::path installation) : installation_(std::move(installation)) {
    if (installation_.empty())
        throw std::runtime_error("Plugin package installation root is unavailable");
    installation_ = fs::canonical(installation_);
}
PluginPackages::Json PluginPackages::catalog() const {
    Store store(installation_, NativeFileLeaseMode::shared);
    return ::cycode::catalog(store);
}
PluginPackages::Json PluginPackages::inspect(const fs::path &source) const {
    Store s(installation_, NativeFileLeaseMode::shared);
    const auto revision = s.revision(), trustRevision = s.trustRevision();
    const auto trusted = s.trust;
    const auto index = s.index;
    s.lease.reset();
    const auto p = package(fs::canonical(source), true);
    auto result = summary(p);
    result["revision"] = revision;
    result["trustRevision"] = trustRevision;
    result["trusted"] = trusted.at("publishers").contains(p.publicKey) &&
                        trusted.at("publishers").at(p.publicKey).at("enabled").get<bool>();
    result["signatureValid"] = true;
    result["payloadVerified"] = true;
    result["source"] = pathUtf8(fs::canonical(source));
    result["current"] = index.at("entries").value(p.plugin.at("id").get<std::string>(), Json(nullptr));
    result["compatible"] = p.document.at("platform") == pluginHostInfo().at("platform") &&
                           pluginCompatibility(p.plugin).at("compatible").get<bool>();
    return result;
}
PluginPackages::Json PluginPackages::install(const fs::path &source, const std::string &revision,
                                             const std::string &trustRevision,
                                             const std::string &digest) const {
    const auto input = fs::canonical(source);
    auto p = package(input, false);
    if (p.digest != digest)
        throw std::runtime_error("Package metadata changed since review");
    compatible(p);
    Store initial(installation_);
    expected(initial, revision, &trustRevision);
    if (!initial.trusted(p.publicKey))
        throw std::runtime_error("Publisher is not trusted; enroll it explicitly from the desktop");
    const auto id = p.plugin.at("id").get<std::string>();
    if (legacy(installation_).contains(id))
        throw std::runtime_error("Managed package cannot replace a bundled/legacy plugin ID");
    const auto old = initial.index.at("entries").value(id, Json(nullptr));
    if (!old.is_null()) {
        const auto previous = selected(initial, id, old.at("current"));
        if (previous.publicKey != p.publicKey)
            throw std::runtime_error(
                "Publisher key changed; remove the old plugin before explicitly installing a new publisher");
        if (previous.digest == p.digest) {
            (void)selected(initial, id, p.digest, true);
            auto result = ::cycode::catalogResult(initial);
            result["action"] = "alreadyInstalled";
            return result;
        }
        if (previous.digest != p.digest &&
            pluginVersion(p.plugin.at("version")) <= pluginVersion(previous.plugin.at("version")))
            throw std::runtime_error(
                "Updates require a newer version; use explicit rollback for installed history");
    }
    if (fs::exists(initial.packagePath(id, p.digest))) {
        (void)selected(initial, id, p.digest, true);
        auto entries = initial.index.at("entries");
        entries[id] = {{"current", p.digest}, {"history", histories(old, p.digest)}};
        selection(initial, installation_, entries, id);
        initial.index["entries"] = entries;
        initial.saveIndex();
        auto result = ::cycode::catalogResult(initial);
        result["action"] = "installed";
        result["id"] = id;
        result["restartRequired"] = true;
        return result;
    }
    const auto staging =
        initial.root / ".staging" /
        utf8Path("install-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!fs::create_directory(staging))
        throw std::runtime_error("Plugin staging directory already exists");
    OwnedStaging owned(staging, p.document.at("files"));
    initial.lease.reset();
    // Payload copying does not hold the global index lock. No staging code is loaded.
    writeTextFileAtomic(staging / "package.json", p.bytes, std::nullopt);
    writeTextFileAtomic(staging / "signature.json", p.signatureBytes, std::nullopt);
    writeTextFileAtomic(staging / "manifest.json", p.plugin.dump(2) + "\n", std::nullopt);
    for (const auto &file : p.document.at("files")) {
        const auto path = join(staging, file.at("path"));
        auto parent = staging;
        for (const auto &part : utf8Path(file.at("path")).parent_path()) {
            parent /= part;
            directory(parent);
        }
        copyChecked(join(input, file.at("path")), &path, file);
    }
    (void)package(staging, false, true);
    Store commit(installation_);
    expected(commit, revision, &trustRevision);
    if (!commit.trusted(p.publicKey))
        throw std::runtime_error("Publisher trust was revoked");
    const auto destination = commit.packagePath(id, p.digest);
    if (fs::exists(destination)) {
        const auto existing = selected(commit, id, p.digest, true);
        if (existing.publicKey != p.publicKey)
            throw std::runtime_error("Installed package signer mismatch");
    } else {
        moveExclusive(staging, destination);
        owned.published = true;
    }
    auto entries = commit.index.at("entries");
    entries[id] = {{"current", p.digest}, {"history", histories(old, p.digest)}};
    selection(commit, installation_, entries, id);
    commit.index["entries"] = entries;
    commit.saveIndex();
    auto result = ::cycode::catalogResult(commit);
    result["action"] = "installed";
    result["id"] = id;
    result["restartRequired"] = true;
    return result;
}
PluginPackages::Json PluginPackages::remove(const std::string &id, const std::string &revision) const {
    Store s(installation_);
    expected(s, revision);
    if (!packageId(id) || !s.index.at("entries").contains(id))
        throw std::runtime_error("Managed plugin is not installed");
    auto entries = s.index.at("entries");
    entries.erase(id);
    selection(s, installation_, entries, id);
    s.index["entries"] = entries;
    s.saveIndex();
    auto result = ::cycode::catalogResult(s);
    result["action"] = "removed";
    result["restartRequired"] = true;
    return result;
}
PluginPackages::Json PluginPackages::rollback(const std::string &id, const std::string &digest,
                                              const std::string &revision,
                                              const std::string &trustRevision) const {
    Store s(installation_);
    expected(s, revision, &trustRevision);
    if (!packageId(id) || !hex(digest, 64) || !s.index.at("entries").contains(id))
        throw std::runtime_error("Unknown managed plugin rollback target");
    const auto old = s.index.at("entries").at(id);
    if (std::find(old.at("history").begin(), old.at("history").end(), digest) == old.at("history").end())
        throw std::runtime_error("Rollback target is not retained history");
    const auto p = selected(s, id, digest, true), current = selected(s, id, old.at("current"));
    if (!s.trusted(p.publicKey) || p.publicKey != current.publicKey)
        throw std::runtime_error("Rollback publisher is not trusted or differs from current publisher");
    compatible(p);
    auto entries = s.index.at("entries");
    entries[id] = {{"current", digest}, {"history", histories(old, digest)}};
    selection(s, installation_, entries, id);
    s.index["entries"] = entries;
    s.saveIndex();
    auto result = ::cycode::catalogResult(s);
    result["action"] = "rolledBack";
    result["restartRequired"] = true;
    return result;
}
PluginPackages::Json PluginPackages::trust(const std::string &key, const std::string &label, bool enabled,
                                           const std::string &revision) const {
    Store s(installation_);
    if (revision != s.trustRevision())
        throw std::runtime_error("Publisher store changed; refresh before editing");
    if (!hex(key, 32))
        throw std::runtime_error("Invalid publisher key");
    s.trust["publishers"][key] = {{"label", label}, {"enabled", enabled}};
    s.saveTrust();
    auto result = ::cycode::catalogResult(s);
    result["action"] = enabled ? "publisherTrusted" : "publisherRevoked";
    result["restartRequired"] = true;
    return result;
}
PluginPackages::Json PluginPackages::clean(const std::string &id, const std::string &digest,
                                           const std::string &revision) const {
    Store s(installation_);
    expected(s, revision);
    const auto path = s.packagePath(id, digest);
    for (const auto &[name, entry] : s.index.at("entries").items())
        if (name == id && (entry.at("current") == digest ||
                           std::find(entry.at("history").begin(), entry.at("history").end(), digest) !=
                               entry.at("history").end()))
            throw std::runtime_error("Cannot delete a selected or rollback-retained plugin package");
    NativeFileLease lease(s.leasePath(digest));
    if (!lease.acquired()) {
        auto result = ::cycode::catalogResult(s);
        result["action"] = "inUse";
        return result;
    }
    const auto p = selected(s, id, digest, true);
    std::set<fs::path> directories;
    for (const auto &file : p.document.at("files")) {
        auto parent = utf8Path(file.at("path")).parent_path();
        while (!parent.empty()) {
            directories.insert(parent);
            parent = parent.parent_path();
        }
    }
    for (const auto &file : p.document.at("files"))
        fs::remove(join(path, file.at("path")));
    for (const auto *name : {"manifest.json", "signature.json", "package.json"})
        fs::remove(join(path, name));
    std::vector<fs::path> ordered(directories.begin(), directories.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
        return std::distance(a.begin(), a.end()) > std::distance(b.begin(), b.end());
    });
    for (const auto &directory : ordered)
        fs::remove(join(path, pathUtf8(directory)));
    fs::remove(path);
    auto result = ::cycode::catalogResult(s);
    result["action"] = "cleaned";
    return result;
}
std::vector<ManagedPluginCandidate> PluginPackages::load(Json &errors, bool verifyPayload) const {
    std::error_code storeError;
    const auto storeStatus = fs::symlink_status(installation_ / "extensions-managed", storeError);
    if (!fs::exists(storeStatus) && !fs::is_symlink(storeStatus) &&
        (!storeError || storeError == std::errc::no_such_file_or_directory))
        return {};
    Store s(installation_, NativeFileLeaseMode::shared);
    const auto entries = s.index.at("entries"), trust = s.trust;
    std::vector<ManagedPluginCandidate> result;
    for (const auto &[id, entry] : entries.items()) {
        try {
            const auto digest = entry.at("current").get<std::string>();
            auto lease = std::make_shared<NativeFileLease>(s.leasePath(digest), NativeFileLeaseMode::shared);
            if (!lease->acquired())
                throw std::runtime_error("Plugin package is being cleaned by another instance");
            result.push_back(
                {s.packagePath(id, digest) / "manifest.json", lease, {{"id", id}, {"digest", digest}}});
        } catch (const std::exception &e) {
            errors.push_back({{"id", id}, {"state", "error"}, {"error", e.what()}});
        }
    }
    s.lease.reset();
    result.erase(
        std::remove_if(result.begin(), result.end(),
                       [&](ManagedPluginCandidate &item) {
                           try {
                               const auto p = package(item.manifest.parent_path(), verifyPayload, true);
                               if (p.plugin.at("id") != item.metadata.at("id") ||
                                   p.digest != item.metadata.at("digest").get<std::string>())
                                   throw std::runtime_error("Managed package identity mismatch");
                               if (!trust.at("publishers").contains(p.publicKey) ||
                                   !trust.at("publishers").at(p.publicKey).at("enabled").get<bool>())
                                   throw std::runtime_error("Managed plugin publisher is not trusted");
                               compatible(p);
                               item.metadata.update(summary(p));
                               item.metadata["payloadVerified"] = verifyPayload;
                               item.declaration = p.plugin;
                               return false;
                           } catch (const std::exception &e) {
                               errors.push_back(
                                   {{"id", item.metadata.at("id")}, {"state", "error"}, {"error", e.what()}});
                               return true;
                           }
                       }),
        result.end());
    return result;
}
} // namespace cycode
