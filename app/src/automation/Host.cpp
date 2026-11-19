#include "Internal.h"
#include <cycode/Tools.h>
#include <iostream>
#include <thread>

namespace cycode::automation {
namespace {
Json object(Json properties = Json::object(), Json required = Json::array()) {
    return {{"type", "object"}, {"properties", std::move(properties)}, {"required", std::move(required)},
            {"additionalProperties", false}};
}
Json stringSchema() { return {{"type", "string"}}; }
const std::map<std::string, std::string> controls{
    {"cy_describe", "DescribeTool"}, {"cy_status", "GetRuntimeStatus"}, {"cy_list_jobs", "ListJobs"},
    {"cy_get_job", "GetJob"}, {"cy_cancel_job", "CancelJob"}, {"cy_events", "GetEvents"},
    {"cy_can_run", "CanRun"}, {"cy_capabilities", "GetCapabilities"}, {"cy_list_operations", "ListOperations"},
    {"cy_describe_operation", "DescribeOperation"}, {"cy_list_backends", "ListBackends"}, {"cy_list_models", "ListModels"},
    {"cy_system_resources", "GetSystemResources"}, {"cy_estimate", "EstimateResources"}, {"cy_diagnose", "Diagnose"},
    {"cy_release_resources", "ReleaseResources"}, {"cy_begin_task", "CreateSession"}, {"cy_finish_task", "FinishTask"},
    {"cy_keep_task_alive", "KeepTaskAlive"}, {"cy_export_job", "ExportJob"}, {"cy_import_file", "ImportFile"}};
const std::set<std::string> common{
    "service_status", "language_status", "workspace_files", "search_text", "documents_read", "documents_read_page",
    "documents_read_lines", "language_definition", "language_references", "language_hover", "language_document_symbols",
    "language_symbols", "language_symbol_resolve", "analysis_prepare", "analysis_status", "analysis_results",
    "index_prepare", "index_status", "index_symbols", "index_refresh", "index_callers", "index_outline", "index_read"};
}
struct Host::Impl {
    NativeCall native;
    Options options;
    Json original;
    std::map<std::string, Json> operations;
    std::map<std::string, std::set<std::string>> principals;
    mutable std::mutex principalMutex;
    std::shared_ptr<cymcp::Registry> canonical, legacyView;
    std::shared_ptr<cymcp::ResourceRegistry> resourceView;
    std::shared_ptr<cymcp::PerformanceMonitor> performance;
    std::shared_ptr<cymcp::ChangeFeed> changes;
    std::unique_ptr<Jobs> jobs;
    std::unique_ptr<cymcp::ToolGateway> gateway;
    std::map<std::string, std::string> legacySessions;
    std::string stdioPrincipal;
    std::jthread eventReader;
    std::mutex sessionMutex;

    Impl(NativeCall callback, Options configured) : native(std::move(callback)), options(std::move(configured)) {
        original = readJson(options.catalogue);
        const auto publicMetadata = options.catalogue.parent_path() / "CyTool.json";
        if (std::filesystem::exists(publicMetadata)) {
            auto metadata = readJson(publicMetadata);
            metadata["operations"] = original.at("operations");
            original = std::move(metadata);
        }
        if (!original.contains("operations")) throw cymcp::Error("InvalidState", "Operation catalogue is missing");
        for (const auto& op : original.at("operations")) {
            if (!op.contains("nativeMethod")) throw cymcp::Error("InvalidState", "Regenerate app/MCPMethods.json");
            operations.emplace(op.at("id"), op);
        }
        principals[options.principal] = options.permissions;
        principals["local"] = options.permissions; // SDK's stdio transport principal
        auto policy = cymcp::Policy([this](const cymcp::CallContext& c, const std::string& name, const Json&) {
            return allowed(name, c) ? cymcp::Decision::allow : cymcp::Decision::deny;
        });
        canonical = std::make_shared<cymcp::Registry>(policy);
        legacyView = std::make_shared<cymcp::Registry>(policy);
        if (options.notifications) {
            changes = std::make_shared<cymcp::ChangeFeed>();
            canonical->set_change_feed(changes); legacyView->set_change_feed(changes);
        }
        if (!options.telemetryFile.empty()) {
            cymcp::PerformanceOptions measured;
            measured.jsonl_file = options.telemetryFile;
            performance = std::make_shared<cymcp::PerformanceMonitor>(measured);
            canonical->set_performance_monitor(performance); legacyView->set_performance_monitor(performance);
        }
        auto storage = options.gateway.resources;
        if (!options.spoolDirectory.empty()) storage.disk_directory = options.spoolDirectory;
        resourceView = std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all(), storage);
        if (changes) resourceView->set_change_feed(changes);
        for (const auto& [id, op] : operations) {
            auto input = sdkSchema(op.at("inputSchema"));
            input["$comment"] = "Exact canonical contract: cycode://contracts/" + id + "; additional constraints are enforced by the host.";
            const auto output = sdkSchema(op.at("outputSchema"));
            const auto permissions = op.value("permissions", Json::array());
            cymcp::Operation operation;
            operation.name = id;
            operation.description = op.value("description", "");
            operation.input_schema = input;
            operation.output_schema = output;
            operation.read_only = permissions.empty();
            operation.handler = [this, id, op](const Json& parameters, const cymcp::CallContext& context) -> Json {
                const auto actor = resolved(context);
                validateContract(parameters, op.at("inputSchema"));
                if (!withinJob && !op.value("permissions", Json::array()).empty()) {
                    auto issued = jobs->submit(id, parameters, actor);
                    auto result = jobs->wait(issued.at("jobId"), actor);
                    if (result.at("state") != "Completed") throw OperationError(result.at("error"));
                    if (!result.value("durable", true)) throw OperationError({{"code", "ResultPersistenceFailed"},
                        {"message", "Native action completed. Do not replay; recover this job from its live owner."},
                        {"details", {{"jobId", result.at("jobId")}, {"state", result.at("state")}, {"outputs", result.at("outputs")}}}});
                    return result.at("outputs");
                }
                auto result = native(op.at("nativeMethod"), options.routeNative ? nativeParameters(op, parameters) : parameters, actor);
                try { validateContract(result, op.at("outputSchema")); }
                catch (const cymcp::Error& e) { throw OperationError({{"code", "InvalidOutput"}, {"message", e.what()},
                    {"details", {{"operationId", id}, {"actualResult", result},
                        {"actionMayHaveCompleted", !op.value("permissions", Json::array()).empty()}}}}); }
                return result;
            };
            canonical->add(std::move(operation));
            cymcp::Operation compatibility;
            compatibility.name = "op_" + id;
            compatibility.description = op.value("description", "") + " Legacy asynchronous job envelope; use direct operation for new clients.";
            compatibility.input_schema = object({{"parameters", input}, {"sessionId", stringSchema()},
                {"priority", stringSchema()}, {"queueTimeout", {{"type", "number"}}},
                {"executionTimeout", {{"type", "number"}}}, {"backendId", stringSchema()}, {"modelId", stringSchema()}}, {"parameters"});
            compatibility.output_schema = {{"type", "object"}};
            compatibility.handler = [this, id](const Json& p, const cymcp::CallContext& context) {
                auto args = p; args["operationId"] = id;
                return legacyCall("SubmitJob", args, context);
            };
            if (options.profile != "code" || common.contains(id)) legacyView->add(std::move(compatibility));
            auto snapshot = std::make_shared<const std::string>(op.dump());
            cymcp::Resource contract;
            contract.uri = "cycode://contracts/" + id; contract.name = id;
            contract.description = "Full canonical input/output contract, native method and permissions";
            contract.mime_type = "application/json"; contract.size = snapshot->size();
            contract.cache_ttl = std::chrono::hours(24);
            contract.read = [snapshot](std::size_t offset, std::size_t length, const cymcp::CallContext&) {
                return offset < snapshot->size() ? snapshot->substr(offset, length) : std::string{};
            };
            resourceView->add(std::move(contract));
        }
        jobs = std::make_unique<Jobs>(options,
            [this](const std::string& name, const Json& parameters, const cymcp::CallContext& context) {
                return canonical->invoke(name, parameters, context);
            }, [this](const std::string& name) { return contract(name); });
        for (const auto& [name, method] : controls) addControl(name, method);
        addControl("core_job_submit", "SubmitJob");
        addControl("core_job_get", "GetJob");
        addControl("core_job_list", "ListJobs");
        addControl("core_job_cancel", "CancelJob");
        addControl("core_contract", "DescribeOperation");
        cymcp::Operation invoke;
        invoke.name = "core_invoke";
        invoke.description = "Legacy gateway to a canonical operation; actual permissions and validation always apply.";
        invoke.input_schema = object({{"parameters", object({{"operationId", stringSchema()},
            {"parameters", {{"type", "object"}}}}, {"operationId", "parameters"})}}, {"parameters"});
        invoke.output_schema = {{"type", "object"}};
        invoke.handler = [this](const Json& value, const cymcp::CallContext& context) {
            auto request = value.at("parameters");
            return legacyCall("SubmitJob", request, context);
        };
        // The historical name is op_core_invoke with two nested parameter envelopes.
        invoke.name = "op_core_invoke";
        legacyView->add(std::move(invoke));
        if (options.profile == "compact")
            gateway = std::make_unique<cymcp::ToolGateway>(canonical, options.gateway, resourceView);
        if (changes) eventReader = std::jthread([this](std::stop_token stop) {
            std::string epoch;
            std::uint64_t sequence = 0;
            while (!stop.stop_requested()) {
                try {
                    Json parameters = epoch.empty() ? Json{{"tail", true}} : Json{{"epoch", epoch}, {"afterSequence", sequence}, {"limit", 512}};
                    cymcp::CallContext context;
                    const auto batch = native("events.read", parameters, context);
                    const auto nextEpoch = batch.at("epoch").get<std::string>();
                    const auto nextSequence = batch.at("nextSequence").get<std::uint64_t>();
                    if (epoch.empty() || nextEpoch != epoch || nextSequence != sequence || batch.value("gap", false)) {
                        epoch = nextEpoch; sequence = nextSequence;
                        auto bytes = std::make_shared<const std::string>(Json{{"epoch", epoch}, {"sequence", sequence},
                            {"gap", batch.value("gap", false)}}.dump());
                        cymcp::Resource resource;
                        resource.uri = "cycode://workspace/changes"; resource.name = "Workspace revision";
                        resource.description = "Live native event epoch/sequence only; no source text or paths";
                        resource.mime_type = "application/json"; resource.size = bytes->size();
                        resource.cache_ttl = options.cacheTtl;
                        resource.read = [bytes](std::size_t offset, std::size_t length, const cymcp::CallContext&) {
                            return offset < bytes->size() ? bytes->substr(offset, length) : std::string{};
                        };
                        resourceView->replace(std::move(resource));
                    }
                } catch (...) { /* A disconnected owner is reported by the real domain call. */ }
                for (unsigned i = 0; i < 10 && !stop.stop_requested(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    ~Impl() {
        if (eventReader.joinable()) { eventReader.request_stop(); eventReader.join(); }
        if (jobs) jobs->stop(); // Join callbacks before their captured host fields are destroyed.
    }
    Json nativeParameters(const Json& op, const Json& parameters) const {
        const auto method = op.at("nativeMethod").get<std::string>();
        if (!method.starts_with("vcs.") || method == "vcs.repositories" || method == "vcs.providerAction" || method == "vcs.configure" ||
            method == "vcs.context" || method == "vcs.providers") return parameters;
        if (parameters.value("provider", "git") != "git")
            throw cymcp::Error("UnsupportedProvider", "Use vcs_provider_action for external providers");
        Json result = parameters;
        result["provider"] = "git";
        result["contextPath"] = parameters.value("repository", ".");
        return result;
    }
    Json contract(std::string name) const {
        if (name.starts_with("op_")) name.erase(0, 3);
        const auto found = operations.find(name);
        if (found == operations.end()) throw cymcp::Error("UnknownOperation", "Unknown canonical operation");
        return found->second;
    }
    cymcp::CallContext resolved(cymcp::CallContext context) const {
        if (context.principal == "local" && !stdioPrincipal.empty()) context.principal = stdioPrincipal;
        return context;
    }
    bool allowed(std::string name, const cymcp::CallContext& incoming) const {
        const auto context = resolved(incoming);
        if (name == "cy_import_file" && !hasRight(context.principal, "files.import")) return false;
        if (name == "cy_export_job" && !hasRight(context.principal, "files.export")) return false;
        if (name.starts_with("op_")) name.erase(0, 3);
        const auto op = operations.find(name);
        if (op == operations.end()) return true; // controls enforce ownership/target policy themselves
        std::lock_guard lock(principalMutex);
        const auto found = principals.find(context.principal);
        if (found == principals.end()) return false;
        for (const auto& permission : op->second.value("permissions", Json::array()))
            if (!found->second.contains("*") && !found->second.contains(permission.get<std::string>())) return false;
        return true;
    }
    bool hasRight(const std::string& who, const std::string& right) const {
        std::lock_guard lock(principalMutex);
        const auto found = principals.find(who);
        return found != principals.end() && (found->second.contains("*") || found->second.contains(right));
    }
    void addControl(const std::string& name, const std::string& method) {
        Json properties = {{"operationId", stringSchema()}, {"parameters", {{"type", "object"}}},
            {"jobId", stringSchema()}, {"sessionId", stringSchema()}, {"backendId", stringSchema()},
            {"modelId", stringSchema()}, {"priority", stringSchema()}, {"destination", stringSchema()},
            {"source", stringSchema()}, {"companions", {{"type", "array"}, {"items", stringSchema()}}},
            {"level", stringSchema()}, {"temporary", {{"type", "boolean"}}}, {"cancelRunning", {{"type", "boolean"}}},
            {"retentionSeconds", {{"type", "number"}, {"minimum", 1}, {"maximum", 86400}}},
            {"after", {{"type", "integer"}, {"minimum", 0}}}, {"queueTimeout", {{"type", "number"}, {"minimum", 0}}},
            {"executionTimeout", {{"type", "number"}, {"minimum", 0}}}};
        cymcp::Operation control;
        control.name = name;
        control.description = method + "; application-owned durable jobs, same native registry and target permissions.";
        Json required = Json::array();
        if (method == "SubmitJob" || method == "CanRun" || method == "EstimateResources" || method == "DescribeOperation") required.push_back("operationId");
        if (method == "GetJob" || method == "CancelJob" || method == "ExportJob") required.push_back("jobId");
        if (method == "KeepTaskAlive" || method == "FinishTask" || method == "ImportFile") required.push_back("sessionId");
        if (method == "ImportFile") required.push_back("source");
        if (method == "ExportJob") required.push_back("destination");
        control.input_schema = object(properties, required);
        control.output_schema = Json::object(); // Legacy controls may return a JSON array.
        static const std::set<std::string> readControls{"DescribeTool", "GetRuntimeStatus", "ListJobs", "GetJob", "GetEvents",
            "CanRun", "GetCapabilities", "ListOperations", "DescribeOperation", "ListBackends", "ListModels",
            "GetSystemResources", "EstimateResources", "Diagnose"};
        control.read_only = readControls.contains(method);
        control.handler = [this, method](const Json& p, const cymcp::CallContext& context) -> Json {
            return legacyCall(method, p, context);
        };
        canonical->add(control);
        legacyView->add(std::move(control));
    }
    Json legacyCall(const std::string& method, Json p, cymcp::CallContext context) {
        context = resolved(context);
        if ((method == "ExportJob" && !hasRight(context.principal, "files.export")) ||
            (method == "ImportFile" && !hasRight(context.principal, "files.import")))
            throw cymcp::Error("AccessDenied", "File import/export requires a configured host root and an explicit file permission");
        if (method == "DescribeTool") {
            auto result = original;
            result["sdk"] = {{"name", "CyMCPSDK"}, {"version", cymcp::version}, {"revision", CYCODE_CYMCP_REVISION}};
            return result;
        }
        if (method == "ListOperations") return original.at("operations");
        if (method == "DescribeOperation") return contract(p.at("operationId"));
        if (method == "ListBackends") return original.value("backends", Json::array());
        if (method == "ListModels") return Json::array();
        if (method == "GetCapabilities") return original.value("capabilities", Json::array());
        if (method == "GetSystemResources") return {{"platform", cymcp::platform_id()}, {"cpuThreads", std::thread::hardware_concurrency()},
            {"jobRamBudgetBytes", options.jobRamBytes}, {"diskFreeBytes", std::filesystem::space(options.state).available}, {"gpus", Json::array()}};
        if (method == "Diagnose") return {{"sdk", "CyMCPSDK"}, {"version", cymcp::version},
            {"operationCount", operations.size()}, {"native", native("service.status", Json::object(), context)}};
        if (method == "WaitJob") return jobs->wait(p.at("jobId"), context);
        if (method == "SubmitJob" || method == "CanRun" || method == "EstimateResources" || method == "Invoke") {
            auto id = p.at("operationId").get<std::string>();
            auto parameters = p.value("parameters", Json::object());
            if (id == "core_invoke") { id = parameters.at("operationId"); parameters = parameters.at("parameters"); }
            if (!allowed(id, context)) throw cymcp::Error("AccessDenied", "Actual operation permission is not granted");
            const auto op = contract(id);
            validateContract(parameters, op.at("inputSchema"));
            if (p.value("backendId", "") != "" && p.value("backendId", "") != "native")
                throw cymcp::Error("InvalidParameters", "Only the native backend is available");
            if (!p.value("modelId", "").empty()) throw cymcp::Error("InvalidParameters", "The code engine has no inference model");
            if (method == "CanRun") return {{"canRun", true}, {"resources", op.value("resourceRequirements", Json::object())}};
            if (method == "EstimateResources") return op.value("resourceRequirements", Json::object());
            if (method == "Invoke") return canonical->invoke(id, parameters, context);
            p["operationId"] = id; p["parameters"] = parameters;
            if (!p.contains("sessionId")) {
                std::lock_guard lock(sessionMutex);
                auto& session = legacySessions[context.principal];
                if (session.empty()) session = jobs->call("CreateSession", {{"retentionSeconds", 86400}}, context).at("sessionId");
                p["sessionId"] = session;
            }
            return jobs->call("SubmitJob", p, context);
        }
        return jobs->call(method, p, context);
    }
};
Host::Host(NativeCall callback, Options options) : impl_(std::make_unique<Impl>(std::move(callback), std::move(options))) {}
Host::~Host() = default;
std::shared_ptr<cymcp::Registry> Host::registry() const { return impl_->canonical; }
std::shared_ptr<cymcp::Registry> Host::legacyRegistry() const { return impl_->legacyView; }
std::shared_ptr<cymcp::ResourceRegistry> Host::resources() const { return impl_->resourceView; }
Json Host::manifest() const { return impl_->original; }
Json Host::describe(const std::string& name) const { return impl_->contract(name); }
Json Host::invoke(const std::string& name, const Json& parameters, const cymcp::CallContext& context) {
    return impl_->canonical->invoke(name, parameters, context);
}
Json Host::legacy(const std::string& name, const Json& parameters, const cymcp::CallContext& context) {
    return impl_->legacyCall(name, parameters, context);
}
void Host::addPrincipal(std::string name, std::set<std::string> permissions) {
    std::lock_guard lock(impl_->principalMutex);
    impl_->principals[std::move(name)] = std::move(permissions);
}
void Host::bindStdioPrincipal(std::string principal) { impl_->stdioPrincipal = std::move(principal); }
std::unique_ptr<cymcp::McpServer> Host::mcpServer() const {
    const auto tools = impl_->gateway ? impl_->gateway->tools() :
        impl_->options.profile == "direct-full" ? impl_->canonical : impl_->legacyView;
    cymcp::McpServerOptions options;
    options.resources = impl_->resourceView; options.cache_ttl = impl_->options.cacheTtl;
    options.changes = impl_->changes; options.performance = impl_->performance;
    return std::make_unique<cymcp::McpServer>(*tools, "CyCodeCore", options);
}
void Host::serveStdio() {
    auto server = mcpServer();
    server->serve_stdio(std::cin, std::cout, impl_->options.workers);
}
void Host::stop() { impl_->jobs->stop(); }
} // namespace cycode::automation
