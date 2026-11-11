#include "cycode/DebugRuntime.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
namespace cycode {
namespace fs = std::filesystem;
using Json = nlohmann::json;
static bool assembly(const fs::path &path) {
    std::ifstream in(path,std::ios::binary); if (!in) return false;
    auto word = [&](size_t at,size_t bytes) -> uint32_t {
        in.clear(); in.seekg(static_cast<std::streamoff>(at)); std::array<unsigned char,4> data{};
        if (!in.read(reinterpret_cast<char *>(data.data()),static_cast<std::streamsize>(bytes))) return 0;
        uint32_t result = 0; for (size_t i=0;i<bytes;++i) result |= uint32_t(data[i]) << (i*8); return result;
    };
    if (word(0,2) != 0x5a4d) return false;
    const auto pe = word(0x3c,4); if (pe > 16*1024*1024 || word(pe,4) != 0x4550) return false;
    const auto optional = pe+24, magic = word(optional,2);
    if (magic != 0x10b && magic != 0x20b) return false;
    const auto directory = optional+(magic == 0x20b ? 112 : 96);
    if (word(directory-4,4) <= 14 || word(pe+20,2) < directory-optional+15*8) return false;
    return word(directory+14*8,4) != 0 && word(directory+14*8+4,4) >= 0x48;
}
static fs::path managedAssembly(const fs::path &path) {
    if (assembly(path)) return path;
    auto sibling = path; sibling.replace_extension(".dll");
    auto configuration = path; configuration.replace_extension(".runtimeconfig.json");
    if (fs::is_regular_file(configuration) && assembly(sibling)) return sibling;
    return {};
}
bool isManagedProgram(const fs::path &path) { return !managedAssembly(path).empty(); }
bool isRunnableArtifact(const fs::path &path) {
    if (!fs::is_regular_file(path)) return false;
    if (path.extension() == ".exe") return true;
    auto config = path; config.replace_extension(".runtimeconfig.json");
    return assembly(path) && fs::is_regular_file(config);
}
std::string debugRuntime(const fs::path &path,const std::string &requested) {
    if (requested != "automatic" && requested != "native" && requested != "dotnet") throw std::runtime_error("Unknown debug runtime");
    return requested == "automatic" ? (isManagedProgram(path) ? "dotnet" : "native") : requested;
}
static std::string dotnet(const fs::path &program,const Json &profile) {
    const auto requested = profile.value("dotnet","automatic");
    if (requested != "automatic" && !requested.empty()) {
        auto found = toolPath(requested);
        if (found.empty() || !fs::is_regular_file(utf8Path(found))) throw std::runtime_error("Configured .NET host is missing");
        return found;
    }
    std::vector<std::string> hosts; auto system = toolPath("dotnet"); if (!system.empty()) hosts.push_back(system);
    auto bundled = installationRoot()/"runtime/tools/csharp-dotnet";
#ifdef _WIN32
    bundled /= "dotnet.exe";
#else
    bundled /= "dotnet";
#endif
    if (fs::is_regular_file(bundled) && std::find(hosts.begin(),hosts.end(),pathUtf8(bundled)) == hosts.end()) hosts.push_back(pathUtf8(bundled));
    if (hosts.empty()) throw std::runtime_error("No .NET host installed; select one in the debug profile");
    auto config = program; config.replace_extension(".runtimeconfig.json");
    Json frameworks = Json::array();
    if (fs::is_regular_file(config)) {
        if (fs::file_size(config) > 65536) throw std::runtime_error("Runtime configuration exceeds 64 KiB");
        std::ifstream input(config); const auto options = Json::parse(input).value("runtimeOptions",Json::object());
        if (options.contains("framework")) frameworks.push_back(options.at("framework"));
        for (const auto &f : options.value("frameworks",Json::array())) frameworks.push_back(f);
    }
    if (frameworks.empty()) return hosts.front();
    for (const auto &host : hosts) {
        auto result = runProcess({host,"--list-runtimes"},program.parent_path(),std::chrono::seconds(5),65536);
        if (result.exitCode || result.timedOut) continue;
        bool matches = true;
        for (const auto &framework : frameworks) {
            const auto version = framework.value("version","");
            const auto prefix = framework.value("name","")+" "+version.substr(0,version.find('.'))+".";
            std::istringstream lines(result.output); std::string line; bool found = false;
            while (std::getline(lines,line)) if (line.starts_with(prefix)) found = true;
            matches &= found;
        }
        if (matches) return host;
    }
    throw std::runtime_error("Required .NET runtime is unavailable in the detected hosts: "+frameworks.dump()+". Install it or select a compatible .NET host.");
}
std::string netCoreDebugArgument(const std::string &text, bool windows) {
    if (text.empty()) throw std::runtime_error("NetCoreDbg cannot preserve empty launch arguments; use Run without debugging for this argument list");
    size_t trailing = 0;
    while (trailing < text.size() && text[text.size()-trailing-1] == '\\') ++trailing;
    if (trailing && text.find(' ') == std::string::npos) {
        if (text.find_first_of("\"\t\r\n") != std::string::npos)
            throw std::runtime_error("NetCoreDbg cannot preserve this trailing-backslash argument");
        return text;
    }
    if (!windows) {
        // PAL removes only the slash immediately before a quote, whereas CRT
        // counts runs of slashes. NetCoreDbg drops spaced trailing slashes on PAL.
        if (trailing) throw std::runtime_error("NetCoreDbg cannot preserve a spaced argument ending with a backslash on this platform; use Run without debugging");
        std::string escaped;
        for (const auto c : text) { if (c == '"') escaped += '\\'; escaped += c; }
        return escaped;
    }
    std::string escaped;
    size_t slashes = 0;
    for (size_t i=0;i<text.size()-trailing;++i) {
        const auto c = text[i];
        if (c == '\\') { ++slashes; continue; }
        escaped.append(c == '"' ? slashes*2+1 : slashes,'\\');
        slashes = 0; escaped += c;
    }
    // For a spaced trailing backslash, NetCoreDbg adds one itself on Windows.
    if (trailing) escaped.append(trailing*2-1,'\\');
    return escaped;
}
DebugProgram debugProgram(const fs::path &root,const Json &profile) {
    auto program = utf8Path(profile.value("program","")); if (program.empty()) throw std::runtime_error("Select a compiled program or .NET assembly in the debug profile");
    if (program.is_relative()) program = root/program;
    if (!fs::is_regular_file(program)) throw std::runtime_error("Debug program does not exist: "+pathUtf8(program));
    DebugProgram result{debugRuntime(program,profile.value("runtime","automatic")),{}};
    if (result.runtime == "dotnet") {
        const auto dll = managedAssembly(program);
        if (dll.empty()) throw std::runtime_error("The selected program is not a managed .NET assembly/apphost");
        result.argv = {dotnet(dll,profile),pathUtf8(fs::weakly_canonical(dll))};
    } else result.argv = {pathUtf8(fs::weakly_canonical(program))};
    auto args = profile.value("args",std::vector<std::string>{}); result.argv.insert(result.argv.end(),args.begin(),args.end()); return result;
}
}
