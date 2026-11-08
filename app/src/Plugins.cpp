#include "cycode/Plugins.h"
#include "cycode/DebugAdapters.h"
#include "cycode/Events.h"
#include "cycode/PluginManifest.h"
#include "cycode/Commands.h"
#include "cycode/TextFile.h"
#include "cycode/TemplateDefinitions.h"
#include "cycode/Languages.h"
#include "cycode/SnippetCatalogue.h"
#include "cycode/FileTransactions.h"
#include "PluginPackageData.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace cycode {
namespace {
using Json = nlohmann::json;
thread_local Plugins *callbackHost = nullptr;
struct ProcessPackageSelection {
    std::vector<ManagedPluginCandidate> candidates;
    Json errors=Json::array();
};
std::shared_ptr<const ProcessPackageSelection> packageSelection(const std::filesystem::path &installation) {
    static std::mutex mutex;
    static std::map<std::string,std::shared_ptr<const ProcessPackageSelection>> selections;
    std::lock_guard lock(mutex);
    const auto key=directoryIdentity(std::filesystem::canonical(installation));
    if(const auto found=selections.find(key);found!=selections.end())return found->second;
    auto selected=std::make_shared<ProcessPackageSelection>();
    try{selected->candidates=PluginPackages(installation).load(selected->errors,false);}
    catch(const std::exception &e){selected->errors.push_back({{"id","managed-packages"},{"state","error"},{"error",e.what()}});}
    selections.emplace(key,selected);return selected;
}
bool identifier(const std::string &id) {
    return pluginIdentifier(id);
}
Json parse(const char *text) {
    if (!text)
        throw std::runtime_error("Plugin returned null");
    size_t size = 0;
    while (size <= 4 * 1024 * 1024 && text[size])
        ++size;
    if (size > 4 * 1024 * 1024)
        throw std::runtime_error("Plugin JSON exceeds 4 MiB");
    return Json::parse(text, text + size);
}
void unload(void *library) {
    if (!library)
        return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}
bool visible(const Json &when, const Json &context) {
    for (auto it = when.begin(); it != when.end(); ++it) {
        if (!context.contains(it.key()))
            return false;
        if (it.value().is_array()) {
            if (std::find(it.value().begin(), it.value().end(), context.at(it.key())) == it.value().end())
                return false;
        } else if (context.at(it.key()) != it.value())
            return false;
    }
    return true;
}
} // namespace
std::filesystem::path Plugins::statePath() const {
    if (workspace_.empty())
        throw std::runtime_error("Plugin workspace is unavailable");
    const auto path = std::filesystem::weakly_canonical(workspace_ / ".cycode/plugins.json");
    auto b = path.begin();
    for (auto a = workspace_.begin(); a != workspace_.end(); ++a, ++b) {
        if (b == path.end())
            throw std::runtime_error("Plugin configuration escapes workspace");
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str()))
            throw std::runtime_error("Plugin configuration escapes workspace");
#else
        if (*a != *b)
            throw std::runtime_error("Plugin configuration escapes workspace");
#endif
    }
    return path;
}
void Plugins::workspace(const std::filesystem::path &path, HostRequest host) {
    std::lock_guard lock(mutex_);
    if (!records_.empty())
        throw std::runtime_error("Cannot replace a loaded plugin host workspace");
    workspace_ = std::filesystem::canonical(path);
    host_ = std::move(host);
    try {
        const auto file = statePath();
        if (std::filesystem::exists(file)) {
            baseline_ = readTextFileBytes(file);
            state_ = Json::parse(*baseline_);
            if (!state_.is_object() || state_.value("schemaVersion", 0) != 1 ||
                !state_.at("enabled").is_object())
                throw std::runtime_error("Invalid plugin configuration");
            for (auto it = state_.at("enabled").begin(); it != state_.at("enabled").end(); ++it)
                if (!identifier(it.key()) || !it.value().is_boolean())
                    throw std::runtime_error("Invalid plugin activation entry");
        }
    } catch (const std::exception &e) {
        configurationValid_ = false;
        errors_.push_back({{"state", "configuration-error"}, {"error", e.what()}});
    }
}
bool Plugins::active(const std::string &id) const {
    const auto it = records_.find(id);
    if (it == records_.end() || !it->second.loaded || !it->second.enabled)
        return false;
    // Required dependencies have already been topologically ordered at load.
    for (const auto &d : it->second.info.value("dependencies", Json::array()))
        if (!d.value("optional", false) && !active(d.at("id")))
            return false;
    return true;
}
void Plugins::add(const std::string &kind, Json info, CyCodeCommand callback, CyCodeCommandV2 callbackV2,
                  bool legacy) {
    if (registering_.empty() || !info.is_object())
        throw std::runtime_error("Register contributions only during initialization");
    const std::string id = info.at("id");
    if (!identifier(id) || (!legacy && !id.starts_with(registering_ + ".")))
        throw std::runtime_error("Contribution ID must use the plugin namespace");
    if (kind != "command" && kind != "menu" && kind != "panel" && kind != "vcs" && kind != "project" &&
        kind != "syntax" && kind!="template" && kind!="languageServer" && kind!="events" && kind!="debugAdapter" && kind!="snippets")
        throw std::runtime_error("Unsupported contribution kind");
    if (contributions_.size() >= 2048)
        throw std::runtime_error("Contribution limit reached");
    for (const auto &c : contributions_)
        if (c.info.at("id") == id)
            throw std::runtime_error("Duplicate contribution ID: " + id);
    if (kind == "command" && findCommand(id))
        throw std::runtime_error("Cannot replace a built-in command");
    if (kind != "menu" && kind != "syntax" && kind!="template" && kind!="languageServer" && kind!="debugAdapter" && kind!="snippets" && !callback && !callbackV2)
        throw std::runtime_error("Contribution needs a callback");
    if (kind == "vcs" && id == "git")
        throw std::runtime_error("Git is a reserved built-in provider");
    if(kind=="debugAdapter") {
        if(callback||callbackV2||info.contains("when")||info.contains("context"))throw std::runtime_error("Debug adapters are unconditional declarative contributions");
        const auto permissions=records_.at(registering_).info.value("permissions",Json::array());
        if(std::find(permissions.begin(),permissions.end(),"process.execute")==permissions.end())throw std::runtime_error("Debug adapters require process.execute");
        size_t count=1;for(const auto &c:contributions_)if(c.kind==kind)++count;
        if(count>32)throw std::runtime_error("At most 32 plugin debug adapters are supported");
        info=expandDebugAdapter(std::move(info),utf8Path(records_.at(registering_).info.at("manifest")).parent_path(),workspace_);
    }
    if(kind=="events") {
        if(!callbackV2 || info.contains("when") || info.contains("context"))throw std::runtime_error("Event subscriptions require an unconditional ABI2 callback");
        size_t count=1;for(const auto &c:contributions_)if(c.kind=="events")++count;
        if(count>32)throw std::runtime_error("At most 32 event subscriptions are supported");
        const auto types=info.value("types",Json::array());
        if(!types.is_array() || types.size()>16)throw std::runtime_error("Invalid subscription filters");
        for(const auto &v:types){const auto type=v.get<std::string>();if(type.empty() || type.size()>64 || type.find('\0')!=std::string::npos || (type.find('*')!=std::string::npos && type.find('*')!=type.size()-1))throw std::runtime_error("Invalid event subscription type");}
        const auto batch=info.value("maxBatch",Json(32));
        if(!batch.is_number_integer() || batch<1 || batch>32)throw std::runtime_error("Subscription maxBatch must be 1 through 32");
    }
    if(kind=="languageServer") {
        if(callback||callbackV2||info.contains("when"))throw std::runtime_error("Language servers use unconditional declarative contributions");
        const auto permissions=records_.at(registering_).info.value("permissions",Json::array());
        if(std::find(permissions.begin(),permissions.end(),"process.execute")==permissions.end())throw std::runtime_error("Language servers require process.execute");
        validateLanguageServer(info);size_t count=1;
        for(const auto &c:contributions_)if(c.kind==kind){++count;
            auto overlaps=[&](const Json &a,const Json &b){for(const auto &l:a.at("languages"))for(const auto *key:{"extensions","filenames"})for(const auto &v:l.value(key,Json::array()))if(!contributedLanguageId(b,std::string(key)=="extensions"?"probe"+v.get<std::string>():v.get<std::string>()).empty())return true;return false;};
            if(overlaps(info,c.info)||overlaps(c.info,info))throw std::runtime_error("Plugin language selectors overlap");
        }
        if(count>32)throw std::runtime_error("At most 32 plugin language servers are supported");
        for(auto &v:info["arguments"]){auto arg=v.get<std::string>();
            for(const auto &[token,value]:std::map<std::string,std::string>{{"${pluginDir}",pathUtf8(utf8Path(records_.at(registering_).info.at("manifest")).parent_path())},{"${workspace}",pathUtf8(workspace_)}})
                for(size_t pos=0;(pos=arg.find(token,pos))!=std::string::npos;pos+=value.size())arg.replace(pos,token.size(),value);
            v=std::move(arg);
        }
        validateLanguageServer(info);
    }
    if(kind=="template") {
        if(callback||callbackV2)throw std::runtime_error("Project templates are declarative and cannot have callbacks");
        validateProjectTemplate(info);size_t count=1,bytes=info.dump().size();
        for(const auto &c:contributions_)if(c.kind=="template"){++count;bytes+=c.info.dump().size();}
        if(count>32||bytes>1024*1024)throw std::runtime_error("Project template registry exceeds 32 templates / 1 MiB");
    }
    if(kind=="snippets") {
        if(callback||callbackV2||info.contains("plugin"))
            throw std::runtime_error("Snippet catalogues are declarative; the host supplies their owner");
        validateSnippetContribution(info);
        size_t count=1,bytes=info.dump().size()+registering_.size()+12;
        for(const auto &c:contributions_)if(c.kind=="snippets"){
            ++count;bytes+=c.info.dump().size()+c.owner.size()+12;
        }
        if(!snippetRegistryFits(count,bytes)) throw std::runtime_error("Plugin snippets exceed 32 catalogues / 1 MiB");
    }
    for (const auto *key : {"title", "category", "context"})
        if (info.contains(key) && !info.at(key).is_string())
            throw std::runtime_error("Invalid contribution text field");
    if (info.contains("context")) {
        const auto scope = info.at("context");
        if (scope != "global" && scope != "document" && scope != "editor")
            throw std::runtime_error("Invalid command context");
    }
    if (info.contains("order") && !info.at("order").is_number_integer())
        throw std::runtime_error("Invalid contribution order");
    if (kind == "menu") {
        const std::set<std::string> locations = {"file", "edit", "view",       "navigate", "build",
                                                 "run",  "vcs",  "extensions", "editor",   "project"};
        if (!info.at("command").is_string() || !locations.contains(info.at("location")))
            throw std::runtime_error("Invalid menu location/command");
    }
    if (kind == "syntax") {
        if (!info.at("name").is_string() || info.at("name").get_ref<const std::string &>().size() > 256)
            throw std::runtime_error("Invalid syntax name");
        for (const auto *key : {"extensions", "filenames", "wordLists"})
            if (info.contains(key)) {
                const auto &values = info.at(key);
                if (!values.is_array() || values.size() > 128)
                    throw std::runtime_error("Invalid syntax list");
                for (const auto &v : values)
                    if (!v.is_string() || v.get_ref<const std::string &>().size() > 65536)
                        throw std::runtime_error("Invalid syntax list entry");
            }
        if (info.contains("base") && !info.at("base").is_string())
            throw std::runtime_error("Invalid syntax base");
        if (info.contains("rules")) {
            const auto &rules = info.at("rules");
            if (!rules.is_object())
                throw std::runtime_error("Invalid syntax rules");
            for (const auto *key : {"keywords", "types", "functions"})
                if (rules.contains(key)) {
                    if (!rules.at(key).is_array() || rules.at(key).size() > 10000)
                        throw std::runtime_error("Invalid syntax words");
                    for (const auto &v : rules.at(key))
                        if (!v.is_string() || v.get_ref<const std::string &>().size() > 256)
                            throw std::runtime_error("Invalid syntax word");
                }
            for (const auto *key :
                 {"lineComment", "blockCommentStart", "blockCommentEnd", "quotes", "escape"})
                if (rules.contains(key) &&
                    (!rules.at(key).is_string() || rules.at(key).get_ref<const std::string &>().size() > 32))
                    throw std::runtime_error("Invalid syntax delimiter");
            if (rules.contains("caseSensitive") && !rules.at("caseSensitive").is_boolean())
                throw std::runtime_error("Invalid syntax case sensitivity");
            if (rules.contains("escape") && rules.at("escape").get_ref<const std::string &>().size() != 1)
                throw std::runtime_error("Syntax escape must be one byte");
        }
    }
    if (kind == "vcs" || kind == "project") {
        if (!info.at("capabilities").is_array())
            throw std::runtime_error("Provider needs capabilities");
        for (const auto &cap : info.at("capabilities"))
            if (!cap.is_string() || cap.get<std::string>().size() > 100)
                throw std::runtime_error("Invalid provider capability");
        std::set<std::string> actionIds;
        for (const auto &action : info.value("actions", Json::array())) {
            if (!action.is_object() || !identifier(action.at("id")) || !action.at("title").is_string())
                throw std::runtime_error("Invalid provider action");
            if (!actionIds.insert(action.at("id")).second)
                throw std::runtime_error("Duplicate provider action");
            if (action.contains("scope") && action.at("scope") != "file" &&
                action.at("scope") != "repository")
                throw std::runtime_error("Invalid action scope");
            const auto &caps = info.at("capabilities");
            if (std::find(caps.begin(), caps.end(), action.at("id")) == caps.end())
                throw std::runtime_error("Action is not a provider capability");
            if (!action.value("fields", Json::array()).is_array() ||
                action.value("fields", Json::array()).size() > 32)
                throw std::runtime_error("Invalid action fields");
            std::set<std::string> fieldIds;
            for (const auto &field : action.value("fields", Json::array())) {
                if (!field.is_object() || !identifier(field.at("id")) ||
                    !fieldIds.insert(field.at("id")).second || !field.at("title").is_string())
                    throw std::runtime_error("Invalid action field");
                if (field.contains("default") && !field.at("default").is_string())
                    throw std::runtime_error("Action field default must be text");
                if (field.contains("required") && !field.at("required").is_boolean())
                    throw std::runtime_error("Invalid required action field");
            }
        }
    }
    if (info.contains("shortcuts")) {
        if (kind != "command" || !info.at("shortcuts").is_array())
            throw std::runtime_error("Invalid command shortcuts");
        for (const auto &s : info.at("shortcuts"))
            Keymap::parse(s);
    }
    if (info.contains("when") && !info.at("when").is_object())
        throw std::runtime_error("Invalid contribution context");
    if (info.value("title", std::string{}).size() > 256)
        throw std::runtime_error("Contribution title too long");
    contributions_.push_back({registering_, kind, std::move(info), callback, callbackV2});
}
void Plugins::addCommand(void *ctx, const char *id, const char *title, CyCodeCommand callback) {
    auto &self = *static_cast<Plugins *>(ctx);
    try {
        if (!id || !title)
            throw std::runtime_error("Missing command descriptor");
        self.add("command", {{"id", id}, {"title", title}}, callback, nullptr, true);
    } catch (const std::exception &e) {
        self.registrationError_ = e.what();
    }
}
void Plugins::addVcs(void *ctx, const char *id, const char *title, const char *caps, CyCodeCommand callback) {
    auto &self = *static_cast<Plugins *>(ctx);
    try {
        if (!id || !title)
            throw std::runtime_error("Missing provider descriptor");
        self.add("vcs", {{"id", id}, {"title", title}, {"capabilities", parse(caps)}}, callback, nullptr,
                 true);
    } catch (const std::exception &e) {
        self.registrationError_ = e.what();
    }
}
int Plugins::contribute(void *ctx, const char *kind, const char *info, CyCodeCommandV2 callback) {
    auto &self = *static_cast<Plugins *>(ctx);
    try {
        if (!kind)
            throw std::runtime_error("Missing contribution kind");
        self.add(kind, parse(info), nullptr, callback);
        return 0;
    } catch (const std::exception &e) {
        self.registrationError_ = e.what();
        return 1;
    }
}
const char *Plugins::request(void *ctx, const char *method, const char *parameters) {
    auto &self = *static_cast<Plugins *>(ctx);
    try {
        if (callbackHost != &self)
            return "{\"ok\":false,\"error\":\"Host requests require the callback thread\"}";
        if (!method || self.invoking_.empty() || !self.host_)
            throw std::runtime_error("Host requests require a plugin callback");
        const std::string service(method);
        const std::map<std::string, std::string> allowed = {{"workspace.describe", ""},
                                                            {"vcs.submodules", ""},
                                                            {"vcs.worktrees", ""},
                                                            {"vcs.worktree.plan", ""},
                                                            {"vcs.conflict.list", ""},
                                                            {"vcs.conflict.inspect", ""},
                                                            {"vcs.conflict.plan", ""},
                                                            {"vcs.conflict.apply", "process.execute"},
                                                            {"vcs.checkout.plan", ""},
                                                            {"vcs.checkout.apply", "process.execute"},
                                                            {"vcs.checkout.undo", "process.execute"},
                                                            {"vcs.merge.state", ""},
                                                            {"vcs.replay.state", ""},
                                                            {"vcs.replay.plan", ""},
                                                            {"vcs.replay.apply", "process.execute"},
                                                            {"vcs.merge.plan", ""},
                                                            {"vcs.merge.commit", "process.execute"},
                                                            {"vcs.merge.quit", "process.execute"},
                                                            {"vcs.worktree.move", "process.execute"},
                                                            {"vcs.worktree.repair", "process.execute"},
                                                            {"vcs.worktree.add", "process.execute"},
                                                            {"vcs.worktree.remove", "process.execute"},
                                                            {"vcs.worktree.lock", "vcs.write"},
                                                            {"vcs.worktree.unlock", "vcs.write"},
                                                            {"vcs.submodule.init", "process.execute"},
                                                            {"vcs.submodule.sync", "process.execute"},
                                                            {"vcs.submodule.update", "process.execute"},
                                                            {"vcs.submodule.addPlan", ""},
                                                            {"vcs.submodule.add", "process.execute"},
                                                            {"vcs.submodule.deinitPlan", ""},
                                                            {"vcs.submodule.deinit", "process.execute"},
                                                            {"vcs.submodule.settingsPlan", ""},
                                                            {"vcs.submodule.settings", "process.execute"},
                                                            {"build.status", ""},
                                                            {"build.output", "process.execute"},
                                                            {"build.location", ""},
                                                            {"project.status", ""},
                                                            {"project.output", "process.execute"},
                                                            {"build.inspect", ""},
                                                            {"build.presets", ""},
                                                            {"build.presetPlan", ""},
                                                            {"build.presetRun", "process.execute"},
                                                            {"build.configure", "process.execute"},
                                                            {"build.run", "process.execute"},
                                                            {"build.cancel", "process.execute"},
                                                            {"msbuild.inspect", ""},
                                                            {"msbuild.build", "process.execute"},
                                                            {"msbuild.rebuild", "process.execute"},
                                                            {"msbuild.clean", "process.execute"},
                                                            {"msbuild.restore", "process.execute"},
                                                            {"msbuild.evaluate", "process.execute"},
                                                            {"msbuild.prepare", "process.execute"},
                                                            {"project.inspect", ""},
                                                            {"project.configure", "process.execute"},
                                                            {"project.build", "process.execute"},
                                                            {"project.test", "process.execute"},
                                                            {"project.install", "process.execute"},
                                                            {"project.script", "process.execute"},
                                                            {"project.launch", "process.execute"},
                                                            {"project.debug", "process.execute"},
                                                            {"project.cancel", "process.execute"},
                                                            {"tests.discover", "process.execute"},
                                                            {"tests.run", "process.execute"},
                                                            {"tests.cancel", "process.execute"},
                                                            {"language.status", ""},
                                                            {"language.preferences", ""},
                                                            {"language.preferencesForget", "process.execute"},
                                                            {"language.configure", "process.execute"},
                                                            {"language.complete", ""},
                                                            {"snippets.complete", ""},
                                                            {"snippets.userRead", ""},
                                                            {"snippets.userDraft", ""},
                                                            {"snippets.userSave", "settings.write"},
                                                            {"language.completionResolve", ""},
                                                            {"language.completionPrepare", ""},
                                                            {"language.hover", ""},
                                                            {"language.signature", ""},
                                                            {"language.definition", ""},
                                                            {"language.declaration", ""},
                                                            {"language.typeDefinition", ""},
                                                            {"language.implementation", ""},
                                                            {"language.documentHighlights", ""},
                                                            {"language.references", ""},
                                                            {"language.hierarchyPrepare", ""},
                                                            {"language.hierarchyExpand", ""},
                                                            {"language.hierarchyStatus", ""},
                                                            {"language.symbols", ""},
                                                            {"language.symbolResolve", ""},
                                                            {"language.documentSymbols", ""},
                                                            {"language.highlight", ""},
                                                            {"language.codeActions", ""},
                                                            {"language.codeActionPrepare", ""},
                                                            {"language.rename", ""},
                                                            {"language.format", ""},
                                                            {"changes.preview", ""},
                                                            {"changes.apply", "workspace.write"},
                                                            {"changes.undo", "workspace.write"},
                                                            {"templates.list", ""},
                                                            {"templates.preview", ""},
                                                            {"templates.create", "workspace.write"},
                                                            {"workspace.files", ""},
                                                            {"workspace.counterparts", ""},
                                                              {"workspace.include", ""},
                                                            {"events.read", ""},
                                                            {"debug.adapters", ""},
                                                            {"debug.profile", "process.execute"},
                                                            {"debug.status", "process.execute"},
                                                            {"debug.configure", "process.execute"},
                                                            {"debug.launch", "process.execute"},
                                                            {"debug.attach", "process.execute"},
                                                            {"debug.stop", "process.execute"},
                                                            {"debug.pause", "process.execute"},
                                                            {"debug.dataBreakpointInfo", "process.execute"},
                                                            {"debug.dataBreakpoints", "process.execute"},
                                                            {"debug.instructionBreakpoints", "process.execute"},
                                                            {"debug.hotReload", "process.execute"},
                                                            {"debug.cancelReload", "process.execute"},
                                                            {"debug.continue", "process.execute"},
                                                            {"debug.next", "process.execute"},
                                                            {"debug.stepIn", "process.execute"},
                                                            {"debug.stepOut", "process.execute"},
                                                            {"debug.stepBack", "process.execute"},
                                                            {"debug.reverseContinue", "process.execute"},
                                                            {"debug.restartFrame", "process.execute"},
                                                            {"debug.stepInTargets", "process.execute"},
                                                            {"debug.stepIntoTarget", "process.execute"},
                                                            {"debug.gotoTargets", "process.execute"},
                                                            {"debug.gotoTarget", "process.execute"},
                                                            {"debug.runToCursor", "process.execute"},
                                                            {"debug.restoreRunToCursor", "process.execute"},
                                                            {"debug.breakpoints", "process.execute"},
                                                            {"debug.evaluate", "process.execute"},
                                                            {"debug.variables", "process.execute"},
                                                            {"debug.memory", "process.execute"},
                                                            {"debug.setVariable", "process.execute"},
                                                            {"debug.setExpression", "process.execute"},
                                                            {"debug.writeMemory", "process.execute"},
                                                            {"debug.disassemble", "process.execute"},
                                                            {"debug.exceptionInfo", "process.execute"},
                                                            {"debug.functionBreakpoints", "process.execute"},
                                                            {"debug.exceptionBreakpoints", "process.execute"},
                                                            {"debug.frame", "process.execute"},
                                                            {"debug.thread", "process.execute"},
                                                            {"debug.source", "process.execute"},
                                                            {"resources.preview", ""},
                                                            {"resources.trash", ""},
                                                            {"resources.children", ""},
                                                            {"resources.events", ""},
                                                            {"resources.apply", "workspace.write"},
                                                            {"resources.undo", "workspace.write"},
                                                            {"documents.list", ""},
                                                            {"documents.read", ""},
                                                            {"documents.create", "workspace.write"},
                                                            {"documents.createUntitled", "workspace.write"},
                                                            {"documents.replace", "workspace.write"},
                                                            {"documents.save", "workspace.write"},
                                                            {"documents.saveAs", "workspace.write"},
                                                            {"documents.close", "workspace.write"},
                                                            {"unreal.inspect", ""},
                                                            {"unreal.artifacts", ""},
                                                            {"unreal.launchProfile", "process.execute"},
                                                            {"unreal.automationPlan", ""},
                                                            {"unreal.automationRun", "process.execute"},
                                                            {"unreal.generate", "process.execute"},
                                                            {"unreal.build", "process.execute"},
                                                            {"unreal.database", "process.execute"},
                                                            {"unreal.prepare", "process.execute"}};
        auto it = allowed.find(service);
        if (it == allowed.end())
            throw std::runtime_error("Host service is not exported to plugins");
        if (!it->second.empty()) {
            if (!self.hostMutationAllowed_ && service!="debug.profile" && service!="debug.status" && service!="unreal.launchProfile" && service!="build.output" && service!="project.output")
                throw std::runtime_error("Read-only provider callbacks cannot mutate host state");
            const auto permissions =
                self.records_.at(self.invoking_).info.value("permissions", Json::array());
            if (std::find(permissions.begin(), permissions.end(), it->second) == permissions.end())
                throw std::runtime_error("Plugin permission is missing: " + it->second);
            if((service=="debug.configure"||service=="debug.breakpoints"||service=="debug.functionBreakpoints"||service=="debug.exceptionBreakpoints")&&std::find(permissions.begin(),permissions.end(),"workspace.write")==permissions.end())throw std::runtime_error("Plugin permission is missing: workspace.write");
            if(service=="unreal.automationRun"&&std::find(permissions.begin(),permissions.end(),"workspace.write")==permissions.end())throw std::runtime_error("Plugin permission is missing: workspace.write");
            if(service.starts_with("vcs.submodule.")||((service.starts_with("vcs.worktree.")||service.starts_with("vcs.checkout.")||service.starts_with("vcs.merge.")||service.starts_with("vcs.replay.")||service=="vcs.conflict.apply")&&it->second=="process.execute")) for(const auto *permission:{"workspace.write","vcs.write"})
                if(std::find(permissions.begin(),permissions.end(),permission)==permissions.end())throw std::runtime_error(std::string("Plugin permission is missing: ")+permission);
        }
        self.response_ = Json({{"ok", true}, {"result", self.host_(service, parse(parameters))}}).dump();
    } catch (const std::exception &e) {
        self.response_ = Json({{"ok", false}, {"error", e.what()}}).dump();
    }
    return self.response_.c_str();
}
void Plugins::load(const std::filesystem::path &directory) {
    loadDirectories({directory});
}
void Plugins::loadInstalled(const std::filesystem::path &installation) {
    std::lock_guard lock(mutex_);
    const auto selected=packageSelection(installation);
    for(const auto &error:selected->errors)errors_.push_back(error);
    loadDirectories({installation / "runtime/plugins", installation / "extensions"}, selected->candidates);
}
void Plugins::loadDirectories(const std::vector<std::filesystem::path> &directories, const std::vector<ManagedPluginCandidate> &managed) {
    std::lock_guard lock(mutex_);
    hostV1_ = {1, sizeof(CyCodeHostV1), this, addCommand, addVcs};
    hostV2_ = {2, sizeof(CyCodeHostV2), this, contribute, request, CYCODE_HOST_FEATURE_ACTIVATION_V2 | CYCODE_HOST_FEATURE_EVENTS_V2 | CYCODE_HOST_FEATURE_EVENT_SUBSCRIPTIONS_V2 | CYCODE_HOST_FEATURE_DEBUG_ADAPTERS_V2 | CYCODE_HOST_FEATURE_SNIPPETS_V2};
    std::map<std::string, Json> pending;
    std::vector<std::filesystem::path> manifests;
    for (const auto &directory : directories) {
        if (!std::filesystem::is_directory(directory))
            continue;
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() != ".json")
                continue;
            manifests.push_back(entry.path());
        }
    }
    std::map<std::filesystem::path,const ManagedPluginCandidate *> packages;
    for(const auto &candidate:managed){manifests.push_back(candidate.manifest);packages[candidate.manifest]=&candidate;}
    for(const auto &manifest:manifests) {
            Json info;
            try {
                info = packages.contains(manifest) ? packages.at(manifest)->declaration : Json::parse(readTextFileBytes(manifest));
                validatePluginManifest(info);
                info.erase("managed");
                if(packages.contains(manifest))info["managed"]=packages.at(manifest)->metadata;
                const std::string id = info.at("id");
                if (!identifier(id) || pending.contains(id) || records_.contains(id))
                    throw std::runtime_error("Duplicate or invalid plugin ID");
                info["manifest"] = pathUtf8(manifest);
                info["compatibility"] = pluginCompatibility(info);
                pending.emplace(id, std::move(info));
            } catch (const std::exception &e) {
                if (!info.is_object())
                    info = Json::object();
                info["manifest"] = pathUtf8(manifest);
                info["state"] = "error";
                info["error"] = e.what();
                errors_.push_back(std::move(info));
            }
    }
    while (!pending.empty()) {
        bool progressed = false;
        for (auto it = pending.begin(); it != pending.end();) {
            bool wait = false;
            std::string error;
            for (const auto &d : it->second.value("dependencies", Json::array())) {
                const std::string id = d.at("id");
                if (pending.contains(id)) {
                    wait = true;
                    continue;
                }
                if (!records_.contains(id) || records_.at(id).info.contains("error")) {
                    if (!d.value("optional", false))
                        error = "Missing dependency: " + id;
                    continue;
                }
                const auto found = pluginVersion(records_.at(id).info.value("version", std::string("0.0.0")));
                if ((d.contains("minimum") && found < pluginVersion(d.at("minimum"))) ||
                    (d.contains("maximumExclusive") && found >= pluginVersion(d.at("maximumExclusive"))))
                    error = "Incompatible dependency: " + id;
            }
            if (wait) {
                ++it;
                continue;
            }
            const auto id = it->first;
            Record r;
            r.info = it->second;
            if(packages.contains(utf8Path(r.info.at("manifest"))))r.packageLease=packages.at(utf8Path(r.info.at("manifest")))->lease;
            r.enabled = configurationValid_ && state_.value("enabled", Json::object()).value(id, true);
            records_.emplace(id, std::move(r));
            auto &current = records_.at(id);
            try {
                if (!current.info.at("compatibility").at("compatible").get<bool>())
                    throw std::runtime_error(current.info.at("compatibility").at("reason"));
                if (!error.empty())
                    throw std::runtime_error(error);
                const std::string name = current.info.at("library");
                if (!identifier(name))
                    throw std::runtime_error("Plugin library must be a local basename");
                bool dependenciesActive = true;
                for (const auto &d : current.info.value("dependencies", Json::array()))
                    if (!d.value("optional", false) && !active(d.at("id")))
                        dependenciesActive = false;
                if (current.enabled && dependenciesActive)
                    initialize(id);
            } catch (const std::exception &e) {
                current.info["error"] = e.what();
            }
            it = pending.erase(it);
            progressed = true;
        }
        if (!progressed) {
            for (auto &[id, info] : pending) {
                Record r;
                r.info = info;
                r.info["error"] = "Cyclic plugin dependencies";
                records_.emplace(id, std::move(r));
            }
            break;
        }
    }
    ++revision_;
}
void Plugins::initialize(const std::string &id) {
    auto &current = records_.at(id);
    if (current.loaded)
        return;
    const auto count = contributions_.size();
    const auto directory = utf8Path(current.info.at("manifest")).parent_path();
    const std::string name = current.info.at("library");
    try {
        if(current.info.contains("managed")) {
            const auto verified=package_data::package(directory,true,true);
            if(verified.digest!=current.info.at("managed").at("digest").get<std::string>()||verified.plugin.at("id")!=id)throw std::runtime_error("Managed plugin changed after process selection");
            current.info["managed"]["payloadVerified"]=true;
        }
#ifdef _WIN32
        current.library = LoadLibraryExW((directory / utf8Path(name + ".dll")).c_str(), nullptr,
                                         LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        auto symbol = [&](const char *key) -> void * {
            return current.library
                       ? reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(current.library), key))
                       : nullptr;
        };
#else
#ifdef __APPLE__
        auto file = directory / (name + ".dylib");
        if (!std::filesystem::exists(file))
            file = directory / (name + ".so");
#else
        const auto file = directory / (name + ".so");
#endif
        current.library = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
        auto symbol = [&](const char *key) {
            return current.library ? dlsym(current.library, key) : nullptr;
        };
#endif
        if (!current.library)
            throw std::runtime_error("Cannot load plugin library");
        registering_ = id;
        registrationError_.clear();
        current.stop = current.info.at("abi") == 1
                           ? reinterpret_cast<CyCodePluginShutdown>(symbol("cycode_plugin_shutdown"))
                           : nullptr;
        current.stopV2 = current.info.at("abi") == 2
                             ? reinterpret_cast<CyCodePluginShutdownV2>(symbol("cycode_plugin_shutdown_v2"))
                             : nullptr;
        int result = 1;
        if (current.info.at("abi") == 2) {
            current.activationV2 = reinterpret_cast<CyCodePluginActiveV2>(symbol("cycode_plugin_active_v2"));
            const auto init = reinterpret_cast<CyCodePluginInitV2>(symbol("cycode_plugin_init_v2"));
            if (!init)
                throw std::runtime_error("Missing cycode_plugin_init_v2 export");
            result = init(&hostV2_, &current.instance);
        } else {
            const auto init = reinterpret_cast<CyCodePluginInit>(symbol("cycode_plugin_init"));
            if (!init)
                throw std::runtime_error("Missing cycode_plugin_init export");
            result = init(&hostV1_);
        }
        if (result || !registrationError_.empty())
            throw std::runtime_error(registrationError_.empty() ? "Plugin initialization failed"
                                                                : registrationError_);
        for (size_t i = count; i < contributions_.size(); ++i)
            if (contributions_[i].kind == "menu") {
                const auto command = contributions_[i].info.at("command");
                if (std::none_of(contributions_.begin() + count, contributions_.end(), [&](const auto &c) {
                        return c.kind == "command" && c.info.at("id") == command;
                    }))
                    throw std::runtime_error("Menu command must belong to the plugin");
            }
        current.loaded = true;
        loadOrder_.push_back(id);
        registering_.clear();
        if (current.enabled)
            notifyActivation(id, true);
    } catch (...) {
        contributions_.resize(count);
        if (current.stopV2 && current.instance) {
            try {
                current.stopV2(current.instance);
            } catch (...) {
            }
        }
        if (current.stop && current.library) {
            try {
                current.stop();
            } catch (...) {
            }
        }
        unload(current.library);
        current.library = nullptr;
        current.stop = nullptr;
        current.stopV2 = nullptr;
        current.activationV2 = nullptr;
        current.instance = nullptr;
        registering_.clear();
        throw;
    }
}
void Plugins::notifyActivation(const std::string &id, bool enabled) {
    eventStates_.clear(); // Registry activation changes require a fresh snapshot.
    auto &record = records_.at(id);
    if (!record.activationV2 || record.lifecycleActive == enabled)
        return;
    // Initialization, activation and background threads have no host access.
    // In particular a lifecycle callback must not inherit nested command access.
    auto *previousHost = callbackHost;
    callbackHost = nullptr;
    try {
        record.activationV2(record.instance, enabled ? 1u : 0u);
    } catch (...) {
        callbackHost = previousHost;
        record.lifecycleActive = enabled;
        record.info["lifecycleError"] = "Plugin activation callback threw across the ABI boundary";
        return;
    }
    callbackHost = previousHost;
    record.lifecycleActive = enabled;
}
void Plugins::activateDormant() {
    bool progressed = true;
    while (progressed) {
        progressed = false;
        for (auto &[id, record] : records_) {
            if (record.loaded || !record.enabled || record.info.contains("error"))
                continue;
            bool ready = true;
            for (const auto &dependency : record.info.value("dependencies", Json::array()))
                if (!dependency.value("optional", false) && !active(dependency.at("id")))
                    ready = false;
            if (!ready)
                continue;
            try {
                initialize(id);
                progressed = true;
            } catch (const std::exception &error) {
                record.info["error"] = error.what();
            }
        }
    }
}
Plugins::~Plugins() {shutdown();}
void Plugins::shutdown() {
    std::lock_guard lock(mutex_);
    for (auto it = loadOrder_.rbegin(); it != loadOrder_.rend(); ++it) {
        auto &r = records_.at(*it);
        notifyActivation(*it, false);
        if (r.stopV2 && r.instance) {
            try {
                r.stopV2(r.instance);
            } catch (...) {
            }
        }
        if (r.stop) {
            try {
                r.stop();
            } catch (...) {
            }
        }
        unload(r.library);
        r.library=nullptr;r.instance=nullptr;r.loaded=false;
    }
    loadOrder_.clear();contributions_.clear();eventStates_.clear();
}
void Plugins::pumpEvents(const std::function<Json(const Json &)> &read,std::stop_token stop) {
    std::unique_lock lock(mutex_,std::try_to_lock);
    if(!lock.owns_lock() || stop.stop_requested())return;
    std::vector<Contribution> listeners;
    for(const auto &c:contributions_)if(c.kind=="events") {
        if(active(c.owner))listeners.push_back(c);
        else eventStates_.erase(c.info.at("id"));
    }
    if(listeners.empty())return;
    const auto start=eventRoundRobin_%listeners.size();size_t delivered=0;
    for(size_t offset=0;offset<listeners.size() && !stop.stop_requested();++offset) {
        const auto index=(start+offset)%listeners.size();const auto &c=listeners[index];
        const auto id=c.info.at("id").get<std::string>();auto &state=eventStates_[id];
        if(!state.error.empty())continue;
        try {
            Json batch;
            if(!state.started){batch=read({{"tail",true}});batch["event"]="subscribed";batch["gap"]=true;}
            else {
                batch=read({{"epoch",state.epoch},{"afterSequence",state.cursor},{"limit",c.info.value("maxBatch",32)},{"types",c.info.value("types",Json::array())},{"excludeOrigin",c.owner}});
                if(batch.at("events").empty() && !batch.at("gap").get<bool>()){state.cursor=batch.at("nextSequence");continue;}
                batch["event"]=batch.at("gap").get<bool>()?"gap":"events";
            }
            batch["subscription"]=id;
            const bool resync=batch.at("gap");
            if(resync){batch["events"]=Json::array();batch["nextSequence"]=batch.at("latestSequence");}
            call(c,batch,!resync);state.started=true;state.epoch=batch.at("epoch");state.cursor=batch.at("nextSequence");
            eventRoundRobin_=(index+1)%listeners.size();
            if(++delivered>=8)break;
        } catch(const std::exception &e){state.error=e.what();}
    }
}
uint64_t Plugins::revision() const {
    return revision_.load();
}
Plugins::Json Plugins::contributions(const std::string &kind, const Json &context) const {
    std::lock_guard lock(mutex_);
    Json result = Json::array();
    for (const auto &c : contributions_)
        if (c.kind == kind && active(c.owner) && visible(c.info.value("when", Json::object()), context)) {
            auto info = c.info;
            info["plugin"] = c.owner;
            result.push_back(std::move(info));
        }
    std::stable_sort(result.begin(), result.end(),
                     [](const auto &a, const auto &b) { return a.value("order", 0) < b.value("order", 0); });
    return result;
}
Plugins::Json Plugins::describe() const {
    std::lock_guard lock(mutex_);
    Json plugins = errors_;
    for (const auto &[id, r] : records_) {
        auto info = r.info;
        info["enabled"] = r.enabled;
        info["active"] = active(id);
        info["loaded"] = r.loaded;
        info["lifecycle"] = {{"supported", r.loaded ? Json(r.activationV2 != nullptr) : Json(nullptr)},
                             {"active", r.lifecycleActive}};
        if (r.info.contains("lifecycleError"))
            info["lifecycle"]["error"] = r.info.at("lifecycleError");
        info["state"] = !r.info.value("compatibility", Json::object()).value("compatible", true) ? "incompatible"
                        : r.info.contains("error") ? "error"
                        : !r.enabled             ? "disabled"
                        : !active(id)            ? "dependency-disabled"
                                                 : "loaded";
        plugins.push_back(std::move(info));
    }
    // Inventory includes contextual contributions; individual views filter them.
    auto inventory = [&](const std::string &kind) {
        Json result = Json::array();
        for (const auto &c : contributions_)
            if (c.kind == kind && active(c.owner)) {
                auto info = c.info;
                info["plugin"] = c.owner;
                result.push_back(std::move(info));
            }
        std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
            return a.value("order", 0) < b.value("order", 0);
        });
        return result;
    };
    Json known = Json::array();
    Json debugAdapters=Json::array();
    for(const auto &entry:inventory("debugAdapter"))debugAdapters.push_back(debugAdapterSummary(entry));
    Json snippetCatalogues=Json::array();
    for(const auto &c:contributions_)if(c.kind=="snippets" && active(c.owner))
        snippetCatalogues.push_back({{"id",c.info.at("id")},{"plugin",c.owner},
            {"title",c.info.value("title",c.info.at("id").get<std::string>())},
            {"order",c.info.value("order",0)},{"definitionCount",c.info.at("definitions").size()}});
    auto subscriptions=inventory("events");
    for(auto &subscription:subscriptions){const auto id=subscription.at("id").get<std::string>();
        if(eventStates_.contains(id)){const auto &state=eventStates_.at(id);subscription["started"]=state.started;subscription["cursor"]=state.cursor;subscription["error"]=state.error;}
        else{subscription["started"]=false;subscription["cursor"]=0;subscription["error"]="";}
    }
    for (const auto &c : contributions_)
        if (c.kind == "command") {
            auto entry = c.info;
            entry["plugin"] = c.owner;
            known.push_back(std::move(entry));
        }
    return {{"abi", 2},
            {"host", pluginHostInfo()},
            {"supportedAbis", {1, 2}},
            {"revision", revision_.load()},
            {"plugins", plugins},
            {"knownCommands", known},
            {"eventSubscriptions",subscriptions},
            {"debugAdapters",debugAdapters},
            {"snippetCatalogues",std::move(snippetCatalogues)},
            {"commands", inventory("command")},
            {"menus", inventory("menu")},
            {"panels", inventory("panel")},
            {"projects", inventory("project")},
            {"vcsProviders", inventory("vcs")},
            {"syntaxes", inventory("syntax")},{"templates",inventory("template")},{"languageServers",inventory("languageServer")}};
}
Plugins::Json Plugins::languageSnapshot() const {
    const auto snapshot=serviceSnapshot();return {{"revision",snapshot.at("revision")},{"servers",snapshot.at("servers")}};
}
Plugins::Json Plugins::serviceSnapshot() const {
    std::lock_guard lock(mutex_);Json servers=Json::array(),debugAdapters=Json::array();
    for(const auto &c:contributions_)if((c.kind=="languageServer"||c.kind=="debugAdapter")&&active(c.owner)) {
        auto info=c.info;info["plugin"]=c.owner;(c.kind=="languageServer"?servers:debugAdapters).push_back(std::move(info));
    }
    return {{"revision",revision_.load()},{"servers",std::move(servers)},{"debugAdapters",std::move(debugAdapters)}};
}
Plugins::Json Plugins::templateSnapshot() const {
    std::lock_guard lock(mutex_);Json entries=Json::array();
    for(const auto &c:contributions_)if(c.kind=="template"&&active(c.owner)){auto info=c.info;info["plugin"]=c.owner;entries.push_back(std::move(info));}
    std::stable_sort(entries.begin(),entries.end(),[](const auto &a,const auto &b){return a.value("order",0)<b.value("order",0);});
    return {{"revision",revision_.load()},{"templates",entries}};
}
Plugins::Json Plugins::snippetSnapshot() const {
    std::lock_guard lock(mutex_);Json entries=Json::array();
    for(const auto &c:contributions_)if(c.kind=="snippets"&&active(c.owner)){
        auto info=c.info;info["plugin"]=c.owner;entries.push_back(std::move(info));
    }
    std::sort(entries.begin(),entries.end(),[](const auto &a,const auto &b){
        const auto x=a.value("order",0),y=b.value("order",0);
        return x!=y ? x<y : a.at("id").template get<std::string>()<b.at("id").template get<std::string>();
    });
    return {{"revision",revision_.load()},{"catalogues",std::move(entries)}};
}
Plugins::Json Plugins::createFromTemplate(const std::string &owner,uint64_t revision,const std::function<Json()> &commit) {
    std::lock_guard lock(mutex_);
    if(revision_.load()!=revision||!active(owner))throw std::runtime_error("Project templates changed or plugin was disabled; preview again");
    return commit();
}
Plugins::Json Plugins::configure(const Json &params) {
    std::lock_guard lock(mutex_);
    if (!configurationValid_)
        throw std::runtime_error("Repair invalid plugin configuration before enabling plugins");
    if (params.value("expectedRevision", revision_.load()) != revision_)
        throw std::runtime_error("RevisionConflict: plugin configuration changed");
    const std::string id = params.at("id");
    const bool enabled = params.at("enabled");
    if (!records_.contains(id) || records_.at(id).info.contains("error"))
        throw std::runtime_error("Unknown or unloadable plugin");
    if (enabled)
        for (const auto &d : records_.at(id).info.value("dependencies", Json::array()))
            if (!d.value("optional", false) && !active(d.at("id")))
                throw std::runtime_error("Enable required dependency first: " +
                                         d.at("id").get<std::string>());
    if (enabled && !records_.at(id).loaded)
        initialize(id);
    if (!enabled)
        for (const auto &[other, r] : records_)
            if (other != id && active(other))
                for (const auto &d : r.info.value("dependencies", Json::array()))
                    if (!d.value("optional", false) && d.at("id") == id)
                        throw std::runtime_error("Plugin is required by: " + other);
    auto next = state_;
    next["schemaVersion"] = 1;
    if (!next.contains("enabled"))
        next["enabled"] = Json::object();
    next["enabled"][id] = enabled;
    const auto file = statePath();
    std::filesystem::create_directories(file.parent_path());
    const auto bytes = next.dump(2) + "\n";
    writeTextFileAtomic(file, bytes, baseline_);
    baseline_ = bytes;
    state_ = std::move(next);
    records_.at(id).enabled = enabled;
    ++revision_;
    if (enabled)
        activateDormant();
    for(auto it=loadOrder_.rbegin();it!=loadOrder_.rend();++it)if(!active(*it))notifyActivation(*it,false);
    for(const auto &owner:loadOrder_)if(active(owner))notifyActivation(owner,true);
    return describe();
}
Plugins::Json Plugins::call(const Contribution &c, const Json &input, bool allowMutations) {
    EventOrigin origin(c.owner);
    if (!active(c.owner))
        throw std::runtime_error("Plugin is disabled or unavailable");
    const auto bytes = input.dump();
    if (bytes.size() > 4 * 1024 * 1024)
        throw std::runtime_error("Plugin input exceeds 4 MiB");
    const auto previous = invoking_;
    invoking_ = c.owner;
    auto *previousHost = callbackHost;
    callbackHost = this;
    const bool previousMutations = hostMutationAllowed_;
    hostMutationAllowed_ = allowMutations;
    try {
        auto result = parse(c.functionV2 ? c.functionV2(records_.at(c.owner).instance, bytes.c_str())
                                         : c.function(bytes.c_str()));
        if (c.functionV2 && result.is_object() && result.contains("error") &&
            result.at("error").is_string() && !result.at("error").get_ref<const std::string &>().empty())
            throw std::runtime_error(result.at("error"));
        invoking_ = previous;
        callbackHost = previousHost;
        hostMutationAllowed_ = previousMutations;
        return result;
    } catch (const std::exception &e) {
        invoking_ = previous;
        callbackHost = previousHost;
        hostMutationAllowed_ = previousMutations;
        records_.at(c.owner).info["lastError"] = e.what();
        throw;
    } catch (...) {
        invoking_ = previous;
        callbackHost = previousHost;
        hostMutationAllowed_ = previousMutations;
        throw std::runtime_error("Plugin callback threw across the ABI boundary");
    }
}
Plugins::Json Plugins::invoke(const std::string &id, const Json &input) {
    std::lock_guard lock(mutex_);
    for (const auto &c : contributions_)
        if ((c.kind == "command" || c.kind == "panel") && c.info.at("id") == id)
            return call(c, input);
    throw std::runtime_error("Unknown plugin command or panel");
}
Plugins::Json Plugins::provider(const std::string &kind, const std::string &id, const std::string &action,
                                const Json &input) {
    for (const auto &c : contributions_)
        if (c.kind == kind && c.info.at("id") == id) {
            const auto caps = c.info.at("capabilities");
            if (std::find(caps.begin(), caps.end(), action) == caps.end())
                throw std::runtime_error("Provider does not support " + action);
            const std::set<std::string> reads = {"detect", "inspect", "status", "diff",
                                                 "log",    "history", "show",   "blame"};
            const bool projectRead = kind == "project" &&
                                     (action == "artifacts" || action == "launchProfile" ||
                                      action == "automationPlan");
            return call(c, {{"action", action}, {"parameters", input}}, !reads.contains(action) && !projectRead);
        }
    throw std::runtime_error("Unknown " + kind + " provider");
}
Plugins::Json Plugins::vcs(const std::string &id, const std::string &action, const Json &input) {
    std::lock_guard lock(mutex_);
    return provider("vcs", id, action, input);
}
Plugins::Json Plugins::project(const std::string &id, const std::string &action, const Json &input) {
    std::lock_guard lock(mutex_);
    return provider("project", id, action, input);
}
} // namespace cycode
