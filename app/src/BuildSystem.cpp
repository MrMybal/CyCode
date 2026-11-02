#include "cycode/BuildSystem.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <tuple>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace cycode {
using Json = nlohmann::json;
namespace fs = std::filesystem;
static std::string environment(const char *name) {
#ifdef _WIN32
    std::wstring key(name, name + std::char_traits<char>::length(name));
    auto length = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    std::wstring value(length, 0);
    if (!length)
        return {};
    GetEnvironmentVariableW(key.c_str(), value.data(), length);
    value.resize(length - 1);
    return pathUtf8(fs::path(value));
#else
    auto value = std::getenv(name);
    return value ? value : "";
#endif
}
static fs::path executable(const std::string &name, const std::vector<fs::path> &hints = {}) {
    std::vector<fs::path> candidates;
    std::istringstream stream(environment("PATH"));
    std::string directory;
#ifdef _WIN32
    constexpr char separator = ';';
    auto filename = name.ends_with(".exe") ? name : name + ".exe";
#else
    constexpr char separator = ':';
    auto filename = name;
#endif
    while (std::getline(stream, directory, separator)) {
        auto path = utf8Path(directory);
        if (path.is_absolute())
            candidates.push_back(path / filename);
    }
    candidates.insert(candidates.end(), hints.begin(), hints.end());
    for (const auto &candidate : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
#ifndef _WIN32
            if (access(candidate.c_str(), X_OK))
                continue;
#endif
            return fs::canonical(candidate);
        }
    }
    return {};
}
static std::string read(const fs::path &path, size_t limit = 32 * 1024 * 1024) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read build metadata: " + pathUtf8(path));
    in.seekg(0, std::ios::end);
    auto length = in.tellg();
    if (length < 0 || length > static_cast<std::streamoff>(limit))
        throw std::runtime_error("Build metadata exceeds size limit");
    in.seekg(0);
    std::string result(static_cast<size_t>(length), '\0');
    in.read(result.data(), length);
    if (!in && length)
        throw std::runtime_error("Build metadata changed during read");
    return result;
}
static Json cache(const fs::path &build, const BuildSystem::Resolver &resolve) {
    Json result = Json::object();
    auto file = resolve(pathUtf8(build / "CMakeCache.txt"));
    if (!fs::exists(file))
        return result;
    std::istringstream stream(read(file, 4 * 1024 * 1024));
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.starts_with("//") || line.starts_with('#'))
            continue;
        auto colon = line.find(':'), equals = line.find('=');
        if (colon != std::string::npos && equals != std::string::npos && colon < equals)
            result[line.substr(0, colon)] = line.substr(equals + 1);
    }
    return result;
}
static bool samePath(const fs::path &a, const fs::path &b) {
#ifdef _WIN32
    return _wcsicmp(fs::weakly_canonical(a).c_str(), fs::weakly_canonical(b).c_str()) == 0;
#else
    return fs::weakly_canonical(a) == fs::weakly_canonical(b);
#endif
}
static bool within(const fs::path &base, const fs::path &path) {
    auto a = fs::weakly_canonical(base), b = fs::weakly_canonical(path);
    auto i = a.begin(), j = b.begin();
    for (; i != a.end(); ++i, ++j) {
        if (j == b.end())
            return false;
#ifdef _WIN32
        if (_wcsicmp(i->c_str(), j->c_str()))
            return false;
#else
        if (*i != *j)
            return false;
#endif
    }
    return true;
}
static void writeAtomic(const fs::path &path, const std::string &text) {
    if (fs::exists(path) && read(path) == text)
        return;
    fs::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    {
        std::ofstream out(temporary, std::ios::binary);
        out << text;
        out.flush();
        if (!out) {
            fs::remove(temporary);
            throw std::runtime_error("Cannot write build metadata");
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(temporary);
        throw std::runtime_error("Cannot replace build metadata");
    }
#else
    fs::rename(temporary, path);
#endif
}
static void ignoreGeneratedTree(const fs::path &path) {
    constexpr char text[] = "# CyCode: generated CMake build tree\n*\n";
#ifdef _WIN32
    auto file =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(file, text, sizeof(text) - 1, &written, nullptr);
    CloseHandle(file);
#else
    auto file = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666);
    if (file < 0)
        return;
    (void)::write(file, text, sizeof(text) - 1);
    ::close(file);
#endif
}
// This quoting produces compilation-database data; it is never used to spawn a shell.
static std::string argument(const std::string &value) {
#ifdef _WIN32
    std::string result = "\"";
    size_t slashes = 0;
    for (char c : value) {
        if (c == '\\') {
            ++slashes;
            continue;
        }
        result.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
        slashes = 0;
        result += c;
    }
    result.append(slashes * 2, '\\');
    return result + '"';
#else
    std::string result = "'";
    for (char c : value)
        result += c == '\'' ? "'\\''" : std::string(1, c);
    return result + '\'';
#endif
}
BuildSystem::BuildSystem(fs::path root, Resolver resolve)
    : root_(std::move(root)), resolve_(std::move(resolve)) {}
Json BuildSystem::detect(bool refresh) {
    std::lock_guard lock(discoveryMutex_);
    if (!refresh && detected_.is_object())
        return detected_;
    Json compilers = Json::array(), generators = Json::array(), warnings = Json::array();
    std::vector<fs::path> cmakeHints, ninjaHints;
    Json installations = Json::array();
#ifdef _WIN32
    auto programFiles = utf8Path(environment("ProgramFiles"));
    auto programFilesX86 = utf8Path(environment("ProgramFiles(x86)"));
    if (programFiles.empty())
        programFiles = "C:/Program Files";
    if (programFilesX86.empty())
        programFilesX86 = "C:/Program Files (x86)";
    auto vswhere = programFilesX86 / "Microsoft Visual Studio/Installer/vswhere.exe";
    if (fs::exists(vswhere)) {
        auto result =
            runProcess({pathUtf8(vswhere), "-products", "*", "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-format", "json", "-utf8"},
                       root_, std::chrono::seconds(5), 1024 * 1024);
        if (!result.exitCode) {
            auto parsed = Json::parse(result.output, nullptr, false);
            if (parsed.is_array())
                installations = std::move(parsed);
        }
    }
    for (const auto &installation : installations) {
        auto path = utf8Path(installation.value("installationPath", ""));
        cmakeHints.push_back(path / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe");
        ninjaHints.push_back(path / "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe");
        auto versionFile = path / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt";
        if (!fs::exists(versionFile))
            continue;
        auto version = read(versionFile, 1024);
        while (!version.empty() && std::isspace(static_cast<unsigned char>(version.back())))
            version.pop_back();
        auto cl = path / "VC/Tools/MSVC" / utf8Path(version) / "bin/Hostx64/x64/cl.exe";
        if (!fs::exists(cl))
            continue;
        compilers.push_back({{"id", "msvc:" + pathUtf8(cl)},
                             {"name", installation.value("displayName", "MSVC") + " / " + version},
                             {"family", "MSVC"},
                             {"cCompiler", pathUtf8(cl)},
                             {"cxxCompiler", pathUtf8(cl)},
                             {"version", version},
                             {"installation", pathUtf8(path)},
                             {"installationVersion", installation.value("installationVersion", "")}});
    }
#endif
    auto cmake = executable("cmake", cmakeHints), ninja = executable("ninja", ninjaHints),
         make = executable("make");
    for (const auto &[family, cName, cxxName] :
         std::vector<std::tuple<std::string, std::string, std::string>>{{"Clang", "clang", "clang++"},
                                                                        {"GCC", "gcc", "g++"}}) {
        std::vector<fs::path> cHints, cxxHints;
#ifdef _WIN32
        if (family == "Clang") {
            cHints.push_back(programFiles / "LLVM/bin/clang.exe");
            cxxHints.push_back(programFiles / "LLVM/bin/clang++.exe");
        }
#endif
        auto c = executable(cName, cHints), cxx = executable(cxxName, cxxHints);
        if (c.empty() || cxx.empty())
            continue;
        auto result = runProcess({pathUtf8(cxx), "--version"}, root_, std::chrono::seconds(3), 4096);
        if (result.exitCode)
            continue;
        auto version = result.output.substr(0, result.output.find_first_of("\r\n"));
        compilers.push_back({{"id", family + ":" + pathUtf8(cxx)},
                             {"name", family + " / " + version},
                             {"family", family},
                             {"cCompiler", pathUtf8(c)},
                             {"cxxCompiler", pathUtf8(cxx)},
                             {"version", version},
                             {"installation", ""},
                             {"installationVersion", ""}});
    }
    std::string cmakeVersion;
    if (!cmake.empty()) {
        auto result =
            runProcess({pathUtf8(cmake), "-E", "capabilities"}, root_, std::chrono::seconds(5), 1024 * 1024);
        auto data = Json::parse(result.output, nullptr, false);
        if (!result.exitCode && data.is_object()) {
            for (const auto &entry : data.value("generators", Json::array())) {
                const auto name = entry.value("name", "");
                bool usable =
                    (name == "Ninja" && !ninja.empty()) || (name == "Unix Makefiles" && !make.empty());
                for (const auto &installation : installations) {
                    const auto version = installation.value("installationVersion", "");
                    usable |= name.starts_with("Visual Studio " + version.substr(0, version.find('.')) + " ");
                }
                if (usable)
                    generators.push_back(entry);
            }
            cmakeVersion = data.value("version", Json::object()).value("string", "");
        } else
            warnings.push_back("CMake capabilities could not be read");
    } else
        warnings.push_back("CMake was not found");
    detected_ = {{"cmake", pathUtf8(cmake)}, {"cmakeVersion", cmakeVersion}, {"ninja", pathUtf8(ninja)},
                 {"make", pathUtf8(make)},   {"toolchains", compilers},      {"generators", generators},
                 {"warnings", warnings}};
    return detected_;
}
Json BuildSystem::profile(const Json &params) const {
    Json value = {{"source", fs::exists(root_ / "CMakeLists.txt")       ? "."
                             : fs::exists(root_ / "app/CMakeLists.txt") ? "app"
                                                                        : "."},
                  {"buildDirectory", "build"},
                  {"configuration", "Debug"},
                  {"toolchain", "automatic"},
                  {"generator", "automatic"},
                  {"target", ""}};
    auto file = resolve_(".cycode/build.json");
    if (fs::exists(file)) {
        auto saved = Json::parse(read(file, 65536));
        if (saved.value("schemaVersion", 0) != 1 || !saved.contains("profile"))
            throw std::runtime_error(
                "Unrecognized .cycode/build.json; preserve it and select a valid CyCode build profile");
        if (!saved.at("profile").is_object())
            throw std::runtime_error("Invalid CyCode build profile");
        for (const auto &[key, setting] : saved.at("profile").items()) {
            if (!value.contains(key))
                throw std::runtime_error("Unknown CyCode build profile field: " + key);
            value[key] = setting;
        }
    }
    for (auto &[key, setting] : value.items())
        if (params.contains(key))
            setting = params.at(key);
    for (const auto &[key, setting] : value.items())
        if (!setting.is_string())
            throw std::runtime_error("Build profile values must be strings");
    const std::string config = value.at("configuration");
    if (config != "Debug" && config != "Release" && config != "RelWithDebInfo" && config != "MinSizeRel")
        throw std::runtime_error("Unsupported build configuration");
    value["source"] = pathUtf8(resolve_(value.at("source")).lexically_relative(root_));
    value["buildDirectory"] = pathUtf8(resolve_(value.at("buildDirectory")).lexically_relative(root_));
    return value;
}
void BuildSystem::saveProfile(const Json &value) {
    writeAtomic(resolve_(".cycode/build.json"),
                Json{{"schemaVersion", 1}, {"profile", value}}.dump(2) + "\n");
}
Json BuildSystem::readReply(const fs::path &directory, const std::string &name) const {
    auto relative = utf8Path(name);
    if (relative.has_parent_path() || relative.is_absolute())
        throw std::runtime_error("Invalid CMake reply reference");
    auto path = resolve_(pathUtf8(directory / relative));
    if (!within(directory, path))
        throw std::runtime_error("CMake reply escapes its directory");
    return Json::parse(read(path));
}
Json BuildSystem::inspect(const Json &params) {
    std::lock_guard lock(operationMutex_);
    return inspectLocked(profile(params));
}
std::filesystem::path BuildSystem::activeDatabaseDirectory() const {
    auto value = profile(Json::object());
    auto build = resolve_(value.at("buildDirectory"));
    auto generated = resolve_(
        pathUtf8(build / ".cycode/clangd" / utf8Path(value.at("configuration")) / "compile_commands.json"));
    auto native = resolve_(pathUtf8(build / "compile_commands.json"));
    if (fs::exists(native))
        return native.parent_path();
    if (fs::exists(generated))
        return generated.parent_path();
    return {};
}
Json BuildSystem::inspectLocked(const Json &value, bool generateDatabase) {
    auto build = resolve_(value.at("buildDirectory")), source = resolve_(value.at("source"));
    auto stored = cache(build, resolve_);
    Json database = {{"path", ""}, {"origin", "none"}, {"entries", 0}, {"available", false}};
    Json result = {{"profile", value},
                   {"configured", !stored.empty()},
                   {"generator", stored.value("CMAKE_GENERATOR", "")},
                   {"configurations", Json::array()},
                   {"targets", Json::array()},
                   {"toolchains", Json::array()},
                   {"compilationDatabase", database},
                   {"message", ""}};
    auto reply = resolve_(pathUtf8(build / ".cmake/api/v1/reply"));
    std::string indexName;
    if (fs::is_directory(reply))
        for (const auto &entry : fs::directory_iterator(reply)) {
            auto name = entry.path().filename().string();
            if (name.starts_with("index-") && name.ends_with(".json") && name > indexName)
                indexName = name;
        }
    Json commands = Json::array();
    if (!indexName.empty()) {
        auto index = readReply(reply, indexName);
        auto references = index.value("reply", Json::object()).value("client-cycode", Json::object());
        Json model, toolchains;
        if (references.contains("codemodel-v2"))
            model = readReply(reply, references["codemodel-v2"].at("jsonFile"));
        if (references.contains("toolchains-v1"))
            toolchains = readReply(reply, references["toolchains-v1"].at("jsonFile"));
        auto compilers =
            toolchains.is_object() ? toolchains.value("toolchains", Json::array()) : Json::array();
        result["toolchains"] = compilers;
        if (model.is_object()) {
            if (!samePath(utf8Path(model.at("paths").at("source")), source) ||
                !samePath(utf8Path(model.at("paths").at("build")), build))
                throw std::runtime_error("CMake codemodel belongs to another source or build tree");
            for (const auto &config : model.at("configurations")) {
                result["configurations"].push_back(config.at("name"));
                if (config.value("name", "") != value.at("configuration").get<std::string>() &&
                    model.at("configurations").size() > 1)
                    continue;
                if (config.at("targets").size() > 5000)
                    throw std::runtime_error("CMake target count exceeds limit");
                std::set<std::string> seen;
                for (const auto &reference : config.at("targets")) {
                    auto target = readReply(reply, reference.at("jsonFile"));
                    result["targets"].push_back({{"name", target.at("name")},
                                                 {"type", target.at("type")},
                                                 {"artifacts", target.value("artifacts", Json::array())}});
                    for (const auto &file : target.value("sources", Json::array())) {
                        if (!file.contains("compileGroupIndex"))
                            continue;
                        const auto groupIndex = file.at("compileGroupIndex").get<size_t>();
                        if (!target.contains("compileGroups") || groupIndex >= target["compileGroups"].size())
                            continue;
                        const auto &group = target["compileGroups"][groupIndex];
                        auto lang = group.value("language", "");
                        if (lang != "C" && lang != "CXX")
                            continue;
                        std::string path;
                        try {
                            path = pathUtf8(resolve_(pathUtf8(source / utf8Path(file.at("path")))));
                        } catch (const std::exception &) {
                            continue;
                        }
                        if (seen.contains(path))
                            continue;
                        Json compiler;
                        for (const auto &entry : compilers)
                            if (entry.value("language", "") == lang)
                                compiler = entry.value("compiler", Json::object());
                        if (!compiler.is_object() || !compiler.contains("path"))
                            continue;
                        bool msvc = compiler.value("id", "") == "MSVC" ||
                                    utf8Path(compiler.at("path")).stem() == "clang-cl";
                        std::string command = argument(compiler.at("path"));
                        if (compiler.contains("commandFragment"))
                            command += " " + compiler.at("commandFragment").get<std::string>();
                        for (const auto &fragment : group.value("compileCommandFragments", Json::array()))
                            command += " " + fragment.at("fragment").get<std::string>();
                        for (const auto &define : group.value("defines", Json::array()))
                            command +=
                                " " + argument((msvc ? "/D" : "-D") + define.at("define").get<std::string>());
                        for (const auto &include : group.value("includes", Json::array()))
                            command += " " +
                                       std::string(msvc                               ? "/I"
                                                   : include.value("isSystem", false) ? "-isystem "
                                                                                      : "-I") +
                                       argument(include.at("path"));
                        for (const auto &include : compiler.value("implicit", Json::object())
                                                       .value("includeDirectories", Json::array()))
                            command += " " + std::string(msvc ? "/I" : "-isystem ") + argument(include);
                        if (msvc)
                            command += lang == "CXX" ? " /TP" : " /TC";
                        command += std::string(msvc ? " /c " : " -c ") + argument(path);
                        auto directory = resolve_(pathUtf8(
                            build / utf8Path(target.value("paths", Json::object()).value("build", ""))));
                        commands.push_back(
                            {{"directory", pathUtf8(directory)}, {"file", path}, {"command", command}});
                        seen.insert(path);
                        if (commands.size() > 100000)
                            throw std::runtime_error("Compilation database exceeds entry limit");
                    }
                }
            }
        }
    }
    auto native = resolve_(pathUtf8(build / "compile_commands.json"));
    auto generated = resolve_(
        pathUtf8(build / ".cycode/clangd" / utf8Path(value.at("configuration")) / "compile_commands.json"));
    if (fs::exists(native)) {
        auto entries = Json::parse(read(native));
        if (!entries.is_array())
            throw std::runtime_error("Invalid CMake compilation database");
        database = {{"path", pathUtf8(native.lexically_relative(root_))},
                    {"origin", "cmake"},
                    {"entries", entries.size()},
                    {"available", !entries.empty()}};
    } else if (generateDatabase && !commands.empty()) {
        writeAtomic(generated, commands.dump(2) + "\n");
        database = {{"path", pathUtf8(generated.lexically_relative(root_))},
                    {"origin", "cmake-file-api"},
                    {"entries", commands.size()},
                    {"available", true}};
    } else if (fs::exists(generated)) {
        auto entries = Json::parse(read(generated));
        if (!entries.is_array())
            throw std::runtime_error("Invalid CyCode compilation database");
        database = {{"path", pathUtf8(generated.lexically_relative(root_))},
                    {"origin", "cmake-file-api"},
                    {"entries", entries.size()},
                    {"available", !entries.empty()}};
    }
    result["compilationDatabase"] = database;
    if (stored.empty())
        result["message"] = "Configure the CMake project to load targets and compilation flags.";
    else if (!database["available"].get<bool>())
        result["message"] =
            "No usable compilation database. Configure with CyCode to request CMake File API metadata.";
    return result;
}
Json BuildSystem::run(const std::string &method, const Json &params) {
    std::lock_guard lock(operationMutex_);
    auto value = profile(params);
    auto source = resolve_(value.at("source")), build = resolve_(value.at("buildDirectory"));
    if (!fs::exists(source / "CMakeLists.txt"))
        throw std::runtime_error("No CMakeLists.txt in source directory");
    if (within(build, source))
        throw std::runtime_error("Choose a build directory separate from the source tree");
    auto stored = cache(build, resolve_);
    if (stored.contains("CMAKE_HOME_DIRECTORY") &&
        !samePath(utf8Path(stored.at("CMAKE_HOME_DIRECTORY")), source))
        throw std::runtime_error("Build directory belongs to another source tree; choose a new directory");
    auto tools = detect();
    if (tools.at("cmake") == "")
        throw std::runtime_error("CMake was not found");
    std::vector<std::string> args{tools.at("cmake")};
    if (method == "build.configure") {
        Json compiler;
        auto selected = value.at("toolchain").get<std::string>();
        for (const auto &entry : tools.at("toolchains"))
            if (entry.at("id") == selected)
                compiler = entry;
        if (!compiler.is_object() && selected != "automatic")
            throw std::runtime_error("Selected toolchain is not installed; refresh detection");
        if (!compiler.is_object() && stored.empty() && !tools.at("toolchains").empty())
            compiler = tools.at("toolchains").front();
        auto generator = value.at("generator").get<std::string>();
        if (generator == "automatic") {
            if (stored.contains("CMAKE_GENERATOR"))
                generator = stored.at("CMAKE_GENERATOR");
            else if (compiler.is_object() && compiler.value("family", "") == "MSVC") {
                auto version = compiler.value("installationVersion", "");
                auto prefix = "Visual Studio " + version.substr(0, version.find('.')) + " ";
                for (const auto &entry : tools.at("generators"))
                    if (entry.at("name").get<std::string>().starts_with(prefix))
                        generator = entry.at("name");
            } else
                generator = tools.at("ninja") != "" ? "Ninja" : "Unix Makefiles";
        }
        bool supported = false;
        for (const auto &entry : tools.at("generators"))
            if (entry.at("name") == generator)
                supported = true;
        if (!supported)
            throw std::runtime_error("Selected CMake generator is unavailable");
        if (stored.contains("CMAKE_GENERATOR") && stored.at("CMAKE_GENERATOR") != generator)
            throw std::runtime_error("Generator changed; choose a new build directory");
        if (compiler.is_object() && !stored.empty()) {
            auto current = inspectLocked(value);
            bool verified = false;
            for (const auto &entry : current.at("toolchains"))
                if (entry.value("language", "") == "CXX") {
                    auto path = entry.value("compiler", Json::object()).value("path", "");
                    if (!path.empty() && !samePath(utf8Path(path), utf8Path(compiler.at("cxxCompiler"))))
                        throw std::runtime_error("Compiler changed; choose a new build directory");
                    verified = !path.empty();
                }
            if (!verified && stored.contains("CMAKE_CXX_COMPILER")) {
                verified =
                    samePath(utf8Path(stored.at("CMAKE_CXX_COMPILER")), utf8Path(compiler.at("cxxCompiler")));
                if (!verified)
                    throw std::runtime_error("Compiler changed; choose a new build directory");
            }
            if (!verified)
                throw std::runtime_error("Existing compiler metadata is unavailable; configure with "
                                         "Automatic first or choose a new build directory");
        }
        args.insert(args.end(), {"-S", pathUtf8(source), "-B", pathUtf8(build), "-G", generator,
                                 "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"});
        bool visualStudio = generator.starts_with("Visual Studio ");
        if (compiler.is_object()) {
            if (compiler.value("family", "") == "MSVC") {
                if (!visualStudio)
                    throw std::runtime_error("MSVC requires a Visual Studio generator in this milestone");
                args.insert(args.end(),
                            {"-A", "x64", "-T",
                             "host=x64,version=" + compiler.at("version").get<std::string>(),
                             "-DCMAKE_GENERATOR_INSTANCE=" + compiler.at("installation").get<std::string>()});
            } else {
                if (visualStudio)
                    throw std::runtime_error("Choose Ninja or Makefiles for a Clang/GCC toolchain");
                args.insert(args.end(),
                            {"-DCMAKE_C_COMPILER=" + compiler.at("cCompiler").get<std::string>(),
                             "-DCMAKE_CXX_COMPILER=" + compiler.at("cxxCompiler").get<std::string>()});
            }
        }
        if (!visualStudio)
            args.push_back("-DCMAKE_BUILD_TYPE=" + value.at("configuration").get<std::string>());
        if (generator.starts_with("Ninja")) {
            if (tools.at("ninja") == "")
                throw std::runtime_error("Ninja was not found");
            args.push_back("-DCMAKE_MAKE_PROGRAM=" + tools.at("ninja").get<std::string>());
        }
        auto query = resolve_(pathUtf8(build / ".cmake/api/v1/query/client-cycode"));
        for (const auto *kind : {"codemodel-v2", "toolchains-v1"})
            writeAtomic(resolve_(pathUtf8(query / kind)), "");
        auto ignore = resolve_(pathUtf8(build / ".gitignore"));
        ignoreGeneratedTree(ignore);
    } else {
        if (stored.empty())
            throw std::runtime_error("Configure the project before building");
        auto current = inspectLocked(value);
        if (stored.value("CMAKE_CONFIGURATION_TYPES", "").empty() && stored.contains("CMAKE_BUILD_TYPE") &&
            stored.at("CMAKE_BUILD_TYPE") != "" && stored.at("CMAKE_BUILD_TYPE") != value.at("configuration"))
            throw std::runtime_error(
                "Configuration changed; reconfigure this single-configuration build first");
        args.insert(args.end(),
                    {"--build", pathUtf8(build), "--config", value.at("configuration"), "--parallel", "4"});
        const std::string target = value.at("target");
        if (!target.empty()) {
            bool found = false;
            for (const auto &entry : current.at("targets"))
                if (entry.at("name") == target)
                    found = true;
            if (!found)
                throw std::runtime_error("Unknown CMake target; refresh the configured project");
            args.insert(args.end(), {"--target", target});
        }
    }
    auto process = runProcess(args, root_, std::chrono::minutes(3), 8 * 1024 * 1024);
    Json result = {{"exitCode", process.exitCode},
                   {"output", process.output},
                   {"timedOut", process.timedOut},
                   {"truncated", process.truncated}};
    if (!process.exitCode) {
        auto project = inspectLocked(value, true);
        saveProfile(value);
        result["project"] = std::move(project);
    } else
        result["project"] = nullptr;
    return result;
}
} // namespace cycode
