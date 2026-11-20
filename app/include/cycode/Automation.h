#pragma once
#include <cymcp/core.hpp>
#include <cymcp/resources.hpp>
#include <cymcp/mcp.hpp>
#include <cymcp/http.hpp>
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

namespace cycode::automation {
using Json = cymcp::Json;
using NativeCall = std::function<Json(const std::string&, const Json&, const cymcp::CallContext&)>;
struct Options {
    std::filesystem::path state, catalogue, spoolDirectory, telemetryFile, importRoot, exportRoot;
    std::string profile = "compact", principal = "local";
    std::set<std::string> permissions;
    cymcp::LocalAccessProfile localAccessProfile = cymcp::LocalAccessProfile::free;
    bool notifications = false, routeNative = true;
    std::size_t workers = 4, queueCapacity = 128;
    std::size_t jobDiskBytes = 1024ULL * 1024 * 1024;
    std::size_t jobRamBytes = 512ULL * 1024 * 1024;
    std::chrono::milliseconds cacheTtl{30000};
    cymcp::ToolGatewayOptions gateway;
};
class Host {
    struct Impl;
    std::unique_ptr<Impl> impl_;
  public:
    Host(NativeCall, Options);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    std::shared_ptr<cymcp::Registry> registry() const;
    std::shared_ptr<cymcp::Registry> legacyRegistry() const;
    std::shared_ptr<cymcp::ResourceRegistry> resources() const;
    Json manifest() const;
    Json describe(const std::string&) const;
    Json invoke(const std::string&, const Json&, const cymcp::CallContext& = {});
    Json legacy(const std::string&, const Json&, const cymcp::CallContext& = {});
    void addPrincipal(std::string, std::set<std::string>);
    void bindStdioPrincipal(std::string);
    std::unique_ptr<cymcp::McpServer> mcpServer() const;
    void serveStdio();
    void stop();
};
// A transport adapter to the actual Engine owned by an existing application.
NativeCall nativeEndpoint(const std::filesystem::path& descriptor);
int runCli(const std::vector<std::string>&);
void printHelp();
Json downloadArtifact(const Json&);
} // namespace cycode::automation
