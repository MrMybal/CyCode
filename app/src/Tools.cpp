#include "cycode/Tools.h"
#include "cycode/Process.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
namespace cycode {
namespace fs = std::filesystem;
static fs::path environment(const std::string &name) {
#ifdef _WIN32
    auto key = utf8Path(name).wstring();
    auto length = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (!length) return {};
    std::wstring value(length, 0); GetEnvironmentVariableW(key.c_str(), value.data(), length); value.resize(length-1); return value;
#else
    auto value = std::getenv(name.c_str()); return value ? fs::path(value) : fs::path{};
#endif
}
static std::string executableName(const std::string &name) {
#ifdef _WIN32
    return name.ends_with(".exe") ? name : name + ".exe";
#else
    return name;
#endif
}
static std::vector<fs::path> pathCandidates(const std::string &name) {
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    std::vector<fs::path> candidates;
    const auto filename = executableName(name);
    std::istringstream paths(pathUtf8(environment("PATH")));
    std::string part;
    while (std::getline(paths, part, separator)) {
        if (part.size() > 1 && part.front() == '"' && part.back() == '"')
            part = part.substr(1, part.size() - 2);
        auto directory = utf8Path(part);
        if (directory.is_absolute()) candidates.push_back(directory / filename);
    }
    return candidates;
}
static fs::path firstExecutable(const std::vector<fs::path> &candidates) {
    for (const auto &candidate : candidates) {
        std::error_code ec;
        if (!candidate.is_absolute() || !fs::is_regular_file(candidate, ec) || ec) continue;
#ifndef _WIN32
        if (access(candidate.c_str(), X_OK) != 0) continue;
#endif
        // The executable name is part of the invocation contract: clang++/g++
        // select the C++ link runtime through argv[0]. Resolving their symlink
        // to clang/gcc changes that behavior. Environment interpreters also
        // need to retain their original location. Do not normalize through links.
        return candidate;
    }
    return {};
}
fs::path findExecutableOnPath(const std::string &name, const std::vector<fs::path> &hints) {
    auto candidates = pathCandidates(name);
    candidates.insert(candidates.end(), hints.begin(), hints.end());
    return firstExecutable(candidates);
}
bool sameExecutableInvocation(const fs::path &a, const fs::path &b) {
    if (!a.is_absolute() || !b.is_absolute() || a.filename().empty() || b.filename().empty()) return false;
#ifdef _WIN32
    if (_wcsicmp(a.filename().c_str(), b.filename().c_str()) != 0) return false;
#else
    if (a.filename() != b.filename()) return false;
#endif
    std::error_code leftError, rightError;
    auto left = fs::weakly_canonical(a.parent_path(), leftError);
    auto right = fs::weakly_canonical(b.parent_path(), rightError);
    if (leftError || rightError) return false;
#ifdef _WIN32
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
#else
    return left == right;
#endif
}
std::filesystem::path installationRoot() {
    fs::path executable;
#ifdef _WIN32
    std::wstring buffer(32768, 0); auto count = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size())); buffer.resize(count); executable = buffer;
#elif defined(__APPLE__)
    uint32_t size = 0; _NSGetExecutablePath(nullptr, &size); std::string buffer(size, 0); if (_NSGetExecutablePath(buffer.data(), &size) == 0) executable = buffer.c_str();
#else
    std::string buffer(32768, 0); auto length = readlink("/proc/self/exe", buffer.data(), buffer.size()); if (length > 0) { buffer.resize(size_t(length)); executable = buffer; }
#endif
    auto directory = executable.parent_path();
    for (int i = 0; i < 8 && !directory.empty(); ++i) {
        std::error_code ec;
        if (fs::is_regular_file(directory / "app/CyTool.json", ec)) return directory;
        if (directory.parent_path() == directory) break;
        directory = directory.parent_path();
    }
    return {};
}
std::string toolPath(const std::string &name) {
    auto input = utf8Path(name);
    if (input.is_absolute()) return name;
    std::string upper = name; std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    std::vector<fs::path> candidates;
    auto override = environment("CYCODE_" + upper);
    if (override.is_absolute()) candidates.push_back(override);
    const auto filename = executableName(name);
    auto root = installationRoot();
    // Release archives keep their explicitly packaged tools outside the private
    // development runtime. Environment overrides still take precedence.
    if (!root.empty()) candidates.push_back(root / "tools" / filename);
    if (name == "codelldb" && !root.empty()) candidates.push_back(root / "runtime/tools/codelldb/extension/adapter" / filename);
    if (name == "netcoredbg" && !root.empty()) candidates.push_back(root / "runtime/tools/netcoredbg/netcoredbg" / filename);
    if (!root.empty()) { candidates.push_back(root / "runtime/tools" / filename); candidates.push_back(root / "runtime/tools" / name / filename); }
    auto systemCandidates = pathCandidates(name);
    candidates.insert(candidates.end(), systemCandidates.begin(), systemCandidates.end());
#ifdef _WIN32
    auto programFiles = environment("ProgramFiles"); if (programFiles.empty()) programFiles = "C:/Program Files";
    if (name == "git") candidates.push_back(programFiles / "Git/cmd/git.exe");
    if (name == "clangd") { candidates.push_back(programFiles / "LLVM/bin/clangd.exe"); candidates.push_back(programFiles / "Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clangd.exe"); }
    if (name == "lldb-dap") candidates.push_back(programFiles / "LLVM/bin/lldb-dap.exe");
    if (name == "dotnet") candidates.push_back(programFiles / "dotnet/dotnet.exe");
    if(name=="cmake"||name=="ctest")for(const auto *edition:{"Community","Professional","Enterprise","BuildTools"})
        candidates.push_back(programFiles/"Microsoft Visual Studio/2022"/edition/"Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"/filename);
    if (name == "msbuild") {
        auto programFilesX86 = environment("ProgramFiles(x86)"); if (programFilesX86.empty()) programFilesX86 = "C:/Program Files (x86)";
        auto vswhere = programFilesX86 / "Microsoft Visual Studio/Installer/vswhere.exe";
        if (fs::is_regular_file(vswhere)) {
            auto found = runProcess({pathUtf8(vswhere),"-latest","-products","*","-requires","Microsoft.Component.MSBuild","-property","installationPath","-utf8"},programFiles,std::chrono::seconds(5),65536);
            auto text = found.output; while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
            if (!found.exitCode && !text.empty()) { auto installation = utf8Path(text); candidates.push_back(installation/"MSBuild/Current/Bin/amd64/MSBuild.exe"); candidates.push_back(installation/"MSBuild/Current/Bin/MSBuild.exe"); }
        }
    }
#else
    candidates.push_back(fs::path("/usr/local/bin") / filename); candidates.push_back(fs::path("/usr/bin") / filename); candidates.push_back(fs::path("/opt/homebrew/bin") / filename);
#endif
    return pathUtf8(firstExecutable(candidates));
}
} // namespace cycode
