#include "Internal.h"
#include <cycode/Bridge.h>
#include <cycode/Tools.h>
#include <cycode/Updates.h>
#include <cymcp/http.hpp>
#include <cymcp/process.hpp>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace cycode::automation {
namespace {
volatile std::sig_atomic_t interrupted = 0;
void onSignal(int) { interrupted = 1; }
std::string environment(const char* name, std::string fallback = {}) {
    const auto* value = std::getenv(name);
    return value ? value : fallback;
}
void defaultEnvironment(const char* name, const std::string& value) {
    if (std::getenv(name)) return;
#ifdef _WIN32
    _wputenv_s(utf8Path(name).c_str(), utf8Path(value).c_str());
#else
    setenv(name, value.c_str(), 0);
#endif
}
std::set<std::string> permissions(const std::string& text) {
    std::set<std::string> result;
    std::istringstream input(text); std::string value;
    while (std::getline(input, value, ',')) if (!value.empty()) result.insert(value);
    return result;
}
cymcp::LocalAccessProfile accessProfile(std::string name) {
    std::replace(name.begin(), name.end(), '_', '-');
    if (name == "free") return cymcp::LocalAccessProfile::free;
    if (name == "local-controlled") return cymcp::LocalAccessProfile::local_controlled;
    if (name == "token-only") return cymcp::LocalAccessProfile::token_only;
    if (name == "protected" || name == "protected-access") return cymcp::LocalAccessProfile::protected_access;
    throw cymcp::Error("InvalidArguments", "Choose free, local-controlled, token-only or protected local access");
}
std::string accessProfileName(cymcp::LocalAccessProfile profile) {
    switch (profile) {
    case cymcp::LocalAccessProfile::free: return "free";
    case cymcp::LocalAccessProfile::local_controlled: return "local-controlled";
    case cymcp::LocalAccessProfile::token_only: return "token-only";
    case cymcp::LocalAccessProfile::protected_access: return "protected";
    }
    throw cymcp::Error("InvalidArguments", "Unknown local access profile");
}
std::filesystem::path cataloguePath(const std::string& executable) {
    const auto binary = std::filesystem::absolute(utf8Path(executable)).parent_path();
    for (const auto& path : {binary / "app/MCPMethods.json", binary / "MCPMethods.json",
            installationRoot() / "app/MCPMethods.json", std::filesystem::current_path() / "app/MCPMethods.json"})
        if (std::filesystem::is_regular_file(path)) return path;
    throw cymcp::Error("InvalidState", "Keep app/MCPMethods.json beside the native Core executable");
}
struct NativeOwner {
    std::filesystem::path workspace, descriptor, state;
    bool disablePlugins = false;
    std::mutex mutex;
    std::unique_ptr<NativeFileLease> installationLease;
    std::unique_ptr<Engine> engine;
    std::unique_ptr<Bridge> bridge;
    NativeCall caller;
    Json call(const std::string& method, const Json& input, const cymcp::CallContext& context) {
        {
            std::lock_guard lock(mutex);
            if (!caller) {
                if (!descriptor.empty()) caller = nativeEndpoint(descriptor);
                else {
                    if (workspace.empty() || !std::filesystem::is_directory(workspace))
                        throw cymcp::Error("InvalidWorkspace", "Choose an existing workspace or attach to a native descriptor");
                    const auto root = installationRoot();
                    defaultEnvironment("CYCODE_INDEX_AT_START", "1");
                    if (!root.empty()) installationLease = updates::installationUse(root);
                    engine = std::make_unique<Engine>(workspace, root / "data/settings/language-servers", root / "data/settings/snippets");
                    if (!disablePlugins && !root.empty()) engine->plugins().loadInstalled(root);
                    const auto path = state / ("native-" + cymcp::random_token(16) + ".json");
                    bridge = std::make_unique<Bridge>(*engine, path);
                    caller = nativeEndpoint(path);
                }
            }
        }
        return caller(method, input, context);
    }
};
Json httpCall(const std::string& url, const std::string& token, const std::string& method, const Json& p) {
    cymcp::HttpRequest request;
    request.url = url; request.method = "POST";
    request.headers = {{"Content-Type", "application/json"}};
    if (!token.empty()) request.headers["Authorization"] = "Bearer " + token;
    request.body = Json({{"protocolVersion", "1.0"}, {"id", 1}, {"method", method}, {"params", p}}).dump();
    request.max_response_bytes = 32 * 1024 * 1024; request.timeout = std::chrono::hours(25);
    const auto response = cymcp::HttpClient().perform(request);
    if (response.status != 200) throw cymcp::Error(response.status == 401 ? "AuthenticationRequired" : "ConnectionFailed",
        "Local SDK HTTP request failed with status " + std::to_string(response.status));
    const auto result = cymcp::parse_json(response.body, 32 * 1024 * 1024);
    if (result.contains("error")) throw OperationError(result.at("error"));
    return result.at("result");
}
Json failure(const std::exception& error) {
    if (const auto* native = dynamic_cast<const OperationError*>(&error)) return native->native;
    if (const auto* sdk = dynamic_cast<const cymcp::Error*>(&error))
        return {{"code", sdk->code}, {"message", sdk->what()}};
    return {{"code", "InvalidRequest"}, {"message", error.what()}};
}
}
void printHelp() {
    std::cout <<
        "CyCodeCore native automation (CyMCPSDK; no chat or Python MCP runtime)\n"
        "  cycode_cli --sdk [--workspace PROJECT] [--data-dir PRIVATE_STATE] COMMAND\n"
        "  Commands: describe, list-operations, describe-operation ID, invoke ID [JSON],\n"
        "            can-run ID [JSON], estimate ID [JSON], mcp, stdio, serve, create-client\n"
        "  Inputs: --params JSON | --params-file FILE; --timeout SECONDS\n"
        "  Local access: --local-access-profile free|local-controlled|token-only|protected (default free)\n"
        "  Controlled permissions: --permissions CSV; --access read|edit\n"
        "  MCP: --mcp-profile compact|direct-full|code|full (code/full preserve legacy jobs)\n"
        "  Existing instance: --native-descriptor FILE | --bridge-config FILE --access read|edit\n"
        "  HTTP: --service-descriptor PRIVATE_FILE; --connect URL (CYMCP_TOKEN for token profiles)\n"
        "  Independent options: --mcp-cache-ttl-ms N, --mcp-spool DIR, --mcp-telemetry FILE,\n"
        "       --mcp-notifications, --mcp-inline-bytes N, --mcp-result-ttl-seconds N,\n"
        "       --mcp-result-memory-bytes N, --mcp-result-disk-bytes N\n"
        "  Agent connections: --connection-state ROOT (private session; shared project index)\n"
        "  Artifact controls: --import-root DIR --export-root DIR with files.import/files.export\n"
        "  Durable jobs: --job-disk-bytes N, --job-ram-bytes N; no automatic replay\n"
        "  Legacy native commands remain: cycode_cli WORKSPACE METHOD [JSON],\n"
        "       WORKSPACE --stdio, WORKSPACE --serve DESCRIPTOR, --version\n";
}
int runCli(const std::vector<std::string>& arguments) {
    try {
        Options options;
        options.profile = environment("CYCODE_MCP_PROFILE", "compact");
        options.permissions = permissions(environment("CYCODE_PERMISSIONS"));
        std::string localProfile = environment("CYCODE_MCP_ACCESS_PROFILE");
        bool explicitPermissions = std::getenv("CYCODE_PERMISSIONS") != nullptr;
        std::filesystem::path workspace, nativeDescriptor, serviceDescriptor, bridgeConfig, connectionState;
        std::string access = "read", connect, token = environment("CYMCP_TOKEN", environment("CYTOOLS_TOKEN"));
        std::string command, jsonText = "{}", inputFile, backendId, modelId;
        std::vector<std::string> positionals;
        bool disablePlugins = false, wireOwner = false, legacyCli = false, explicitState = false;
        double timeout = 0;
        std::string clientName = "local";
        std::set<std::string> clientPermissions;
        int port = 0;
        for (std::size_t i = 1; i < arguments.size(); ++i) {
            const auto& argument = arguments[i];
            auto value = [&]() -> std::string {
                if (++i >= arguments.size()) throw cymcp::Error("InvalidArguments", "Missing value for " + argument);
                return arguments[i];
            };
            if (argument == "--sdk") continue;
            if (argument == "--legacy-cli") { legacyCli = true; continue; }
            if (argument == "--mcp") { command = "mcp"; continue; }
            if (argument == "--help" || argument == "-h") { printHelp(); return 0; }
            if (argument == "--sdk-download") {
                const auto result = downloadArtifact(cymcp::parse_json(value(), 65536));
                std::cout << Json({{"event", "result"}, {"value", result}}).dump() << '\n'; return 0;
            }
            if (argument == "--workspace" || argument == "--core-workspace") workspace = utf8Path(value());
            else if (argument == "--data-dir") { options.state = utf8Path(value()); explicitState = true; }
            else if (argument == "--connection-state") connectionState = utf8Path(value());
            else if (argument == "--import-root") options.importRoot = utf8Path(value());
            else if (argument == "--export-root") options.exportRoot = utf8Path(value());
            else if (argument == "--backend") backendId = value();
            else if (argument == "--model") modelId = value();
            else if (argument == "--native-descriptor") nativeDescriptor = utf8Path(value());
            else if (argument == "--service-descriptor") serviceDescriptor = utf8Path(value());
            else if (argument == "--bridge-config") bridgeConfig = utf8Path(value());
            else if (argument == "--mcp-profile") options.profile = value();
            else if (argument == "--local-access-profile") localProfile = value();
            else if (argument == "--permissions") { options.permissions = permissions(value()); explicitPermissions = true; }
            else if (argument == "--access") { access = value(); explicitPermissions = true; }
            else if (argument == "--disable-native-plugins" || argument == "--disable-plugins") disablePlugins = true;
            else if (argument == "--owner") { wireOwner = true; explicitPermissions = true; }
            else if (argument == "--connect") connect = value();
            else if (argument == "--params") jsonText = value();
            else if (argument == "--params-file") inputFile = value();
            else if (argument == "--timeout") timeout = std::stod(value());
            else if (argument == "--port") port = std::stoi(value());
            else if (argument == "--name") clientName = value();
            else if (argument == "--permission") clientPermissions.insert(value());
            else if (argument == "--mcp-spool") options.spoolDirectory = utf8Path(value());
            else if (argument == "--mcp-telemetry") options.telemetryFile = utf8Path(value());
            else if (argument == "--mcp-notifications") options.notifications = true;
            else if (argument == "--mcp-cache-ttl-ms") options.cacheTtl = std::chrono::milliseconds(std::stoll(value()));
            else if (argument == "--mcp-inline-bytes") options.gateway.inline_bytes = std::stoull(value());
            else if (argument == "--mcp-result-ttl-seconds") options.gateway.resources.result_ttl = std::chrono::seconds(std::stoll(value()));
            else if (argument == "--mcp-result-memory-bytes") options.gateway.resources.max_stored_bytes = std::stoull(value());
            else if (argument == "--mcp-result-disk-bytes") options.gateway.resources.max_disk_bytes = std::stoull(value());
            else if (argument == "--job-disk-bytes") options.jobDiskBytes = std::stoull(value());
            else if (argument == "--job-ram-bytes") options.jobRamBytes = std::stoull(value());
            else if (argument == "--catalogue") options.catalogue = utf8Path(value());
            else if (argument.starts_with("--")) throw cymcp::Error("InvalidArguments", "Unknown automation option " + argument);
            else positionals.push_back(argument);
        }
        if (command.empty()) {
            if (positionals.empty()) { printHelp(); return 2; }
            command = positionals.front(); positionals.erase(positionals.begin());
        }
        if (access != "read" && access != "edit") throw cymcp::Error("InvalidArguments", "Choose read or edit access");
        if (access == "edit") for (const auto* permission : {"workspace.write", "vcs.write", "process.execute", "plugins.execute"}) options.permissions.insert(permission);
        if (wireOwner) { options.principal = "local-owner"; options.permissions.insert("*"); }
        if (!connectionState.empty()) {
            connectionState = std::filesystem::absolute(connectionState);
            cymcp::private_directory(connectionState);
            defaultEnvironment("CYCODE_INDEX_DIR", pathUtf8(connectionState / "index"));
            options.state = connectionState / "sessions" / cymcp::random_token(16);
        }
        if (options.state.empty()) options.state = installationRoot() / "data/jobs";
        options.state = std::filesystem::absolute(options.state);
        const auto profilePath = options.state / "local-access.json";
        if (localProfile.empty() && explicitPermissions)
            localProfile = command == "serve" ? "protected" : "local-controlled";
        if (localProfile.empty() && std::filesystem::exists(profilePath)) {
            const auto saved = readJson(profilePath, 65536);
            if (saved.value("format", "") != "cycode-local-access/1")
                throw cymcp::Error("InvalidState", "Unknown local access settings format");
            localProfile = saved.value("profile", "free");
        }
        options.localAccessProfile = accessProfile(localProfile.empty() ? "free" : localProfile);
        if (options.catalogue.empty()) options.catalogue = cataloguePath(arguments.front());
        if (options.profile != "compact" && options.profile != "direct-full" && options.profile != "code" && options.profile != "full")
            throw cymcp::Error("InvalidArguments", "Unknown MCP presentation profile");
        if (timeout < 0 || options.cacheTtl.count() < 0 || options.gateway.resources.result_ttl.count() <= 0)
            throw cymcp::Error("InvalidArguments", "Invalid automation timing option");
        if (command == "serve" && serviceDescriptor.empty())
            throw cymcp::Error("InvalidArguments", "Use --service-descriptor to publish private endpoint credentials");
        if (!bridgeConfig.empty()) {
            const auto descriptor = readJson(bridgeConfig, 65536);
            if (descriptor.value("protocol", "") != "cycode-core/1") throw cymcp::Error("InvalidEndpoint", "Unsupported service descriptor");
            connect = descriptor.value(access == "edit" ? "editUrl" : "readUrl", descriptor.value("url", ""));
            token = descriptor.value(access == "edit" ? "editToken" : "readToken", "");
            if (localProfile.empty())
                options.localAccessProfile = accessProfile(descriptor.value("localAccessProfile", "protected"));
        }
        if (!connect.empty() && !explicitState && connectionState.empty())
            options.state = options.state.parent_path() / "proxy-sessions" / cymcp::random_token(16);
        auto owner = std::make_shared<NativeOwner>();
        owner->workspace = workspace; owner->descriptor = nativeDescriptor;
        owner->state = options.state; owner->disablePlugins = disablePlugins;
        NativeCall call = [owner](const std::string& method, const Json& p, const cymcp::CallContext& c) {
            return owner->call(method, p, c);
        };
        if (!connect.empty()) {
            // Accept the address shown by an MCP client, including /mcp or the base URL.
            const auto parsed = cymcp::parse_url(connect);
            if (parsed.target == "/mcp") connect.replace(connect.size() - 4, 4, "/v1");
            else if (parsed.target.empty() || parsed.target == "/") {
                while (connect.ends_with('/')) connect.pop_back();
                connect += "/v1";
            }
            auto remote = httpCall(connect, token, "DescribeTool", Json::object());
            const auto localCatalogue = readJson(options.catalogue);
            for (auto& operation : remote.at("operations")) if (!operation.contains("nativeMethod")) {
                const auto found = std::find_if(localCatalogue.at("operations").begin(), localCatalogue.at("operations").end(),
                    [&](const Json& candidate) { return candidate.at("id") == operation.at("id"); });
                if (found == localCatalogue.at("operations").end()) throw cymcp::Error("UnknownOperation", "Remote operation has no compatible native contract");
                operation["nativeMethod"] = found->at("nativeMethod");
            }
            cymcp::private_directory(options.state);
            options.catalogue = options.state / "remote-catalogue.json";
            saveJson(options.catalogue, remote);
            std::map<std::string, std::string> names;
            std::set<std::string> directReads;
            const bool nativeSdk = remote.contains("sdk") && remote.at("sdk").value("name", "") == "CyMCPSDK";
            for (const auto& op : remote.at("operations")) {
                const auto method = op.at("nativeMethod").get<std::string>();
                names[method] = op.at("id");
                if (nativeSdk && op.value("permissions", Json::array()).empty() && !op.value("supportsProgress", false)) directReads.insert(method);
            }
            options.routeNative = false; // The remote registry owns native-only routing fields.
            call = [connect, token, names, directReads](const std::string& method, const Json& p, const cymcp::CallContext& context) {
                const auto found = names.find(method);
                if (found == names.end()) throw cymcp::Error("UnknownOperation", "Operation is absent from the connected service");
                if (directReads.contains(method))
                    return httpCall(connect, token, "Invoke", {{"operationId", found->second}, {"parameters", p}});
                const auto issued = httpCall(connect, token, "SubmitJob", {{"operationId", found->second}, {"parameters", p}});
                const auto id = issued.at("jobId");
                auto job = issued;
                bool cancelled = false;
                while (!terminal(job.value("state", ""))) {
                    bool requested = context.cancellation.cancelled();
                    try { context.cancellation.check(); } catch (const cymcp::Error&) { requested = true; }
                    if (requested && !cancelled && job.value("canCancel", true)) {
                        httpCall(connect, token, "CancelJob", {{"jobId", id}}); cancelled = true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    job = httpCall(connect, token, "GetJob", {{"jobId", id}});
                }
                if (job.value("state", "") != "Completed") throw OperationError(job.at("error"));
                return job.at("outputs");
            };
        }
        if (std::filesystem::exists(options.state / "state.sqlite3") && !std::filesystem::exists(options.state / "legacy-import.json")) {
            // One-time compatibility conversion only. Normal CLI/MCP never runs Python.
            const auto home = installationRoot();
#ifdef _WIN32
            const auto python = home / "tools/python/python.exe";
#else
            const auto python = home / "tools/python/bin/python3";
#endif
            const auto script = home / "app/scripts/migrate_legacy_jobs.py";
            if (std::filesystem::is_regular_file(python) && std::filesystem::is_regular_file(script)) {
                cymcp::ProcessOptions migration;
                migration.executable = python; migration.cwd = home;
                migration.arguments = {"-B", pathUtf8(script), "--state-dir", pathUtf8(options.state)};
                migration.max_line_bytes = 16384; migration.max_queue_bytes = 65536;
                cymcp::Process process(migration); process.close_input();
                cymcp::Cancellation limit; limit.deadline = cymcp::Clock::now() + std::chrono::minutes(2);
                cymcp::ProcessLine line;
                while (process.next(line, limit)) { /* Never write conversion logs into MCP stdout. */ }
                if (process.wait(limit) != 0) throw cymcp::Error("MigrationRequired", "Legacy conversion failed; original data is preserved. Run the helper manually for diagnostics.");
            }
        }
        Host host(call, options);
        cymcp::CallContext context;
        context.principal = options.principal;
        if (connect.empty() && !token.empty()) {
            const auto identity = host.legacy("Authenticate", {{"token", token}}, context);
            context.principal = identity.at("clientId");
            std::set<std::string> rights;
            for (const auto& right : identity.at("permissions")) rights.insert(right.get<std::string>());
            host.addPrincipal(context.principal, rights);
        }
        if (timeout) context.cancellation.deadline = cymcp::Clock::now() + std::chrono::milliseconds(static_cast<long long>(timeout * 1000));
        auto parameters = [&]() {
            if (!inputFile.empty()) return readJson(utf8Path(inputFile), 8 * 1024 * 1024);
            if (!positionals.empty()) return cymcp::parse_json(positionals.back(), 8 * 1024 * 1024);
            return cymcp::parse_json(jsonText, 8 * 1024 * 1024);
        };
        Json result;
        if (command == "mcp") { host.bindStdioPrincipal(context.principal); host.serveStdio(); return 0; }
        if (command == "describe") result = host.legacy("DescribeTool", {}, context);
        else if (command == "list-operations") result = host.legacy("ListOperations", {}, context);
        else if (command == "list-backends") result = host.legacy("ListBackends", {}, context);
        else if (command == "list-models") result = host.legacy("ListModels", {}, context);
        else if (command == "diagnose") result = host.legacy("Diagnose", {}, context);
        else if (command == "describe-operation") {
            if (positionals.size() != 1) throw cymcp::Error("InvalidArguments", "Choose exactly one operation");
            result = host.describe(positionals.front());
        } else if (command == "invoke" || command == "can-run" || command == "estimate") {
            if (positionals.empty()) throw cymcp::Error("InvalidArguments", "Choose a canonical operation");
            const auto id = positionals.front(); positionals.erase(positionals.begin());
            const auto args = parameters();
            host.legacy("CanRun", {{"operationId", id}, {"parameters", args}, {"backendId", backendId}, {"modelId", modelId}}, context);
            if (command == "invoke" && legacyCli) {
                const auto job = host.legacy("SubmitJob", {{"operationId", id}, {"parameters", args}, {"backendId", backendId}, {"modelId", modelId}}, context);
                result = host.legacy("WaitJob", {{"jobId", job.at("jobId")}}, context);
                std::cout << result.dump(2) << '\n';
                return result.value("state", "") == "Completed" ? 0 : 1;
            }
            result = command == "invoke" ? host.invoke(id, args, context) : host.legacy(command == "can-run" ? "CanRun" : "EstimateResources",
                {{"operationId", id}, {"parameters", args}, {"backendId", backendId}, {"modelId", modelId}}, context);
        } else if (command == "create-client") {
            context.principal = "local-owner";
            result = host.legacy("CreateClient", {{"name", clientName}, {"permissions", clientPermissions}}, context);
        } else if (command == "stdio" || command == "jsonl") {
            std::string line;
            while (std::getline(std::cin, line)) {
                Json response, id = nullptr;
                try {
                    const auto input = cymcp::parse_json(line, 8 * 1024 * 1024);
                    id = input.value("id", Json(nullptr));
                    auto actor = context;
                    if (input.contains("authToken")) {
                        const auto who = host.legacy("Authenticate", {{"token", input.at("authToken")}}, context);
                        actor.principal = who.at("clientId");
                        std::set<std::string> rights;
                        for (const auto& right : who.at("permissions")) rights.insert(right.get<std::string>());
                        host.addPrincipal(actor.principal, rights);
                    }
                    response = {{"protocolVersion", "1.0"}, {"id", id}, {"result",
                        host.legacy(input.at("method"), input.value("params", Json::object()), actor)}};
                } catch (const std::exception& error) { response = {{"protocolVersion", "1.0"}, {"id", id}, {"error", failure(error)}}; }
                std::cout << response.dump() << '\n' << std::flush;
            }
            return 0;
        } else if (command == "serve") {
            std::signal(SIGINT, onSignal); std::signal(SIGTERM, onSignal);
            const auto settings = cymcp::local_access_settings(options.localAccessProfile);
            const auto readToken = settings.require_token ? cymcp::random_token(32) : std::string{};
            const auto editToken = settings.require_token ? cymcp::random_token(32) : std::string{};
            host.addPrincipal("service-read", {});
            host.addPrincipal("service-edit", {"workspace.write", "vcs.write", "process.execute", "plugins.execute"});
            // Controlled roles keep separate endpoints; permissive profiles allow
            // every registered operation on either endpoint. JSON cannot grant roles.
            std::shared_ptr<cymcp::McpServer> mcp = host.mcpServer();
            auto endpoint = [&](const std::string& role) {
                return [&, role, mcp](const Json& input, const cymcp::CallContext& incoming) -> Json {
                    auto actor = incoming; actor.principal = role;
                    if (input.contains("jsonrpc")) return mcp->handle(input, actor);
                    try {
                        return {{"protocolVersion", "1.0"}, {"id", input.value("id", Json(nullptr))},
                            {"result", host.legacy(input.at("method"), input.value("params", Json::object()), actor)}};
                    } catch (const std::exception& error) {
                        return {{"protocolVersion", "1.0"}, {"id", input.value("id", Json(nullptr))}, {"error", failure(error)}};
                    }
                };
            };
            auto server = [&](const std::string& credential, const std::string& role) {
                if (settings.require_token)
                    return std::make_unique<cymcp::LocalHttpServer>(credential, endpoint(role));
                return std::make_unique<cymcp::LocalHttpServer>(cymcp::LocalHttpServer::UnauthenticatedLoopback{}, endpoint(role));
            };
            auto reader = server(readToken, "service-read"), editor = server(editToken, "service-edit");
            const auto readPort = reader->start(port), editPort = editor->start(0);
            const auto readBase = "http://127.0.0.1:" + std::to_string(readPort);
            const auto editBase = "http://127.0.0.1:" + std::to_string(editPort);
            if (serviceDescriptor.empty()) throw cymcp::Error("InvalidArguments", "Use --service-descriptor to publish private endpoint credentials");
            saveJson(serviceDescriptor, {{"protocol", "cycode-core/1"}, {"sdk", "CyMCPSDK"},
                {"url", readBase + "/v1"}, {"readUrl", readBase + "/v1"}, {"editUrl", editBase + "/v1"},
                {"mcpReadUrl", readBase + "/mcp"}, {"mcpEditUrl", editBase + "/mcp"},
                {"localAccessProfile", accessProfileName(options.localAccessProfile)},
                {"requireToken", settings.require_token}, {"controlOperations", settings.control_operations},
                {"readToken", readToken}, {"editToken", editToken}});
            saveJson(profilePath, {{"format", "cycode-local-access/1"}, {"profile", accessProfileName(options.localAccessProfile)}});
            std::cerr << "CyCodeCore SDK endpoints ready; local access profile: " << accessProfileName(options.localAccessProfile) << '\n';
            while (!interrupted && (nativeDescriptor.empty() || std::filesystem::exists(nativeDescriptor)))
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            mcp->cancel_all(); host.stop(); editor->stop(); reader->stop();
            std::filesystem::remove(serviceDescriptor);
            return 0;
        } else throw cymcp::Error("InvalidArguments", "Unknown automation command; use --help");
        std::cout << result.dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << Json({{"error", failure(error)}}).dump() << '\n';
        return 1;
    }
}
} // namespace cycode::automation
