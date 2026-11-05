#include "ProjectTemplate.h"
#include "cycode/PluginAPI.h"
#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#include <string>
using Json = nlohmann::json;
namespace {
struct Instance {
    CyCodeHostV2 host;
    Json profile = Json::object();
    std::string output;
    uint64_t revision = 1;
    bool active = false;
    uint64_t observedEvents = 0;
    std::string lastEvent = "Waiting for workspace events";
    Json request(const std::string &method, const Json &parameters) {
        const auto input = parameters.dump();
        auto response = Json::parse(host.request(host.context, method.c_str(), input.c_str()));
        if (!response.at("ok").get<bool>())
            throw std::runtime_error(response.at("error"));
        return response.at("result");
    }
    Json inspect() {
        return request("unreal.inspect", profile);
    }
    Json run(const std::string &action) {
        auto result = request("unreal." + action, profile);
        if (result.contains("project") && result.at("project").value("available", false)) {
            profile = result.at("project").at("profile");
            ++revision;
        }
        return result;
    }
};
template <class F> const char *reply(void *instance, const char *input, F callback) noexcept {
    auto &self = *static_cast<Instance *>(instance);
    try {
        if (!self.active)
            throw std::runtime_error("Unreal extension is inactive");
        self.output = callback(self, Json::parse(input)).dump();
    } catch (const std::exception &e) {
        self.output = Json({{"error", e.what()}}).dump();
    } catch (...) {
        self.output = "{\"error\":\"Unreal extension failed\"}";
    }
    return self.output.c_str();
}
const char *inspect(void *s, const char *p) {
    return reply(s, p, [](Instance &self, const Json &) { return self.inspect(); });
}
const char *project(void *s, const char *p) {
    return reply(s, p, [](Instance &self, const Json &input) {
        const std::string action = input.at("action");
        if (action == "detect") {
            const auto model = self.inspect();
            return Json({{"available", model.at("available")}, {"kind", "unreal"}});
        }
        if (action == "inspect")
            return self.request("unreal.inspect", input.value("parameters", self.profile));
        auto parameters = self.profile;
        parameters.update(input.value("parameters", Json::object()));
        return self.request("unreal." + action, parameters);
    });
}
template <const char *Action> const char *run(void *s, const char *p) {
    return reply(s, p, [](Instance &self, const Json &) { return self.run(Action); });
}
constexpr char generateAction[] = "generate", buildAction[] = "build", prepareAction[] = "prepare",
               databaseAction[] = "database";
const char *events(void *s, const char *p) {
    return reply(s, p, [](Instance &self, const Json &batch) {
        if (batch.value("gap", false))
            self.lastEvent = "Workspace snapshot refreshed";
        else
            for (const auto &event : batch.at("events")) {
                ++self.observedEvents;
                self.lastEvent = event.at("type");
            }
        return Json::object();
    });
}
const char *panel(void *s, const char *p) {
    return reply(s, p, [](Instance &self, const Json &input) {
        if (input.value("event", std::string("read")) == "commit") {
            if (input.at("expectedRevision") != self.revision)
                throw std::runtime_error("RevisionConflict: Unreal profile changed");
            const std::string key = input.at("property");
            const std::array<std::string, 5> keys = {"project", "engineRoot", "target", "platform",
                                                     "configuration"};
            if (std::find(keys.begin(), keys.end(), key) == keys.end() || !input.at("value").is_string() ||
                input.at("value").get_ref<const std::string &>().size() > 4096)
                throw std::runtime_error("Invalid Unreal profile property");
            auto candidate = self.profile;
            candidate[key] = input.at("value");
            const auto checked = self.request("unreal.inspect", candidate);
            if (!checked.at("available").get<bool>())
                throw std::runtime_error("Select an Unreal project in this workspace");
            self.profile = checked.at("profile");
            ++self.revision;
        }
        const auto model = self.inspect();
        Json result = {{"revision", self.revision},
                       {"text", model.value("available", false)
                                    ? model.value("name", std::string("Unreal project"))
                                    : "Open an Unreal .uproject or solution."},
                       {"properties", Json::array()},
                       {"rows", Json::array()},
                       {"actions", Json::array()}};
        if (!model.value("available", false))
            return result;
        result["rows"].push_back({{"name", "Last workspace event"}, {"value", self.lastEvent}});
        result["rows"].push_back(
            {{"name", "Observed events"}, {"value", std::to_string(self.observedEvents)}});
        const auto profile = model.at("profile");
        for (const auto *key : {"project", "engineRoot", "target", "platform", "configuration"})
            result["properties"].push_back(
                {{"id", key}, {"title", key}, {"value", profile.at(key)}, {"readOnly", false}});
        for (const auto *section : {"targets", "modules", "plugins"})
            for (const auto &item : model.at(section)) {
                if (result["rows"].size() >= 2000)
                    break;
                result["rows"].push_back({{"name", item.at("name")}, {"value", item.at("path")}});
            }
        for (const auto *action : {"inspect", "generate", "prepare", "database", "build"})
            result["actions"].push_back(
                {{"command", "cycode.unreal." + std::string(action)}, {"title", action}});
        return result;
    });
}
} // namespace
extern "C" CYCODE_PLUGIN_EXPORT int cycode_plugin_init_v2(const CyCodeHostV2 *host,
                                                          void **instance) noexcept {
    if (!host || host->abi_version != 2 || host->struct_size < sizeof(CyCodeHostV2) || !instance ||
        !(host->features & CYCODE_HOST_FEATURE_ACTIVATION_V2))
        return 1;
    try {
        auto *self = new Instance;
        self->host = *host;
        *instance = self;
        const auto add = [&](const char *kind, const Json &descriptor, CyCodeCommandV2 fn) {
            const auto bytes = descriptor.dump();
            if (host->contribute(host->context, kind, bytes.c_str(), fn))
                throw std::runtime_error("Unreal contribution rejected");
        };
        add("template", unrealProjectTemplate(), nullptr);
        if(host->features & CYCODE_HOST_FEATURE_DEBUG_ADAPTERS_V2)
            add("debugAdapter",{{"id","cycode.unreal.lldb"},{"title","Unreal Engine / CodeLLDB"},
                {"type","lldb"},{"arguments",{"codelldb"}},{"requests",{"launch","attach"}},{"pidField","pid"},
                {"launch",{{"sourceLanguages",{"cpp"}}}}},nullptr);
        if (host->features & CYCODE_HOST_FEATURE_EVENT_SUBSCRIPTIONS_V2)
            add("events",
                {{"id", "cycode.unreal.events"},
                 {"title", "Unreal workspace events"},
                 {"types", {"workspace.changed", "resources.changed", "build.finished"}},
                 {"maxBatch", 32}},
                events);
        const std::array<std::pair<const char *, CyCodeCommandV2>, 5> commands = {
            {{"inspect", inspect},
             {"generate", run<generateAction>},
             {"prepare", run<prepareAction>},
             {"database", run<databaseAction>},
             {"build", run<buildAction>}}};
        for (const auto &[action, callback] : commands) {
            const std::string id = "cycode.unreal." + std::string(action);
            add("command",
                {{"id", id},
                 {"title", "Unreal: " + std::string(action)},
                 {"category", "Build"},
                 {"when", {{"project", "unreal"}}}},
                callback);
            add("menu",
                {{"id", id + ".menu"},
                 {"title", "Unreal: " + std::string(action)},
                 {"location", "build"},
                 {"command", id},
                 {"when", {{"project", "unreal"}}}},
                nullptr);
        }
        add("panel",
            {{"id", "cycode.unreal.overview"},
             {"title", "Unreal project overview"},
             {"when", {{"project", "unreal"}}}},
            panel);
        add("project",
            {{"id", "cycode.unreal.project"},
             {"title", "Unreal Engine"},
             {"capabilities", {"detect", "inspect", "generate", "prepare", "database", "build", "automationPlan", "automationRun", "launchProfile", "artifacts"}}},
            project);
        return 0;
    } catch (...) {
        return 1;
    }
}
extern "C" CYCODE_PLUGIN_EXPORT void cycode_plugin_shutdown_v2(void *instance) noexcept {
    delete static_cast<Instance *>(instance);
}
extern "C" CYCODE_PLUGIN_EXPORT void cycode_plugin_active_v2(void *instance, uint32_t active) noexcept {
    static_cast<Instance *>(instance)->active = active != 0;
}
