#include "cycode/Languages.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
std::string extension(const std::string &path) {
    const auto dot=path.find_last_of('.'); auto ext=dot==std::string::npos ? "" : path.substr(dot);
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c) { return char(std::tolower(c)); }); return ext;
}
bool oneOf(const std::string &value,std::initializer_list<const char *> list) { for (auto item : list) if (value==item) return true; return false; }
}
const std::vector<std::string> &languageBackends() {
    static const std::vector<std::string> keys={"cpp","csharp","rust","java","typescript","html","css"}; return keys;
}
std::string languageBackend(const std::string &path) {
    const auto ext=extension(path);
    if (oneOf(ext,{".c",".cpp",".cc",".cxx",".h",".hpp",".hh",".hxx",".inl"})) return "cpp";
    if (ext==".cs") return "csharp";
    if (ext==".rs") return "rust";
    if (ext==".java") return "java";
    if (oneOf(ext,{".js",".jsx",".mjs",".cjs",".ts",".tsx",".mts",".cts"})) return "typescript";
    if (ext==".html" || ext==".htm") return "html";
    if (ext==".css") return "css";
    return "";
}
std::string languageId(const std::string &path) {
    const auto ext=extension(path); auto backend=languageBackend(path);
    if (backend=="cpp" && ext==".c") return "c";
    if (backend=="typescript") {
        if (ext==".tsx") return "typescriptreact";
        if (ext==".jsx") return "javascriptreact";
        return oneOf(ext,{".js",".mjs",".cjs"}) ? "javascript" : "typescript";
    }
    return backend;
}
std::string languageServerName(const std::string &backend) {
    if (backend=="cpp") return "C++ / clangd";
    if (backend=="csharp") return "C# / Roslyn";
    if (backend=="rust") return "Rust / rust-analyzer";
    if (backend=="java") return "Java / Eclipse JDT LS";
    if (backend=="typescript") return "JavaScript / TypeScript";
    if (backend=="html") return "HTML";
    if (backend=="css") return "CSS";
    return backend;
}
ServerLaunch defaultServerLaunch(const std::filesystem::path &workspace,const std::string &backend) {
    ServerLaunch launch;
    const auto root=installationRoot()/"runtime/tools";
    if (backend=="typescript" || backend=="html" || backend=="css") {
        const auto node=toolPath("node");
        const auto relative=backend=="typescript" ? "typescript-language-server/lib/cli.mjs" : backend=="html" ?
            "vscode-langservers-extracted/bin/vscode-html-language-server" : "vscode-langservers-extracted/bin/vscode-css-language-server";
        const auto script=root/"web-languages/node_modules"/relative;
        if (!node.empty() && std::filesystem::is_regular_file(script)) launch.argv={node,pathUtf8(script),"--stdio"};
        launch.settings={{"provideFormatter",true},{"hostInfo","CyCode"},{"disableAutomaticTypingAcquisition",true},
            {"typescript",{{"format",{{"enable",true}}}}},{"javascript",{{"format",{{"enable",true}}}}},
            {"html",{{"format",{{"enable",true}}}}},{"css",{{"validate",true}}},
            {"embeddedLanguages",{{"css",true},{"javascript",true}}}};
    } else if (backend=="rust") {
        auto executable=toolPath("rust_analyzer");
#ifdef _WIN32
        const auto bundled=root/"rust/bin/rust-analyzer.exe";
#else
        const auto bundled=root/"rust/bin/rust-analyzer";
#endif
        if (executable.empty() && std::filesystem::is_regular_file(bundled)) executable=pathUtf8(bundled);
        if (executable.empty()) executable=toolPath("rust-analyzer");
        if (!executable.empty()) launch.argv={executable};
        Json options={{"cargo",{{"buildScripts",{{"enable",false}}}}},{"procMacro",{{"enable",false}}},{"checkOnSave",false}};
        if (toolPath("cargo").empty() && std::filesystem::is_directory(root/"rust/bin")) {
#ifdef _WIN32
            std::wstring value(32768,0); const auto length=GetEnvironmentVariableW(L"PATH",value.data(),DWORD(value.size())); value.resize(length<value.size() ? length : 0);
            const auto path=pathUtf8(std::filesystem::path(value)); const auto suffix=".exe"; const auto separator=";";
#else
            const auto raw=std::getenv("PATH"); const auto path=raw ? std::string(raw) : std::string(); const auto suffix=""; const auto separator=":";
#endif
            launch.environment={{"PATH",pathUtf8(root/"rust/bin")+separator+path},{"CARGO",pathUtf8(root/"rust/bin"/(std::string("cargo")+suffix))},
                {"RUSTC",pathUtf8(root/"rust/bin"/(std::string("rustc")+suffix))},{"CARGO_HOME",pathUtf8(root/"rust/cargo-home")}};
            options["cargo"]["sysroot"]=pathUtf8(root/"rust");
        }
        launch.settings={{"rust-analyzer",options},{"$initializationOptions",options},{"$workspaceReadyNotification","experimental/serverStatus"}};
    } else if (backend=="java") {
        const auto java=toolPath("java"); auto directory=root/"jdtls";
        const auto plugins=directory/"plugins";
        std::filesystem::path jar;
        std::error_code ec;
        if (std::filesystem::is_directory(plugins,ec)) for (const auto &entry : std::filesystem::directory_iterator(plugins,ec))
            if (entry.path().filename().string().starts_with("org.eclipse.equinox.launcher_") && entry.path().extension()==".jar") jar=entry.path();
#ifdef _WIN32
        const auto config=directory/"config_win";
#elif defined(__APPLE__)
        const auto config=directory/"config_mac";
#else
        const auto config=directory/"config_linux";
#endif
        if (!java.empty() && !jar.empty()) {
            const auto data=workspace/".cycode/java-lsp"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            launch.argv={java,"-Declipse.application=org.eclipse.jdt.ls.core.id1","-Dosgi.bundles.defaultStartLevel=4",
                "-Declipse.product=org.eclipse.jdt.ls.core.product","-Dlog.level=ERROR","-Xms64m","-Xmx512m",
                "--add-modules=ALL-SYSTEM","--add-opens","java.base/java.util=ALL-UNNAMED","--add-opens","java.base/java.lang=ALL-UNNAMED",
                "-jar",pathUtf8(jar),"-configuration",pathUtf8(data/"config"),"-data",pathUtf8(data/"workspace")};
        }
        const Json options={{"autobuild",{{"enabled",false}}},{"import",{{"gradle",{{"enabled",false}}},{"maven",{{"enabled",false}}}}}};
        launch.settings={{"java",options},{"$initializationOptions",{{"settings",{{"java",options}}}}},{"$workspaceReadyNotification","language/status"}};
    }
    return launch;
}
} // namespace cycode
