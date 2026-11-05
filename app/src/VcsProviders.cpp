#include "cycode/Engine.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <algorithm>
namespace cycode {
Json Engine::versionControlProviders() const {
    Json result = Json::array(
        {{{"id", "git"},
          {"title", "Git"},
          {"builtIn", true},
          {"capabilities",
           {"status",        "init",          "inspect",       "diff",       "log",        "history",
            "show",          "blame",         "stage",         "unstage",    "hunks", "applyHunks", "commit",     "branch.create",
            "branch.switch", "branch.rename", "branch.delete", "remote.add", "remote.set", "remote.remove",
            "fetch",         "pull",          "push",          "merge",      "stash.save", "stash.apply",
            "stash.pop",     "stash.drop",    "conflicts",     "resolve", "conflict.list", "conflict.inspect", "conflict.plan", "conflict.apply", "checkout.plan", "checkout.apply", "checkout.undo", "worktrees", "worktree.add", "worktree.remove", "worktree.lock", "worktree.unlock", "worktree.plan", "worktree.move", "worktree.repair"}},
          {"actions", Json::array()}}});
    for(const auto *method:{"merge.state","merge.plan","merge.commit","merge.quit"})result[0]["capabilities"].push_back(method);
    for(const auto *method:{"replay.state","replay.plan","replay.apply"})result[0]["capabilities"].push_back(method);
    const auto description = plugins_.describe();
    for (auto provider : description.at("vcsProviders")) {
        provider["builtIn"] = false;
        result.push_back(std::move(provider));
    }
    return result;
}
Json Engine::versionControlContext(const Json &params) {
    const auto file = resolve(".cycode/vcs.json");
    if (vcsConfiguration_.is_null()) {
        Json configuration = {{"schemaVersion", 1}, {"mappings", Json::array()}};
        std::optional<std::string> baseline;
        if (std::filesystem::exists(file)) {
            baseline = readTextFileBytes(file);
            configuration = Json::parse(*baseline);
            if (configuration.value("schemaVersion", 0) != 1 || !configuration.at("mappings").is_array() ||
                configuration.at("mappings").size() > 256)
                throw std::runtime_error("Invalid version control configuration");
            for (const auto &mapping : configuration.at("mappings")) {
                const auto root = resolve(mapping.at("root"));
                if (!std::filesystem::is_directory(root) || !mapping.at("provider").is_string())
                    throw std::runtime_error("Invalid version control mapping");
            }
        }
        vcsBaseline_ = std::move(baseline);
        vcsConfiguration_ = std::move(configuration);
    }
    const auto path = resolve(params.value("contextPath", params.value("path", std::string("."))));
    std::string selected = "git", selectedRoot = ".";
    size_t depth = 0;
    for (const auto &mapping : vcsConfiguration_.at("mappings")) {
        const auto root = resolve(mapping.at("root"));
        auto a = root.begin(), b = path.begin();
        bool inside = true;
        for (; a != root.end(); ++a, ++b) {
            if (b == path.end()) {
                inside = false;
                break;
            }
#ifdef _WIN32
            if (_wcsicmp(a->c_str(), b->c_str())) {
                inside = false;
                break;
            }
#else
            if (*a != *b) {
                inside = false;
                break;
            }
#endif
        }
        const auto count = size_t(std::distance(root.begin(), root.end()));
        if (inside && count >= depth) {
            depth = count;
            selected = mapping.at("provider");
            selectedRoot = pathUtf8(root.lexically_relative(root_));
        }
    }
    if (params.contains("provider"))
        selected = params.at("provider").get<std::string>();
    Json descriptor = {
        {"id", selected}, {"title", selected}, {"capabilities", Json::array()}, {"actions", Json::array()}};
    bool available = false;
    for (const auto &candidate : versionControlProviders())
        if (candidate.at("id") == selected) {
            descriptor = candidate;
            available = true;
            break;
        }
    descriptor["provider"] = selected;
    descriptor["root"] = selectedRoot;
    descriptor["available"] = available;
    descriptor["mappings"] = vcsConfiguration_.at("mappings");
    descriptor["revision"] = vcsConfigurationRevision_;
    if (selected == "git") {
        const auto requested=params.contains("repository")?resolve(params.at("repository")):path;
        if(params.contains("repository")&&!std::filesystem::is_directory(requested))
            throw std::runtime_error("Selected Git repository folder is unavailable");
        const auto selection=gitRepositoryAt(root_,requested);
        descriptor["gitRepository"]=selection.root;
        descriptor["gitIdentity"]=selection.identity;
    }
    return descriptor;
}
Json Engine::versionControl(const std::string &method, const Json &params) {
    std::lock_guard vcs(vcsMutex_);
    if (method == "vcs.providers")
        return {{"providers", versionControlProviders()}};
    if (method == "vcs.repositories") return discoverGitRepositories(root_);
    if (method.starts_with("vcs.replay.") || method.starts_with("vcs.merge.") || method.starts_with("vcs.checkout.") || method.starts_with("vcs.conflict.") || method=="vcs.submodules" || method.starts_with("vcs.submodule.") || method=="vcs.worktrees" || method.starts_with("vcs.worktree.")) {
        if (params.contains("provider") && params.at("provider")!="git")
            throw std::runtime_error("Typed Git workspace operations require the built-in Git provider");
        return git(method,params);
    }
    auto context = versionControlContext(params);
    if (method == "vcs.context")
        return context;
    if (method == "vcs.providerAction") {
        const auto action = params.at("action").get<std::string>();
        if (context.at("provider") == "git")
            throw std::runtime_error("Use the typed Git operations for the built-in provider");
        const auto caps = context.at("capabilities");
        if (std::find(caps.begin(), caps.end(), action) == caps.end())
            throw std::runtime_error("Unsupported provider action");
        auto input = params.value("input", Json::object());
        if (!input.is_object())
            throw std::runtime_error("Provider input must be an object");
        input["provider"] = context.at("provider");
        input["contextPath"] = params.value("contextPath", std::string("."));
        return versionControl("vcs." + action, input);
    }
    if (method == "vcs.configure") {
        if (params.value("expectedRevision", vcsConfigurationRevision_) != vcsConfigurationRevision_)
            throw std::runtime_error("RevisionConflict: version control configuration changed");
        const std::string provider = params.at("provider");
        const auto providers = versionControlProviders();
        if (std::none_of(providers.begin(), providers.end(),
                         [&](const auto &p) { return p.at("id") == provider; }))
            throw std::runtime_error("Version control provider is unavailable");
        const auto full = resolve(params.value("root", std::string(".")));
        if (!std::filesystem::is_directory(full))
            throw std::runtime_error("Version control mapping root must be a directory");
        const auto root = pathUtf8(full.lexically_relative(root_));
        auto next = vcsConfiguration_;
        auto &mappings = next.at("mappings");
        for (auto it = mappings.begin(); it != mappings.end();) {
            if (resolve(it->at("root")) == full)
                it = mappings.erase(it);
            else
                ++it;
        }
        if (mappings.size() >= 256)
            throw std::runtime_error("Version control mapping limit reached");
        mappings.push_back({{"root", root}, {"provider", provider}});
        const auto file = resolve(".cycode/vcs.json");
        std::filesystem::create_directories(file.parent_path());
        const auto bytes = next.dump(2) + "\n";
        writeTextFileAtomic(file, bytes, vcsBaseline_);
        vcsBaseline_ = bytes;
        vcsConfiguration_ = std::move(next);
        ++vcsConfigurationRevision_;
        return versionControlContext({{"path", root}});
    }
    const std::string provider = context.at("provider");
    if (!context.at("available").get<bool>())
        throw std::runtime_error("Configured version control provider is unavailable: " + provider);
    auto actual = params;
    actual["root"] = context.at("root");
    actual["workspace"] = pathUtf8(root_);
    if (provider != "git") {
        if (params.contains("path")) {
            const auto path = resolve(params.at("path"));
            if (!containsPath(resolve(context.at("root")), path))
                throw std::runtime_error("Path is outside the selected version control root");
            actual["path"] = pathUtf8(path.lexically_relative(root_));
        }
        for (const auto &action : context.at("actions"))
            if (action.at("id") == method.substr(4)) {
                for (const auto &field : action.value("fields", Json::array())) {
                    const std::string key = field.at("id");
                    if (!actual.contains(key) && field.contains("default"))
                        actual[key] = field.at("default");
                    if (actual.contains(key) && !actual.at(key).is_string())
                        throw std::runtime_error("Provider action fields must be text");
                    if (field.value("required", false) &&
                        (!actual.contains(key) || actual.at(key).get_ref<const std::string &>().empty()))
                        throw std::runtime_error("Provider action field is required: " + key);
                }
            }
        const std::set<std::string> reads = {"status", "inspect", "diff", "log", "history", "show", "blame"};
        const bool mutation = !reads.contains(method.substr(4));
        if (mutation) {
            // Read the live baseline under the document lock, but release it
            // before invoking external/native code. Reconciliation preserves
            // buffers edited concurrently while a provider command is running.
            std::lock_guard lock(mutex_);
            for (const auto &[path, document] : documents_) {
                if (document.dirty)
                    throw std::runtime_error("Save modified buffers before changing version control state");
                if (!document.diskConflict.empty() || !std::filesystem::is_regular_file(resolve(path)) ||
                    readTextFileBytes(resolve(path)) != document.disk)
                    throw std::runtime_error("Reconcile open files with disk before changing version control state");
            }
        }
        auto result = plugins_.vcs(provider, method.substr(4), actual);
        if (!result.is_object())
            throw std::runtime_error("Provider result must be an object");
        if (method == "vcs.status") {
            if (!result.at("repository").is_boolean() || !result.at("branch").is_string() ||
                !result.at("provider").is_string() || !result.value("message", Json("")).is_string() ||
                !result.at("files").is_array() || result.at("files").size() > 20000)
                throw std::runtime_error("Invalid provider status result");
            for (const auto &entry : result.at("files")) {
                if (!entry.is_object() || !entry.at("status").is_string() || !entry.at("path").is_string())
                    throw std::runtime_error("Invalid provider file status");
                resolve(entry.at("path"));
            }
        }
        if (mutation)
            refreshGitDocuments(result);
        return result;
    }
    actual["repository"] = method=="vcs.init" ? params.value("repository",context.at("gitRepository").get<std::string>()) : context.at("gitRepository").get<std::string>();
    return git(method, actual);
}
} // namespace cycode
