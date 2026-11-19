#include "Internal.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <cycode/Tools.h>

namespace cycode::automation {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr auto badSocket = INVALID_SOCKET;
void closeSocket(Socket value) { closesocket(value); }
bool timeoutError() { return WSAGetLastError() == WSAETIMEDOUT || WSAGetLastError() == WSAEWOULDBLOCK; }
struct Sockets { Sockets() { WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data)) throw cymcp::Error("NetworkError", "Winsock unavailable"); } ~Sockets() { WSACleanup(); } };
#else
using Socket = int;
constexpr auto badSocket = -1;
void closeSocket(Socket value) { close(value); }
bool timeoutError() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
struct Sockets {};
#endif
struct Connection {
    Socket value = badSocket;
    ~Connection() { if (value != badSocket) closeSocket(value); }
};
bool longOperation(const std::string& method) {
    static const std::set<std::string> methods{"analysis.prepare", "language.rename", "analysis.inspect",
        "build.configure", "build.run", "build.presetRun", "msbuild.build", "msbuild.rebuild", "msbuild.clean",
        "msbuild.restore", "msbuild.evaluate", "msbuild.prepare", "unreal.generate", "unreal.database", "unreal.build",
        "unreal.prepare", "unreal.automationRun", "tests.run", "tests.discover", "debug.hotReload"};
    return methods.contains(method) || (method.starts_with("project.") && method != "project.inspect" &&
        method != "project.cancel" && method != "project.status" && method != "project.output");
}
std::string cancellationMethod(const std::string& method) {
    if (method == "analysis.prepare" || method == "language.rename") return "analysis.cancel";
    if (method == "debug.hotReload") return "debug.cancelReload";
    if (method == "tests.run" || method == "tests.discover") return "tests.cancel";
    if (method.starts_with("project.")) return "project.cancel";
    return "build.cancel";
}
Json exchange(const std::filesystem::path& path, const std::string& method, const Json& parameters,
              const cymcp::CallContext& context, bool control = false) {
    static Sockets sockets;
    (void)sockets;
    const auto endpoint = readJson(path, 64 * 1024);
    if (endpoint.value("protocol", "") != "cycode-native/1" || endpoint.value("host", "") != "127.0.0.1" ||
        !endpoint.contains("token") || !endpoint["token"].is_string())
        throw cymcp::Error("InvalidEndpoint", "Expected an authenticated local native Core descriptor");
    const auto port = endpoint.at("port").get<int>();
    if (port < 1 || port > 65535) throw cymcp::Error("InvalidEndpoint", "Invalid native port");
    Connection connection;
    connection.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection.value == badSocket) throw cymcp::Error("NetworkError", "Cannot create native connection");
#ifdef _WIN32
    DWORD timeout = 250;
#else
    timeval timeout{0, 250000};
#endif
    setsockopt(connection.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(connection.value, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_port = htons(static_cast<unsigned short>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(connection.value, reinterpret_cast<const sockaddr*>(&address), sizeof(address)))
        throw cymcp::Error("NativeUnavailable", "Cannot connect to the selected native Core");
    Json input = parameters;
    const bool cancellable = !control && longOperation(method) &&
        !method.ends_with(".status") && !method.ends_with(".cancel") && !method.ends_with(".output");
    std::string operation;
    if (cancellable) {
        operation = method == "debug.hotReload" ? input.value("operationId", cymcp::random_token(16)) : cymcp::random_token(16);
        input["operationId"] = operation;
    }
    const auto request = Json({{"token", endpoint.at("token")}, {"method", method}, {"params", input}}).dump() + '\n';
    if (request.size() > 8 * 1024 * 1024) throw cymcp::Error("RequestTooLarge", "Native request exceeds its transport budget");
    for (std::size_t offset = 0; offset < request.size();) {
        const auto count = send(connection.value, request.data() + offset,
            static_cast<int>((std::min)(request.size() - offset, std::size_t(65536))), 0);
        if (count <= 0) throw cymcp::Error("NativeUnavailable", "Native request was not delivered");
        offset += static_cast<std::size_t>(count);
    }
    std::string response;
    char buffer[65536];
    bool cancellationSent = false;
    auto nextProgress = cymcp::Clock::now() + std::chrono::seconds(1);
    const auto deadline = cymcp::Clock::now() + std::chrono::hours(25);
    while (response.find('\n') == std::string::npos) {
        const auto count = recv(connection.value, buffer, sizeof(buffer), 0);
        if (count < 0 && timeoutError()) {
            bool cancelled = context.cancellation.cancelled();
            try { context.cancellation.check(); } catch (const cymcp::Error&) { cancelled = true; }
            if (cancelled && cancellable && !cancellationSent) {
                try {
                    cymcp::CallContext stop;
                    exchange(path, cancellationMethod(method), {{"operationId", operation}}, stop, true);
                    cancellationSent = true;
                } catch (...) { /* Retry while retaining ownership of the actual call. */ }
            }
            if (!control && context.events && (method == "analysis.prepare" || method == "language.rename") && cymcp::Clock::now() >= nextProgress) {
                nextProgress = cymcp::Clock::now() + std::chrono::seconds(1);
                try {
                    cymcp::CallContext probe;
                    auto state = exchange(path, "analysis.status", {{"operationId", operation}}, probe, true);
                    context.events({"Progress", {{"percent", state.value("progress", 0.0)},
                        {"currentStep", state.value("phase", "")}, {"currentStepIndex", state.value("completed", 0)},
                        {"stepCount", (std::max)(1, state.value("total", 1))}}});
                } catch (...) { /* Progress cannot abandon a live native operation. */ }
            }
            if (control || cymcp::Clock::now() > deadline)
                throw cymcp::Error("Timeout", "Native control did not complete; inspect current state before retrying");
            continue;
        }
        if (count <= 0) throw cymcp::Error("NativeUnavailable", "Native Core disconnected; do not replay an uncertain mutation");
        response.append(buffer, static_cast<std::size_t>(count));
        if (response.size() > 20 * 1024 * 1024) throw cymcp::Error("ResponseTooLarge", "Use the existing paged native operation");
    }
    const auto value = cymcp::parse_json(response, 20 * 1024 * 1024);
    if (value.contains("error")) throw OperationError(value.at("error"));
    return value.at("result");
}
}
NativeCall nativeEndpoint(const std::filesystem::path& descriptor) {
    return [descriptor](const std::string& method, const Json& arguments, const cymcp::CallContext& context) {
        return exchange(descriptor, method, arguments, context);
    };
}
} // namespace cycode::automation
