#include "cycode/Engine.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
static void check(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
static void write(const fs::path &path,const std::string &text) { fs::create_directories(path.parent_path()); std::ofstream(path,std::ios::binary) << text; }
template<class F> static void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception &) { failed = true; } check(failed,"Expected rejection"); }
int main() {
#ifndef _WIN32
    return 77;
#else
    auto root = fs::temp_directory_path()/utf8Path("cycode-msbuild-é-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root); std::string cmake;
        { Engine discovery(root); if (discovery.dispatch("msbuild.tools").at("tools").empty()) return 77; cmake = discovery.dispatch("toolchains.detect").at("cmake"); }
        write(root/"src/CMakeLists.txt", "cmake_minimum_required(VERSION 3.24)\nproject(MsFixture LANGUAGES CXX)\nset_property(GLOBAL PROPERTY USE_FOLDERS ON)\nadd_executable(Cpp.One main.cpp)\nset_property(TARGET Cpp.One PROPERTY FOLDER Apps)\ntarget_include_directories(Cpp.One PRIVATE include)\ntarget_compile_definitions(Cpp.One PRIVATE WIDGET_VALUE=42)\nset_property(TARGET Cpp.One PROPERTY CXX_STANDARD 20)\nadd_executable(Second other.cpp)\n");
        write(root/"src/main.cpp","#include <cstdio>\n#include \"widget.hpp\"\nint main(){ Widget w; std::printf(\"MSBUILD_%d\\n\",w.Value); return w.Value == 42 ? 0 : 1; }\n");
        write(root/"src/include/widget.hpp","#pragma once\nstruct Widget { int Value = WIDGET_VALUE; };\n");
        write(root/"src/other.cpp","int main(){return 0;}\n");
        auto configure = runProcess({cmake,"-S",pathUtf8(root/"src"),"-B",pathUtf8(root),"-G","Visual Studio 17 2022","-A","x64"},root,std::chrono::seconds(40));
        if (configure.exitCode) throw std::runtime_error(configure.output);
        const auto sln = root/"MsFixture.sln"; Engine engine(sln);
        auto call = [&](const std::string &method,Json params = {}) { if (params.is_null()) params = Json::object(); auto value = engine.dispatch(method,params); if (value.contains("exitCode") && value.at("exitCode") != 0) throw std::runtime_error(method+": "+value.at("output").get<std::string>()); return value; };
        auto model = call("msbuild.inspect",{{"configuration","Debug"},{"platform","x64"}});
        check(model.at("available") && model.at("configurations").size() >= 2,"Solution inspection");
        check(!fs::exists(root/".cycode"),"Inspection must not evaluate/save projects");
        Json project; for (const auto &item : model.at("projects")) if (item.at("name") == "Cpp.One") project = item;
        check(project.is_object() && project.at("activeConfiguration").at("build"),"Solution project mapping");
        Json parameters = {{"configuration","Debug"},{"platform","x64"},{"project",project.at("id")}};
        call("msbuild.build",parameters);
        check(fs::is_regular_file(root/"Debug/Cpp.One.exe") && !fs::exists(root/"Debug/Second.exe"),"Selected nested dotted project target");
        auto run = runProcess({pathUtf8(root/"Debug/Cpp.One.exe")},root,std::chrono::seconds(5)); check(!run.exitCode && run.output.find("MSBUILD_42") != std::string::npos,"Actual native artifact executes");
        auto evaluated = call("msbuild.evaluate",parameters); check(evaluated.at("evaluation").at("Items").at("ClCompile").size() == 1,"Real MSBuild evaluation");
        check(evaluated.at("project").at("artifacts")[0].at("path").get<std::string>().find("Cpp.One.exe") != std::string::npos,"Actual target artifact");
        auto prepared = call("msbuild.prepare",parameters); check(prepared.at("project").at("compilationDatabase").at("entries") == 1,"Derived compilation database");
        if (auto clangd = toolPath("clangd"); !clangd.empty()) {
            engine.configureLanguage(clangd,true); auto doc = call("documents.read",{{"path","src/main.cpp"}});
            auto definition = call("language.definition",{{"path","src/main.cpp"},{"expectedRevision",doc.at("revision")},{"position",{{"line",2},{"character",12}}}});
            check(!definition.at("locations").empty() && definition.at("locations")[0].at("path") == "src/include/widget.hpp","clangd uses evaluated project includes");
        }
        check(call("msbuild.inspect").at("compilationDatabase").at("available"),"Saved profile/database reload");
        rejects([&] { call("msbuild.build",{{"configuration","Debug;Custom=evil"}}); });
        rejects([&] { call("msbuild.inspect",{{"entry","../outside.sln"}}); });
        rejects([&] { call("msbuild.build",{{"project","unknown"}}); });
        call("msbuild.clean",parameters); check(!fs::exists(root/"Debug/Cpp.One.exe"),"Actual clean");
        call("msbuild.rebuild",parameters); check(fs::exists(root/"Debug/Cpp.One.exe"),"Actual rebuild");
        { Engine direct(root/"Cpp.One.vcxproj"); check(direct.dispatch("msbuild.inspect").at("available"),"Direct vcxproj open"); }
        std::ifstream input(sln,std::ios::binary); std::string text((std::istreambuf_iterator<char>(input)),{}); input.close();
        std::string upper = project.at("id"); for (auto &c : upper) c = char(std::toupper(static_cast<unsigned char>(c)));
        for (auto suffix : {"ActiveCfg","Build.0"}) { const auto from = upper+".Debug|x64."+suffix+" = Debug|x64", to = upper+".Debug|x64."+suffix+" = Release|x64"; auto at = text.find(from); check(at != std::string::npos,"Project mapping exists"); text.replace(at,from.size(),to); }
        write(sln,text); auto mapped = call("msbuild.inspect",parameters); for (const auto &item : mapped.at("projects")) if (item.at("id") == project.at("id")) check(item.at("activeConfiguration").at("configuration") == "Release","Mapped project configuration");
        check(!mapped.at("compilationDatabase").at("available").get<bool>(),"Changed solution invalidates cached analysis");
        call("msbuild.build",parameters); check(fs::exists(root/"Release/Cpp.One.exe"),"MSBuild honors solution mapping");
        write(root/"src/main.cpp","int main() { UnknownValue; }\n");
        const auto failure = engine.dispatch("msbuild.build",parameters);
        check(failure.at("exitCode") != 0 && !failure.at("diagnostics").empty() && failure.at("diagnostics")[0].at("path") == "src/main.cpp","Actual compiler diagnostics are navigable");
        write(root/"managed/Managed.csproj","<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net8.0</TargetFramework><OutputType>Exe</OutputType></PropertyGroup></Project>");
        write(root/"managed/Program.cs","System.Console.WriteLine(\"CSHARP_42\");\n");
        write(root/"managed/NuGet.Config","<configuration><packageSources><clear /></packageSources></configuration>");
        { Engine managed(root/"managed/Managed.csproj"); auto result = managed.dispatch("msbuild.build",{{"tool","dotnet"},{"restore",true}}); if (result.at("exitCode") != 0) throw std::runtime_error(result.at("output").get<std::string>());
            auto metadata = managed.dispatch("msbuild.evaluate",{{"tool","dotnet"}}); check(metadata.at("exitCode") == 0,"Managed evaluation");
            auto artifact = metadata.at("project").at("artifacts")[0].at("path").get<std::string>();
            auto output = runProcess({toolPath("dotnet"),artifact},root,std::chrono::seconds(10)); check(!output.exitCode && output.output.find("CSHARP_42") != std::string::npos,"Actual .NET build and execution");
        }
        std::cout << "MSBuild native/managed build, mappings, targets, index, artifacts and guards passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << "\nFixture: " << pathUtf8(root) << '\n'; return 1; }
    fs::remove_all(root); return 0;
#endif
}
