#include "cycode/ProjectTemplates.h"
#include "cycode/FileTransactions.h"
#include "cycode/Paths.h"
#include "cycode/TemplateDefinitions.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
std::string token() {
    std::random_device random;
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i)
        out << std::setw(8) << uint32_t(random());
    return out.str();
}
std::string lower(std::string value) {
    for (auto &c : value)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return value;
}
bool same(const fs::path &a, const fs::path &b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
void ordinary(const fs::path &path) {
    if (!same(fs::absolute(path).lexically_normal(), fs::weakly_canonical(path)) ||
        fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Project creation cannot traverse links or redirected directories");
}
bool present(const fs::path &path) {
    return fs::symlink_status(path).type() != fs::file_type::not_found;
}
void nameCheck(const std::string &name) {
    auto letter = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    if (name.empty() || name.size() > 64 || !letter(name.front()) ||
        !std::all_of(name.begin(), name.end(), [&](unsigned char c) {
            return letter(c) || (c >= '0' && c <= '9') || c == '_' || c == '-';
        }))
        throw std::runtime_error("Project name must start with a letter and use at most 64 ASCII letters, "
                                 "digits, underscores or hyphens");
    const auto key = lower(name);
    if (key == "con" || key == "prn" || key == "aux" || key == "nul" ||
        (key.size() == 4 && (key.starts_with("com") || key.starts_with("lpt")) && key[3] >= '1' &&
         key[3] <= '9'))
        throw std::runtime_error("Project name is reserved on Windows");
}
// Roll back only the exact files issued by this plan. Keep unexpected user data,
// redirected paths and nonempty folders; never recursively delete a staging tree.
void cleanStage(const fs::path &stage, const std::string &identity, const Json &files) noexcept {
    try {
        ordinary(stage);
        if (directoryIdentity(stage) != identity)
            return;
        std::set<fs::path> folders;
        for (const auto &file : files) {
            const auto path = stage / utf8Path(file.at("path"));
            ordinary(path);
            if (fs::is_regular_file(path) && readTextFileBytes(path) == file.at("text").get<std::string>())
                fs::remove(path);
            for (auto parent = path.parent_path(); parent != stage && !parent.empty();
                 parent = parent.parent_path())
                folders.insert(parent);
        }
        for (auto it = folders.rbegin(); it != folders.rend(); ++it) {
            std::error_code error;
            fs::remove(*it, error);
        }
        std::error_code error;
        fs::remove(stage, error);
    } catch (...) { /* A changed staging tree belongs to its current owner. */
    }
}
} // namespace
ProjectTemplates::ProjectTemplates(fs::path root, Registry registry, Guard guard)
    : registry_(std::move(registry)), guard_(std::move(guard)), root_(fs::canonical(root)) {
    if (!fs::is_directory(root_))
        throw std::runtime_error("Project destination root must be an existing directory");
    rootIdentity_ = directoryIdentity(root_);
}
Json ProjectTemplates::catalogue() {
    return {{"templates", Json::array({{{"id", "cpp.console"},
                                        {"title", "C++ console (CMake)"},
                                        {"language", "cpp"},
                                        {"buildSystem", "cmake"},
                                        {"requirements", {"CMake 3.24+", "C++20 compiler"}}},
                                       {{"id", "c.console"},
                                        {"title", "C console (CMake)"},
                                        {"language", "c"},
                                        {"buildSystem", "cmake"},
                                        {"requirements", {"CMake 3.24+", "C17 compiler"}}},
                                       {{"id", "rust.console"},
                                        {"title", "Rust console (Cargo)"},
                                        {"language", "rust"},
                                        {"buildSystem", "cargo"},
                                        {"requirements", {"Cargo / Rust 2021 toolchain"}}},
                                       {{"id", "csharp.console"},
                                        {"title", "C# console (.NET solution)"},
                                        {"language", "csharp"},
                                        {"buildSystem", "msbuild"},
                                        {"requirements", {".NET SDK and matching target framework"}}},
                                       {{"id", "java.console"},
                                        {"title", "Java console (standalone)"},
                                        {"language", "java"},
                                        {"buildSystem", "java"},
                                        {"requirements", {"JDK (javac and java)"}}},
                                       {{"id", "web.javascript"},
                                        {"title", "Web (HTML / CSS / JavaScript)"},
                                        {"language", "javascript"},
                                        {"buildSystem", "npm"},
                                        {"requirements", {"Node.js 18+ and npm"}}}})},
            {"frameworks", {"net8.0", "net9.0", "net10.0"}}};
}
Json ProjectTemplates::list() const {
    auto result = catalogue();
    // Snapshot revision and descriptors must be read under the same host lock.
    const auto snapshot = registry_ ? registry_() : Json({{"revision", 0}, {"templates", Json::array()}});
    result["revision"] = snapshot.at("revision");
    for (auto &item : result["templates"]) {
        item["plugin"] = "";
        item["fields"] = Json::array();
        item["nameStyle"] = "portable";
    }
    for (const auto &entry : snapshot.at("templates")) {
        auto definition = entry;
        definition.erase("plugin");
        validateProjectTemplate(definition);
        Json info;
        for (const auto *key : {"id", "title", "language", "buildSystem", "requirements"})
            info[key] = entry.at(key);
        info["plugin"] = entry.at("plugin");
        info["fields"] = entry.value("fields", Json::array());
        info["nameStyle"] = entry.value("nameStyle", std::string("portable"));
        result["templates"].push_back(std::move(info));
    }
    return result;
}
fs::path ProjectTemplates::destination(const std::string &relative) const {
    ordinary(root_);
    if (directoryIdentity(root_) != rootIdentity_)
        throw std::runtime_error("Project destination root changed; preview again");
    if (relative.empty() || relative.size() > 2048 || relative.find('\0') != std::string::npos ||
        relative.find_first_of("\r\n\\:") != std::string::npos)
        throw std::runtime_error("Use a relative project destination with forward slashes");
    const auto path = utf8Path(relative);
    if (path.has_root_path())
        throw std::runtime_error("Project destination must stay inside the granted root");
    for (const auto &part : path) {
        const auto key = lower(pathUtf8(part));
        if (key == ".." || key == "." || key.empty() || key == ".git" || key == ".cycode")
            throw std::runtime_error("Invalid or protected project destination");
    }
    if (same(root_, installationRoot())) {
        const auto top = lower(pathUtf8(*path.begin()));
        if (top == "runtime" || top == "data" || top == "app")
            throw std::runtime_error("Project destination is application-owned data");
    }
    const auto result = root_ / path;
    nameCheck(pathUtf8(result.filename()));
    ordinary(result);
    if (!fs::is_directory(result.parent_path()))
        throw std::runtime_error("Project parent directory must already exist");
    if (present(result))
        throw std::runtime_error("Project destination already exists; choose a new directory");
    return result;
}
Json ProjectTemplates::preview(const Json &params) {
    const auto snapshot = registry_ ? registry_() : Json({{"revision", 0}, {"templates", Json::array()}});
    std::lock_guard lock(mutex_);
    if (!params.is_object())
        throw std::runtime_error("Expected project template parameters");
    for (const auto &[key, value] : params.items())
        if (key != "template" && key != "name" && key != "destination" && key != "framework" &&
            key != "options")
            throw std::runtime_error("Unknown project template parameter: " + key);
    const std::string kind = params.at("template"), name = params.at("name");
    nameCheck(name);
    const auto catalogueModel = catalogue();
    Json info;
    for (const auto &item : catalogueModel.at("templates"))
        if (item.at("id") == kind)
            info = item;
    Json definition;
    for (const auto &item : snapshot.at("templates"))
        if (item.at("id") == kind) {
            definition = item;
            info = item;
        }
    if (info.is_null())
        throw std::runtime_error("Unknown project template");
    const auto relative = params.value("destination", name);
    (void)destination(relative);
    const std::string framework = params.value("framework", kind == "csharp.console" ? "net8.0" : "");
    if (kind == "csharp.console") {
        const auto &values = catalogueModel.at("frameworks");
        if (std::find(values.begin(), values.end(), framework) == values.end())
            throw std::runtime_error("Unsupported .NET target framework");
    } else if (!framework.empty())
        throw std::runtime_error("Target framework applies only to .NET projects");
    Json files = Json::array();
    auto add = [&](const std::string &path, const std::string &text) {
        files.push_back({{"path", path}, {"text", text}, {"bytes", text.size()}});
    };
    std::string openFile, entry = relative, instructions;
    Json selectedOptions = Json::object();
    if (!definition.is_null()) {
        definition.erase("plugin");
        validateProjectTemplate(definition);
        const auto expanded =
            expandProjectTemplate(definition, name, params.value("options", Json::object()));
        files = expanded.at("files");
        openFile = expanded.at("openFile");
        instructions = expanded.at("instructions");
        selectedOptions = expanded.at("options");
        if (expanded.at("entry") != ".")
            entry = relative + "/" + expanded.at("entry").get<std::string>();
    } else if (params.contains("options") &&
               (!params.at("options").is_object() || !params.at("options").empty()))
        throw std::runtime_error("This built-in template has no additional options");
    else if (kind == "cpp.console" || kind == "c.console") {
        const bool cpp = kind == "cpp.console";
        openFile = cpp ? "src/main.cpp" : "src/main.c";
        add("CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.24)\nproject(" + name + " VERSION 0.1.0 LANGUAGES " +
                (cpp ? "CXX" : "C") + ")\nset(CMAKE_EXPORT_COMPILE_COMMANDS ON)\nadd_executable(" + name +
                " " + openFile + ")\nset_target_properties(" + name + " PROPERTIES " + (cpp ? "CXX" : "C") +
                "_STANDARD " + (cpp ? "20" : "17") + " " + (cpp ? "CXX" : "C") + "_STANDARD_REQUIRED YES " +
                (cpp ? "CXX" : "C") +
                "_EXTENSIONS NO)\ninclude(CTest)\nif(BUILD_TESTING)\n  add_test(NAME smoke COMMAND " + name +
                " --self-test)\nendif()\n");
        add(openFile,
            cpp ? "#include <iostream>\n#include <string_view>\n\nint answer() { return 42; }\n\nint "
                  "main(int argc, char **argv) {\n    if (argc > 1 && std::string_view(argv[1]) == "
                  "\"--self-test\")\n        return answer() == 42 ? 0 : 1;\n    std::cout << \"Hello from " +
                      name + "!\\n\";\n    return 0;\n}\n"
                : "#include <stdio.h>\n#include <string.h>\n\nstatic int answer(void) { return 42; }\n\nint "
                  "main(int argc, char **argv) {\n    if (argc > 1 && strcmp(argv[1], \"--self-test\") == "
                  "0)\n        return answer() == 42 ? 0 : 1;\n    puts(\"Hello from " +
                      name + "!\");\n    return 0;\n}\n");
        instructions = "Configure and build in CyCode's CMake view. Run CTest in the generated build "
                       "directory.\nCLI: cmake -S . -B build\n     cmake --build build --config Debug\n     "
                       "ctest --test-dir build -C Debug --output-on-failure";
    } else if (kind == "rust.console") {
        openFile = "src/main.rs";
        add("Cargo.toml",
            "[package]\nname = \"" + lower(name) + "\"\nversion = \"0.1.0\"\nedition = \"2021\"\n");
        add(openFile, "fn answer() -> i32 { 42 }\n\nfn main() {\n    println!(\"Hello from " + name +
                          "! {}\", answer());\n}\n\n#[cfg(test)]\nmod tests {\n    #[test]\n    fn "
                          "answer_is_42() { assert_eq!(super::answer(), 42); }\n}\n");
        instructions = "Use Project tasks: Configure, Build, Test and Run.\nCLI: cargo build --offline\n     "
                       "cargo test --offline\n     cargo run --offline";
    } else if (kind == "csharp.console") {
        openFile = "Program.cs";
        entry = relative + "/" + name + ".sln";
        add(name + ".csproj", "<Project Sdk=\"Microsoft.NET.Sdk\">\n  <PropertyGroup>\n    "
                              "<OutputType>Exe</OutputType>\n    <TargetFramework>" +
                                  framework +
                                  "</TargetFramework>\n    <ImplicitUsings>enable</ImplicitUsings>\n    "
                                  "<Nullable>enable</Nullable>\n    <DebugType>portable</DebugType>\n  "
                                  "</PropertyGroup>\n</Project>\n");
        add(openFile, "static int Answer() => 42;\n\nif (args.Contains(\"--self-test\"))\n    return "
                      "Answer() == 42 ? 0 : 1;\n\nConsole.WriteLine(\"Hello from " +
                          name + "!\");\nreturn 0;\n");
        auto guid = token();
        guid = "{" + guid.substr(0, 8) + "-" + guid.substr(8, 4) + "-" + guid.substr(12, 4) + "-" +
               guid.substr(16, 4) + "-" + guid.substr(20) + "}";
        add(name + ".sln",
            "Microsoft Visual Studio Solution File, Format Version 12.00\n# Visual Studio Version "
            "17\nProject(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"" +
                name + "\", \"" + name + ".csproj\", \"" + guid +
                "\"\nEndProject\nGlobal\n\tGlobalSection(SolutionConfigurationPlatforms) = "
                "preSolution\n\t\tDebug|Any CPU = Debug|Any CPU\n\t\tRelease|Any CPU = Release|Any "
                "CPU\n\tEndGlobalSection\n\tGlobalSection(ProjectConfigurationPlatforms) = "
                "postSolution\n\t\t" +
                guid + ".Debug|Any CPU.ActiveCfg = Debug|Any CPU\n\t\t" + guid +
                ".Debug|Any CPU.Build.0 = Debug|Any CPU\n\t\t" + guid +
                ".Release|Any CPU.ActiveCfg = Release|Any CPU\n\t\t" + guid +
                ".Release|Any CPU.Build.0 = Release|Any CPU\n\tEndGlobalSection\nEndGlobal\n");
        instructions =
            "Open the .sln in CyCode and use MSBuild: Restore, Build and Run. Install the .NET SDK and " +
            framework +
            " targeting/runtime packs first.\nCLI: dotnet build\n     dotnet run -- --self-test\nNo external "
            "NuGet package or test framework is required.";
    } else if (kind == "java.console") {
        openFile = "src/main/java/Main.java";
        add(openFile,
            "public class Main {\n    static int answer() { return 42; }\n\n    public static void "
            "main(String[] args) {\n        if (args.length > 0 && args[0].equals(\"--self-test\")) {\n      "
            "      if (answer() != 42) throw new AssertionError(\"Expected 42\");\n            return;\n     "
            "   }\n        System.out.println(\"Hello from " +
                name + "!\");\n    }\n}\n");
        instructions =
            "Use Project tasks: Build and Run. The standalone Java adapter detects src/main/java.\nCLI: "
            "javac -d build src/main/java/Main.java\n     java -cp build Main --self-test\nThis template "
            "does not require Maven, Gradle or a downloaded test framework.";
    } else {
        openFile = "index.html";
        add("package.json",
            Json({{"name", lower(name)},
                  {"version", "0.1.0"},
                  {"private", true},
                  {"type", "module"},
                  {"scripts",
                   {{"build",
                     "node --check src/main.js && node --check src/app.js && node --check server.cjs"},
                    {"test", "node --test tests/app.test.js"},
                    {"start", "node server.cjs"}}}})
                    .dump(2) +
                "\n");
        add(openFile,
            "<!doctype html>\n<html lang=\"en\">\n<head>\n  <meta charset=\"utf-8\">\n  <meta "
            "name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n  <title>" +
                name +
                "</title>\n  <link rel=\"stylesheet\" href=\"/styles.css\">\n</head>\n<body>\n  <main><h1>" +
                name +
                "</h1><p id=\"message\"></p></main>\n  <script type=\"module\" "
                "src=\"/src/main.js\"></script>\n</body>\n</html>\n");
        add("styles.css", "body { font-family: system-ui, sans-serif; max-width: 60rem; margin: 4rem auto; "
                          "padding: 1rem; color: #222; background: #f7f7f7; }\n");
        add("src/app.js", "export function greeting(name) { return `Hello from ${name}!`; }\n");
        add("src/main.js", "import { greeting } from "
                           "'./app.js';\ndocument.querySelector('#message').textContent = greeting('" +
                               name + "');\n");
        add("tests/app.test.js",
            "import test from 'node:test';\nimport assert from 'node:assert/strict';\nimport { greeting } "
            "from '../src/app.js';\ntest('greeting', () => assert.equal(greeting('CyCode'), 'Hello from "
            "CyCode!'));\n");
        add("server.cjs",
            "const http = require('node:http');\nconst fs = require('node:fs');\nconst path = "
            "require('node:path');\nconst routes = { '/': ['index.html', 'text/html'], '/styles.css': "
            "['styles.css', 'text/css'], '/src/main.js': ['src/main.js', 'text/javascript'], '/src/app.js': "
            "['src/app.js', 'text/javascript'] };\nconst port = Number(process.argv[2] || 3000);\nif "
            "(!Number.isInteger(port) || port < 1 || port > 65535) throw new Error('Invalid "
            "port');\nhttp.createServer((req, res) => {\n  const key = (req.url || '').split('?')[0];\n  "
            "const route = Object.hasOwn(routes, key) ? routes[key] : undefined;\n  if (!route || !['GET', "
            "'HEAD'].includes(req.method)) { "
            "res.writeHead(404); res.end(); return; }\n  fs.readFile(path.join(__dirname, route[0]), (error, "
            "bytes) => {\n    if (error) { res.writeHead(500); res.end(); return; }\n    res.writeHead(200, "
            "{ 'Content-Type': route[1] + '; charset=utf-8', 'X-Content-Type-Options': 'nosniff' });\n    "
            "res.end(req.method === 'HEAD' ? undefined : bytes);\n  });\n}).listen(port, '127.0.0.1', () => "
            "console.log(`Open http://127.0.0.1:${port}`));\n");
        instructions = "Use Project tasks: Build (JavaScript syntax checks), Test and Run (local HTTP "
                       "server). Open its displayed URL in a browser.\nCLI: npm run build\n     npm test\n   "
                       "  npm start\nNo npm install is required: this template has no external dependencies.";
    }
    if (definition.is_null()) {
        add(".gitignore", ".cycode/\n.cache/\nbuild/\nbin/\nobj/\ntarget/\nnode_modules/\n");
        add("README.md", "# " + name + "\n\n" + info.at("title").get<std::string>() + "\n\n" + instructions +
                             "\n\nProject creation does not install tools, fetch packages, initialize Git or "
                             "select a project license.\n");
    }
    const auto id = token();
    Json model = {{"id", id},
                  {"template", kind},
                  {"name", name},
                  {"destination", relative},
                  {"entry", entry},
                  {"openFile", openFile},
                  {"framework", framework},
                  {"plugin", info.value("plugin", std::string{})},
                  {"catalogueRevision", definition.is_null() ? Json(0) : snapshot.at("revision")},
                  {"options", selectedOptions},
                  {"files", files},
                  {"requirements", info.at("requirements")},
                  {"instructions", instructions},
                  {"state", "preview"}};
    while (plans_.size() >= 32)
        plans_.erase(std::min_element(plans_.begin(), plans_.end(), [](const auto &a, const auto &b) {
            return a.second.created < b.second.created;
        }));
    plans_.emplace(id, Plan{model, directoryIdentity(destination(relative).parent_path())});
    return model;
}
Json ProjectTemplates::create(const std::string &id) {
    std::string owner;
    uint64_t revision = 0;
    {
        std::lock_guard lock(mutex_);
        const auto found = plans_.find(id);
        if (found == plans_.end())
            throw std::runtime_error("Unknown project preview; preview again");
        owner = found->second.model.value("plugin", std::string{});
        revision = found->second.model.value("catalogueRevision", uint64_t(0));
    }
    const auto commit = [this, id] { return createLocked(id); };
    if (!owner.empty()) {
        if (!guard_)
            throw std::runtime_error("Plugin template activation guard is unavailable");
        return guard_(owner, revision, commit);
    }
    return commit();
}
Json ProjectTemplates::createLocked(const std::string &id) {
    std::lock_guard lock(mutex_);
    const auto found = plans_.find(id);
    if (found == plans_.end())
        throw std::runtime_error("Unknown project preview; preview again");
    const auto &plan = found->second;
    if (std::chrono::steady_clock::now() - plan.created > std::chrono::minutes(10)) {
        plans_.erase(found);
        throw std::runtime_error("Project preview expired; preview again");
    }
    if (plan.model.at("state") != "preview")
        throw std::runtime_error("Project preview was already created");
    const auto path = destination(plan.model.at("destination"));
    if (directoryIdentity(path.parent_path()) != plan.parentIdentity)
        throw std::runtime_error("Project parent directory changed; preview again");
    const auto stage = path.parent_path() / (".cycode-new-" + token());
    if (!fs::create_directory(stage))
        throw std::runtime_error("Cannot allocate a new project staging directory");
    const auto stageIdentity = directoryIdentity(stage);
    try {
        std::set<fs::path> expected;
        for (const auto &file : plan.model.at("files")) {
            const auto target = stage / utf8Path(file.at("path"));
            for (auto part = utf8Path(file.at("path")); !part.empty(); part = part.parent_path())
                expected.insert(part);
            ordinary(target);
            fs::create_directories(target.parent_path());
            ordinary(target);
            writeTextFileAtomic(target, file.at("text").get<std::string>(), std::nullopt);
        }
        // Validate every staged byte, then publish through the same exclusive
        // filesystem primitive as resource operations. Never overwrite a destination.
        size_t count = 0;
        for (const auto &item : fs::recursive_directory_iterator(stage)) {
            ordinary(item.path());
            if (++count > 128 || !expected.contains(item.path().lexically_relative(stage)))
                throw std::runtime_error("Project staging content changed");
        }
        if (count != expected.size() || directoryIdentity(stage) != stageIdentity)
            throw std::runtime_error("Project staging content changed");
        for (const auto &file : plan.model.at("files")) {
            const auto target = stage / utf8Path(file.at("path"));
            ordinary(target);
            if (!fs::is_regular_file(target) ||
                readTextFileBytes(target) != file.at("text").get<std::string>())
                throw std::runtime_error("Project staging content changed");
        }
        (void)destination(plan.model.at("destination"));
        if (directoryIdentity(path.parent_path()) != plan.parentIdentity)
            throw std::runtime_error("Project parent directory changed; preview again");
        moveExclusive(stage, path);
    } catch (...) {
        cleanStage(stage, stageIdentity, plan.model.at("files"));
        throw;
    }
    found->second.model["state"] = "created";
    return found->second.model;
}
} // namespace cycode
