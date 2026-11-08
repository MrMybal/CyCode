#include "TemplateFixture.h"
#include "cycode/PluginAPI.h"
#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
using Json = nlohmann::json;
static std::atomic<int> legacyCleanupCalls = 0;
struct Instance {
    CyCodeHostV2 host;
    std::string output;
    int counter = 0;
    uint64_t revision = 1;
    bool active = false, lifecycleHostDenied = false;
    int activationChanges = 0;
    int eventBatches = 0, eventResyncs = 0;
    bool eventFail = false, eventMutate = false, eventInitialWriteDenied = false;
};
static Json host(Instance &state, const char *method, const Json &params) {
    return Json::parse(state.host.request(state.host.context, method, params.dump().c_str()));
}
static const char *command(void *context, const char *text) {
    auto &state = *static_cast<Instance *>(context);
    try {
        const auto input = Json::parse(text);
        if (input.value("mode", std::string{}) == "events") {
            state.eventFail = input.value("fail", state.eventFail);
            state.eventMutate = input.value("mutate", state.eventMutate);
            state.output = Json{{"batches", state.eventBatches},
                                {"resyncs", state.eventResyncs},
                                {"initialWriteDenied", state.eventInitialWriteDenied}}
                               .dump();
        } else if (input.value("mode", std::string{}) == "lifecycle")
            state.output = Json({{"active", state.active},
                                 {"changes", state.activationChanges},
                                 {"hostDenied", state.lifecycleHostDenied},
                                 {"legacyCleanupCalls", legacyCleanupCalls.load()}})
                               .dump();
        else if (input.value("mode", std::string{}) == "replace")
            state.output = host(state, "documents.replace", input.at("document")).dump();
        else if (input.value("mode", std::string{}) == "host")
            state.output =
                host(state, input.at("method").get<std::string>().c_str(), input.at("parameters")).dump();
        else if (input.value("mode", std::string{}) == "forbidden")
            state.output = host(state, "terminal.create", Json::object()).dump();
        else {
            auto result = host(state, "workspace.describe", Json::object());
            result["counter"] = ++state.counter;
            state.output = result.dump();
        }
    } catch (const std::exception &e) {
        state.output = Json({{"error", e.what()}}).dump();
    }
    return state.output.c_str();
}
static const char *events(void *context, const char *text) {
    auto &state = *static_cast<Instance *>(context);
    try {
        const auto batch = Json::parse(text);
        if (batch.value("gap", false)) {
            ++state.eventResyncs;
            state.eventInitialWriteDenied =
                !host(state, "documents.create", {{"path", "must-not-create.txt"}}).at("ok").get<bool>();
        } else {
            if (state.eventFail)
                throw std::runtime_error("Fixture subscriber failed");
            ++state.eventBatches;
            if (state.eventMutate)
                for (const auto &event : batch.at("events"))
                    if (event.at("type") == "document.changed") {
                        const auto path = event.at("data").at("path");
                        const auto read = host(state, "documents.read", {{"path", path}});
                        if (read.at("ok") &&
                            read.at("result").at("revision") == event.at("data").at("revision")) {
                            const auto &doc = read.at("result");
                            auto changed =
                                host(state, "documents.replace",
                                     {{"path", path},
                                      {"expectedRevision", doc.at("revision")},
                                      {"text", doc.at("text").get<std::string>() + "// subscriber\n"}});
                            if (!changed.at("ok"))
                                throw std::runtime_error("Fixture subscriber edit failed");
                        }
                    }
        }
        state.output = "{}";
    } catch (const std::exception &e) {
        state.output = Json{{"error", e.what()}}.dump();
    }
    return state.output.c_str();
}
static const char *panel(void *context, const char *text) {
    auto &state = *static_cast<Instance *>(context);
    try {
        const auto input = Json::parse(text);
        if (input.value("event", std::string{}) == "invalid-model") {
            state.output = R"({"revision":0,"properties":[{"id":"counter","value":{}}]})";
            return state.output.c_str();
        }
        if (input.value("event", std::string{}) == "commit") {
            if (input.at("expectedRevision") != state.revision)
                throw std::runtime_error("RevisionConflict");
            state.counter = input.at("value");
            ++state.revision;
        }
        state.output =
            Json({{"revision", state.revision},
                  {"text", "Native extension panel"},
                  {"properties", Json::array({{{"id", "counter"},
                                               {"title", "Counter"},
                                               {"value", state.counter},
                                               {"readOnly", false}}})},
                  {"rows", Json::array({{{"name", "Instance"}, {"value", std::to_string(state.counter)}}})}})
                .dump();
    } catch (const std::exception &e) {
        state.output = Json({{"error", e.what()}}).dump();
    }
    return state.output.c_str();
}
static const char *vcs(void *context, const char *text) {
    auto &state = *static_cast<Instance *>(context);
    try {
        const auto input = Json::parse(text);
        const std::string action = input.at("action");
        if (action == "status") {
            // Prove that a read callback cannot use host write services.
            const auto forbidden = host(state, "documents.create", {{"path", "should-not-exist.txt"}});
            const auto forbiddenChange = host(state, "changes.apply", {{"id", "not-issued"}});
            state.output = Json({{"provider", "fixture.native.depot"},
                                 {"repository", true},
                                 {"branch", "test"},
                                 {"files", Json::array({{{"status", "locked"}, {"path", "main.cpp"}}})},
                                 {"message", "Fixture provider"},
                                 {"hostWriteDenied", !forbidden.at("ok").get<bool>()},
                                 {"hostChangeDenied", !forbiddenChange.at("ok").get<bool>() &&
                                     forbiddenChange.at("error").get<std::string>().find("Read-only")!=std::string::npos}})
                               .dump();
        } else
            state.output = Json({{"exitCode", 0},
                                 {"output", action},
                                 {"timedOut", false},
                                 {"truncated", false},
                                 {"parameters", input.at("parameters")}})
                               .dump();
    } catch (const std::exception &e) {
        state.output = Json({{"error", e.what()}}).dump();
    }
    return state.output.c_str();
}
static const char *project(void *context, const char *) {
    auto &state = *static_cast<Instance *>(context);
    try {
        const auto read = host(state, "workspace.describe", Json::object());
        const auto write = host(state, "documents.create", {{"path", "project-read-must-not-exist.txt"}});
        const auto change = host(state, "changes.apply", {{"id", "not-issued"}});
        state.output = Json({{"hostReadAllowed", read.at("ok")},
                             {"hostWriteDenied", !write.at("ok").get<bool>() &&
                                 write.at("error").get<std::string>().find("Read-only") != std::string::npos},
                             {"hostChangeDenied", !change.at("ok").get<bool>() &&
                                 change.at("error").get<std::string>().find("Read-only") != std::string::npos}})
                           .dump();
    } catch (const std::exception &e) {
        state.output = Json({{"error", e.what()}}).dump();
    }
    return state.output.c_str();
}
extern "C" CYCODE_PLUGIN_EXPORT int cycode_plugin_init_v2(const CyCodeHostV2 *host, void **instance) {
    if (!host || host->abi_version != 2 || host->struct_size < sizeof(CyCodeHostV2) || !instance)
        return 1;
    try {
        auto *state = new Instance{*host};
        *instance = state;
#ifndef CYCODE_FIXTURE_EMPTY
        auto add = [&](const char *kind, const Json &descriptor, CyCodeCommandV2 callback) {
            return host->contribute(host->context, kind, descriptor.dump().c_str(), callback);
        };
        if (add("template", fixtureProjectTemplate(), nullptr))
            return 1;
        if ((host->features & CYCODE_HOST_FEATURE_SNIPPETS_V2) && add("snippets",
            {{"id","fixture.native.snippets"},{"title","Fixture snippets"},
             {"definitions",{{"Wrap",{{"prefix","wrap_fixture"},{"scope","cpp"},
                 {"body","[${1:$TM_SELECTED_TEXT}]$0"}}}}}},nullptr)) return 1;
        if (add("command",
                {{"id", "fixture.native.inspect"},
                 {"title", "Inspect extension workspace"},
                 {"shortcuts", {"Ctrl+Alt+F10"}}},
                command))
            return 1;
        if (add("menu",
                {{"id", "fixture.native.menu"},
                 {"title", "Extension project action"},
                 {"location", "project"},
                 {"command", "fixture.native.inspect"}},
                nullptr))
            return 1;
        if (add("panel", {{"id", "fixture.native.panel"}, {"title", "Extension test panel"}}, panel))
            return 1;
        if (add("events",
                {{"id", "fixture.native.events"},
                 {"title", "Fixture document observer"},
                 {"types", {"document.changed"}},
                 {"maxBatch", 8}},
                events))
            return 1;
        if (add("syntax",
                {{"id", "fixture.native.syntax"},
                 {"name", "Fixture syntax"},
                 {"extensions", {"cytest"}},
                 {"rules", {{"keywords", {"custom_keyword"}}, {"lineComment", "#"}}}},
                nullptr))
            return 1;
        if (add("vcs",
                {{"id", "fixture.native.depot"},
                 {"title", "Fixture depot"},
                 {"capabilities", {"status", "checkout", "submit"}},
                 {"actions",
                  Json::array({{{"id", "checkout"}, {"title", "Check out file"}, {"scope", "file"}},
                               {{"id", "submit"},
                                {"title", "Submit changelist"},
                                {"fields", Json::array({{{"id", "description"},
                                                         {"title", "Description"},
                                                         {"required", true}}})}}})}},
                vcs))
            return 1;
        if (add("project",
                {{"id", "fixture.native.project"},
                 {"title", "Fixture project"},
                 {"capabilities", {"inspect", "artifacts", "launchProfile", "automationPlan"}}},
                project))
            return 1;
#endif
        return 0;
    } catch (...) {
        return 1;
    }
}
extern "C" CYCODE_PLUGIN_EXPORT void cycode_plugin_shutdown_v2(void *instance) {
    delete static_cast<Instance *>(instance);
}
// A DLL may export both ABI entry-point families. ABI2 must never call ABI1 cleanup.
extern "C" CYCODE_PLUGIN_EXPORT void cycode_plugin_shutdown(void) {
    ++legacyCleanupCalls;
}
extern "C" CYCODE_PLUGIN_EXPORT void cycode_plugin_active_v2(void *instance, uint32_t active) noexcept {
    auto &state = *static_cast<Instance *>(instance);
    state.active = active != 0;
    ++state.activationChanges;
    try {
        state.lifecycleHostDenied = !host(state, "workspace.describe", Json::object()).at("ok").get<bool>();
    } catch (...) {
        state.lifecycleHostDenied = false;
    }
}
