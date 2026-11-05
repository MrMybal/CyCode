#include "cycode/JavaDebug.h"
#include "cycode/DebugAdapters.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cycode {
using Json = nlohmann::json;
Json javaDebugAttachDefaults() {
    return {{"hostName", "127.0.0.1"}, {"port", 5005}, {"timeout", 5000}};
}
void validateJavaDebugAttach(const Json &value) {
    if (!value.is_object() || value.size() != 3 || !value.contains("hostName") || !value.contains("port") ||
        !value.contains("timeout"))
        throw std::runtime_error("Java attach requires hostName, port and timeout");
    if (!value.at("hostName").is_string())
        throw std::runtime_error("Java attach host must be text");
    const auto &host = value.at("hostName").get_ref<const std::string &>();
    if (host.empty() || host.size() > 253 ||
        host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.:%-") !=
            std::string::npos)
        throw std::runtime_error(
            "Use a Java attach hostname or unbracketed IP address, without a URL or port");
    if (host.find(':') == std::string::npos) {
        if (host.find('%') != std::string::npos)
            throw std::runtime_error("Invalid Java attach hostname");
    } else {
        const auto scope = host.find('%');
        if (std::count(host.begin(), host.end(), ':') < 2 ||
            host.substr(0, scope).find_first_not_of("0123456789abcdefABCDEF:.") != std::string::npos ||
            (scope != std::string::npos &&
             (scope + 1 == host.size() ||
              host.substr(scope + 1).find_first_not_of(
                  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") != std::string::npos)))
            throw std::runtime_error("Use an unbracketed IPv6 Java attach address with an optional scope");
    }
    if (!value.at("port").is_number_integer() || value.at("port") < 1 || value.at("port") > 65535)
        throw std::runtime_error("Java attach port must be between 1 and 65535");
    if (!value.at("timeout").is_number_integer() || value.at("timeout") < 100 || value.at("timeout") > 10000)
        throw std::runtime_error("Java attach timeout must be between 100 and 10000 milliseconds");
}
Json javaDebugAttachRequest(const Json &profile, const std::filesystem::path &cwd) {
    if (profile.contains("pid"))
        throw std::runtime_error("Java attach uses a JDWP host and port, not a process ID");
    auto result = profile.value("javaAttach", javaDebugAttachDefaults());
    validateJavaDebugAttach(result);
    const auto options = profile.value("adapterOptions", Json::object());
    validateJavaDebugOptions(options);
    for (const auto *key : {"projectName", "sourcePaths"})
        if (options.contains(key))
            result[key] = options.at(key);
    if (result.contains("sourcePaths"))
        for (auto &entry : result["sourcePaths"]) {
            auto path = utf8Path(entry.get<std::string>());
            if (!path.is_absolute())
                path = cwd / path;
            if (!std::filesystem::is_directory(path))
                throw std::runtime_error("Java attach source directory does not exist: " + pathUtf8(path));
            entry = pathUtf8(std::filesystem::canonical(path));
        }
    result["type"] = "java";
    result["request"] = "attach";
    if (result.dump().size() > 65536)
        throw std::runtime_error("Java attach request exceeds 64 KiB");
    return result;
}
std::string javaDebugArguments(const Json &arguments, bool windows) {
    if (!arguments.is_array() || arguments.size() > 128)
        throw std::runtime_error("Invalid Java debug argument list");
    std::string result;
    for (const auto &entry : arguments) {
        if (!entry.is_string())
            throw std::runtime_error("Invalid Java debug argument");
        const auto &text = entry.get_ref<const std::string &>();
        if (text.size() > 8192 || text.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Java debug argument");
        if (!result.empty())
            result += ' ';
        result += '"';
        if (windows) {
            size_t slashes = 0;
            for (const char c : text) {
                if (c == '\\') {
                    ++slashes;
                    continue;
                }
                result.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
                result += c;
                slashes = 0;
            }
            result.append(slashes * 2, '\\');
        } else {
            for (const char c : text) {
                if (c == '"' || c == '\\')
                    result += '\\';
                result += c;
            }
        }
        result += '"';
        if (result.size() > 65536)
            throw std::runtime_error("Encoded Java arguments exceed 64 KiB");
    }
    return result;
}
void validateJavaDebugOptions(const Json &value, bool requireMain) {
    validateDebugAdapterOptions(value);
    const std::set<std::string> keys = {"mainClass",  "projectName", "javaExec",
                                        "classPaths", "sourcePaths", "vmArgs"};
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!keys.contains(it.key()))
            throw std::runtime_error("Unknown Java debug option: " + it.key());
        if (it.key() == "classPaths" || it.key() == "sourcePaths" || it.key() == "vmArgs") {
            if (!it->is_array() || it->size() > 128)
                throw std::runtime_error("Java debug option requires up to 128 strings");
            for (const auto &item : *it)
                if (!item.is_string() ||
                    (it.key() != "vmArgs" && item.get_ref<const std::string &>().empty()))
                    throw std::runtime_error("Invalid Java debug argument/path");
        } else if (!it->is_string())
            throw std::runtime_error("Java debug option requires text");
    }
    const auto main = value.value("mainClass", std::string{});
    if ((requireMain && main.empty()) || main.size() > 4096 ||
        main.find_first_of("/\\:;") != std::string::npos ||
        std::any_of(main.begin(), main.end(), [](unsigned char c) { return c <= 32 || c == 127; }))
        throw std::runtime_error("Set a fully qualified Java main class in the debug profile");
}
Json javaDebugRequest(const Json &profile, const std::filesystem::path &cwd) {
    auto result = profile.at("adapterOptions");
    validateJavaDebugOptions(result, true);
    if (profile.value("terminal", "integrated") == "external")
        throw std::runtime_error("Java debugging supports the integrated terminal or debug console");
    // Never let the adapter infer an unintended project/classpath from stale LSP models.
    if (!result.contains("classPaths") || result.at("classPaths").empty())
        throw std::runtime_error("Java debugging requires explicit runtime classPaths; build a Project tasks "
                                 "target or configure them");
    for (auto key : {"classPaths", "sourcePaths"})
        if (result.contains(key))
            for (auto &entry : result[key]) {
                auto path = utf8Path(entry.get<std::string>());
                if (!path.is_absolute())
                    path = cwd / path;
                if (std::string(key) == "sourcePaths" && !std::filesystem::is_directory(path))
                    throw std::runtime_error("Java debug source directory does not exist: " + pathUtf8(path));
                if (std::string(key) == "classPaths") {
#ifdef _WIN32
                    constexpr char separator = ';';
#else
                    constexpr char separator = ':';
#endif
                    if (pathUtf8(path).find(separator) != std::string::npos ||
                        pathUtf8(path).find('*') != std::string::npos)
                        throw std::runtime_error("Unrepresentable or wildcard Java debug classpath entry");
                }
                // Gradle runtime models include resources directories even when no resources were produced.
                // Preserve those literal entries, as the JVM does, rather than dropping them or refusing
                // launch.
                entry = pathUtf8(std::filesystem::weakly_canonical(path));
            }
    if (result.contains("javaExec") && !result.at("javaExec").get_ref<const std::string &>().empty()) {
        auto path = utf8Path(result.at("javaExec"));
        if (!path.is_absolute())
            path = cwd / path;
        if (!std::filesystem::is_regular_file(path))
            throw std::runtime_error("Java debug executable does not exist");
        result["javaExec"] = pathUtf8(std::filesystem::canonical(path));
        if (profile.at("terminal") == "console" &&
            utf8Path(result.at("javaExec")).parent_path().filename() != "bin")
            throw std::runtime_error("Java debug console requires a JDK bin/java executable; use the "
                                     "integrated terminal for custom launchers");
    }
    result["type"] = "java";
    result["request"] = "launch";
    result["noDebug"] = false;
    result["cwd"] = pathUtf8(cwd);
#ifdef _WIN32
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif
    result["args"] = javaDebugArguments(profile.at("args"), windows);
    result["vmArgs"] = javaDebugArguments(result.value("vmArgs", Json::array()), windows);
    result["env"] = profile.at("env");
    if (profile.at("terminal") == "console")
        for (const auto &entry : result.at("env"))
            if (entry.is_null())
                throw std::runtime_error("Java environment removals require the integrated terminal");
    result["stopOnEntry"] = profile.at("stopOnEntry");
    result["console"] = profile.at("terminal") == "console" ? "internalConsole" : "integratedTerminal";
    result["shortenCommandLine"] = "none";
    if (result.dump().size() > 65536)
        throw std::runtime_error("Java debug request exceeds 64 KiB");
    return result;
}
Json javaDebugTarget(const Json &artifact, const std::filesystem::path &workspace, const std::string &java) {
    Json paths = artifact.value("classPathEntries", Json::array());
    if (artifact.contains("classPath"))
        paths.push_back(pathUtf8(workspace / utf8Path(artifact.at("classPath"))));
    for (auto &entry : paths) {
        auto path = utf8Path(entry.get<std::string>());
        if (!path.is_absolute())
            path = workspace / path;
        entry = pathUtf8(path.lexically_normal());
    }
    Json result = {{"mainClass", artifact.at("mainClass")}, {"classPaths", paths}, {"javaExec", java}};
    validateJavaDebugOptions(result, true);
    return result;
}
bool javaDebugAvailable(const Json &capabilities) {
    const auto provider = capabilities.value("executeCommandProvider", Json());
    if (!provider.is_object())
        return false;
    const auto commands = provider.value("commands", Json::array());
    return commands.is_array() &&
           std::find(commands.begin(), commands.end(), "vscode.java.startDebugSession") != commands.end();
}
Json javaIncrementalBuildParams() {
    // JSON-RPC positional arguments: the JDT method takes one boolean.
    return Json::array({false});
}
void validateJavaBuildResult(const Json &result) {
    if (!result.is_number_integer() || result < 0 || result > 3)
        throw std::runtime_error("Invalid JDT workspace build result");
    if (result == 1) return;
    if (result == 2) throw std::runtime_error("JDT workspace has compilation errors; Java reload was not requested");
    if (result == 3) throw std::runtime_error("JDT workspace build was cancelled; Java reload was not requested");
    throw std::runtime_error("JDT workspace build failed; Java reload was not requested");
}
Json javaHotReloadResult(const Json &reply) {
    if (!reply.is_object() || reply.dump().size() > 262144)
        throw std::runtime_error("Invalid Java reload response");
    if (reply.contains("errorMessage") && !reply.at("errorMessage").is_null()) {
        const auto &error = reply.at("errorMessage");
        if (!error.is_string() || error.get_ref<const std::string &>().size() > 8192 ||
            error.get_ref<const std::string &>().find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Java reload error response");
        if (!error.get_ref<const std::string &>().empty()) throw std::runtime_error("Java reload: " + error.get<std::string>());
    }
    if (!reply.contains("changedClasses") || !reply.at("changedClasses").is_array() || reply.at("changedClasses").size() > 2048)
        throw std::runtime_error("Invalid Java reload class list");
    for (const auto &name : reply.at("changedClasses"))
        if (!name.is_string() || name.get_ref<const std::string &>().empty() ||
            name.get_ref<const std::string &>().size() > 4096 || name.get_ref<const std::string &>().find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Java reload class name");
    return {{"status", reply.at("changedClasses").empty() ? "unchanged" : "applied"},
            {"changedClasses", reply.at("changedClasses")}, {"message", ""}};
}
Json javaHotReloadEvent(const Json &body) {
    if (!body.is_object() || !body.contains("changeType") || !body.at("changeType").is_string())
        throw std::runtime_error("Invalid Java reload event");
    const auto type = body.at("changeType").get<std::string>();
    if (type != "ERROR" && type != "WARNING" && type != "STARTING" && type != "END" && type != "BUILD_COMPLETE")
        throw std::runtime_error("Unknown Java reload event");
    const auto message = body.value("message", Json()).is_null() ? Json("") : body.at("message");
    if (!message.is_string() || message.get_ref<const std::string &>().size() > 8192 ||
        message.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid Java reload event message");
    return {{"changeType", type}, {"message", message}};
}
} // namespace cycode
