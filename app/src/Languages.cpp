#include "cycode/Languages.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
std::string extension(const std::string &path) {
    const auto dot = path.find_last_of('.');
    auto ext = dot == std::string::npos ? "" : path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return ext;
}
bool oneOf(const std::string &value, std::initializer_list<const char *> list) {
    for (auto item : list)
        if (value == item)
            return true;
    return false;
}
} // namespace
const std::vector<std::string> &languageBackends() {
    static const std::vector<std::string> keys = {"cpp",  "csharp", "rust",   "java", "typescript",
                                                  "html", "css",    "python", "hlsl", "glsl"};
    return keys;
}
std::string languageBackend(const std::string &path) {
    const auto ext = extension(path);
    if (oneOf(ext, {".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx", ".inl"}))
        return "cpp";
    if (ext == ".cs")
        return "csharp";
    if (ext == ".rs")
        return "rust";
    if (ext == ".java")
        return "java";
    if (oneOf(ext, {".js", ".jsx", ".mjs", ".cjs", ".ts", ".tsx", ".mts", ".cts"}))
        return "typescript";
    if (ext == ".html" || ext == ".htm")
        return "html";
    if (ext == ".css")
        return "css";
    if (oneOf(ext, {".py", ".pyi", ".pyw"}))
        return "python";
    if (oneOf(ext, {".hlsl", ".hlsli", ".fx", ".fxh"}))
        return "hlsl";
    if (oneOf(ext, {".glsl", ".vert", ".frag", ".geom", ".tesc", ".tese", ".comp"}))
        return "glsl";
    return "";
}
std::string languageId(const std::string &path) {
    const auto ext = extension(path);
    auto backend = languageBackend(path);
    if (backend == "cpp" && ext == ".c")
        return "c";
    if (backend == "typescript") {
        if (ext == ".tsx")
            return "typescriptreact";
        if (ext == ".jsx")
            return "javascriptreact";
        return oneOf(ext, {".js", ".mjs", ".cjs"}) ? "javascript" : "typescript";
    }
    return backend;
}
bool languageUsesLegacyLexer(const std::string &path) {
    const auto backend = languageBackend(path);
    return oneOf(backend, {"cpp", "csharp", "rust", "java", "typescript", "html", "css"});
}
std::string languageServerName(const std::string &backend) {
    if (backend == "cpp")
        return "C++ / clangd";
    if (backend == "csharp")
        return "C# / Roslyn";
    if (backend == "rust")
        return "Rust / rust-analyzer";
    if (backend == "java")
        return "Java / Eclipse JDT LS";
    if (backend == "typescript")
        return "JavaScript / TypeScript";
    if (backend == "html")
        return "HTML";
    if (backend == "css")
        return "CSS";
    if (backend == "python")
        return "Python / Pyright";
    if (backend == "hlsl")
        return "HLSL / HLSL Tools";
    if (backend == "glsl")
        return "GLSL / GLSL Analyzer";
    return backend;
}
ServerLaunch defaultServerLaunch(const std::filesystem::path &workspace, const std::string &backend) {
    ServerLaunch launch;
    const auto root = installationRoot() / "runtime/tools";
    if (backend == "python") {
        auto executable = toolPath("pyright-langserver");
        if (!executable.empty())
            launch.argv = {executable, "--stdio"};
        const auto node = toolPath("node");
        const auto script = root / "python-languages/node_modules/pyright/langserver.index.js";
        if (launch.argv.empty() && !node.empty() && std::filesystem::is_regular_file(script))
            launch.argv = {node, pathUtf8(script), "--stdio"};
        launch.settings = {{"python",
                            {{"analysis",
                              {{"autoSearchPaths", true},
                               {"diagnosticMode", "openFilesOnly"},
                               {"typeCheckingMode", "basic"}}}}}};
    } else if (backend == "hlsl" || backend == "glsl") {
        auto executable = toolPath(backend == "hlsl" ? "HlslLanguageServer" : "glsl_analyzer");
#ifdef _WIN32
        const auto bundled = root / "glsl-analyzer/bin/glsl_analyzer.exe";
#else
        const auto bundled = root / "glsl-analyzer/bin/glsl_analyzer";
#endif
        if (backend == "glsl" && executable.empty() && std::filesystem::is_regular_file(bundled))
            executable = pathUtf8(bundled);
        if (!executable.empty())
            launch.argv = {executable};
    }
    if (backend == "typescript" || backend == "html" || backend == "css") {
        const auto node = toolPath("node");
        const auto relative = backend == "typescript" ? "typescript-language-server/lib/cli.mjs"
                              : backend == "html"
                                  ? "vscode-langservers-extracted/bin/vscode-html-language-server"
                                  : "vscode-langservers-extracted/bin/vscode-css-language-server";
        const auto script = root / "web-languages/node_modules" / relative;
        if (!node.empty() && std::filesystem::is_regular_file(script))
            launch.argv = {node, pathUtf8(script), "--stdio"};
        launch.settings = {{"provideFormatter", true},
                           {"hostInfo", "CyCode"},
                           {"disableAutomaticTypingAcquisition", true},
                           {"typescript", {{"format", {{"enable", true}}}}},
                           {"javascript", {{"format", {{"enable", true}}}}},
                           {"html", {{"format", {{"enable", true}}}}},
                           {"css", {{"validate", true}}},
                           {"embeddedLanguages", {{"css", true}, {"javascript", true}}}};
    } else if (backend == "rust") {
        auto executable = toolPath("rust_analyzer");
#ifdef _WIN32
        const auto bundled = root / "rust/bin/rust-analyzer.exe";
#else
        const auto bundled = root / "rust/bin/rust-analyzer";
#endif
        if (executable.empty() && std::filesystem::is_regular_file(bundled))
            executable = pathUtf8(bundled);
        if (executable.empty())
            executable = toolPath("rust-analyzer");
        if (!executable.empty())
            launch.argv = {executable};
        Json options = {{"cargo", {{"buildScripts", {{"enable", false}}}}},
                        {"procMacro", {{"enable", false}}},
                        {"checkOnSave", false}};
        if (toolPath("cargo").empty() && std::filesystem::is_directory(root / "rust/bin")) {
#ifdef _WIN32
            std::wstring value(32768, 0);
            const auto length = GetEnvironmentVariableW(L"PATH", value.data(), DWORD(value.size()));
            value.resize(length < value.size() ? length : 0);
            const auto path = pathUtf8(std::filesystem::path(value));
            const auto suffix = ".exe";
            const auto separator = ";";
#else
            const auto raw = std::getenv("PATH");
            const auto path = raw ? std::string(raw) : std::string();
            const auto suffix = "";
            const auto separator = ":";
#endif
            launch.environment = {{"PATH", pathUtf8(root / "rust/bin") + separator + path},
                                  {"CARGO", pathUtf8(root / "rust/bin" / (std::string("cargo") + suffix))},
                                  {"RUSTC", pathUtf8(root / "rust/bin" / (std::string("rustc") + suffix))},
                                  {"CARGO_HOME", pathUtf8(root / "rust/cargo-home")}};
            options["cargo"]["sysroot"] = pathUtf8(root / "rust");
        }
        launch.settings = {{"rust-analyzer", options},
                           {"$initializationOptions", options},
                           {"$workspaceReadyNotification", "experimental/serverStatus"}};
    } else if (backend == "java") {
        const auto java = toolPath("java");
        auto directory = root / "jdtls";
        const auto plugins = directory / "plugins";
        std::filesystem::path jar;
        std::error_code ec;
        if (std::filesystem::is_directory(plugins, ec))
            for (const auto &entry : std::filesystem::directory_iterator(plugins, ec))
                if (entry.path().filename().string().starts_with("org.eclipse.equinox.launcher_") &&
                    entry.path().extension() == ".jar")
                    jar = entry.path();
#ifdef _WIN32
        const auto config = directory / "config_win";
#elif defined(__APPLE__)
        const auto config = directory / "config_mac";
#else
        const auto config = directory / "config_linux";
#endif
        if (!java.empty() && !jar.empty()) {
            const auto data = workspace / ".cycode/java-lsp" /
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            launch.argv = {java,
                           "-Declipse.application=org.eclipse.jdt.ls.core.id1",
                           "-Dosgi.bundles.defaultStartLevel=4",
                           "-Declipse.product=org.eclipse.jdt.ls.core.product",
                           "-Dlog.level=ERROR",
                           "-Xms64m",
                           "-Xmx512m",
                           "--add-modules=ALL-SYSTEM",
                           "--add-opens",
                           "java.base/java.util=ALL-UNNAMED",
                           "--add-opens",
                           "java.base/java.lang=ALL-UNNAMED",
                           "-jar",
                           pathUtf8(jar),
                           "-configuration",
                           pathUtf8(data / "config"),
                           "-data",
                           pathUtf8(data / "workspace")};
        }
        const Json options = {
            {"autobuild", {{"enabled", false}}},
            {"import", {{"gradle", {{"enabled", false}}}, {"maven", {{"enabled", false}}}}}};
        launch.settings = {{"java", options},
                           {"$initializationOptions", {{"settings", {{"java", options}}}}},
                           {"$workspaceReadyNotification", "language/status"}};
        const auto debugBundle = toolPath("java_debug");
        if (!debugBundle.empty() && utf8Path(debugBundle).extension() == ".jar")
            launch.settings["$initializationOptions"]["bundles"] = Json::array({debugBundle});
    }
    return launch;
}
void validateLanguageServer(const Json &d) {
    if (!d.is_object() || d.dump().size() > 128 * 1024)
        throw std::runtime_error("Language server descriptor exceeds 128 KiB");
    const auto id = d.at("id").get<std::string>();
    if (id.empty() || id.size() > 160 || !std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '.' || c == '-' || c == '_';
        }))
        throw std::runtime_error("Invalid language server ID");
    if (d.value("title", std::string{}).empty() || d.at("title").get<std::string>().size() > 256)
        throw std::runtime_error("Language server needs a title");
    const auto &argv = d.at("arguments");
    if (!argv.is_array() || argv.empty() || argv.size() > 128)
        throw std::runtime_error("Language server needs 1..128 argv arguments");
    for (const auto &a : argv)
        if (!a.is_string() || a.get_ref<const std::string &>().size() > 8192 ||
            a.get_ref<const std::string &>().find('\0') != std::string::npos)
            throw std::runtime_error("Invalid language server argument");
    if (argv.front().get_ref<const std::string &>().empty())
        throw std::runtime_error("Language server executable is empty");
    const auto settings = d.value("settings", Json::object());
    if (!settings.is_object() || settings.dump().size() > 65536)
        throw std::runtime_error("Invalid language server settings");
    if (settings.contains("$workspaceReadyNotification") &&
        settings.at("$workspaceReadyNotification") != "" &&
        settings.at("$workspaceReadyNotification") != "language/status" &&
        settings.at("$workspaceReadyNotification") != "experimental/serverStatus")
        throw std::runtime_error("Unsupported readiness notification");
    const auto &languages = d.at("languages");
    if (!languages.is_array() || languages.empty() || languages.size() > 32)
        throw std::runtime_error("Language server needs 1..32 language selectors");
    std::set<std::string> selectors;
    for (const auto &l : languages) {
        const auto name = l.at("id").get<std::string>();
        if (name.empty() || name.size() > 128 || !std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return std::isalnum(c) || c == '-' || c == '_' || c == '.';
            }))
            throw std::runtime_error("Invalid LSP language ID");
        size_t count = 0;
        for (const auto *key : {"extensions", "filenames"}) {
            const auto entries = l.value(key, Json::array());
            if (!entries.is_array() || entries.size() > 128)
                throw std::runtime_error("Invalid language selectors");
            for (const auto &v : entries) {
                auto text = v.get<std::string>();
                if (text.empty() || text.size() > 128 ||
                    text.find_first_of("/\\:*?<>|\"\r\n") != std::string::npos ||
                    text.find('\0') != std::string::npos)
                    throw std::runtime_error("Invalid language selector");
                std::transform(text.begin(), text.end(), text.begin(),
                               [](unsigned char c) { return char(std::tolower(c)); });
                if (std::string(key) == "extensions" && (text.size() < 2 || text.front() != '.'))
                    throw std::runtime_error("Language extensions must begin with a dot");
                if (!languageBackend(std::string(key) == "extensions" ? "file" + text : text).empty())
                    throw std::runtime_error("Plugin language selector overlaps a built-in backend");
                if (!selectors.insert(std::string(key) + ":" + text).second)
                    throw std::runtime_error("Duplicate language selector");
                ++count;
            }
        }
        if (!count)
            throw std::runtime_error("Each language needs a file selector");
    }
}
std::string contributedLanguageId(const Json &d, const std::string &path) {
    auto name = pathUtf8(utf8Path(path).filename()), ext = extension(path);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    for (const auto &l : d.at("languages"))
        for (const auto *key : {"filenames", "extensions"})
            for (const auto &v : l.value(key, Json::array())) {
                auto match = v.get<std::string>();
                std::transform(match.begin(), match.end(), match.begin(),
                               [](unsigned char c) { return char(std::tolower(c)); });
                if (match == (std::string(key) == "extensions" ? ext : name))
                    return l.at("id");
            }
    return {};
}
} // namespace cycode
