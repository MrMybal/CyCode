#include "cycode/Engine.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
namespace fs = std::filesystem;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
static void write(const fs::path &path,const std::string &text) { std::ofstream out(path,std::ios::binary); out << text; }
static Json position(const std::string &text,const std::string &needle,size_t offset=0) {
    const auto at = text.rfind(needle); require(at != std::string::npos,"Missing symbol");
    auto start = text.rfind('\n',at); start = start == std::string::npos ? 0 : start+1;
    return {{"line",std::count(text.begin(),text.begin()+at,'\n')},{"character",LanguageClient::utf16Length(text.substr(start,at-start))+offset}};
}
template<class F> static Json eventually(F fn,const std::function<bool(const Json &)> &accept) {
    Json result; const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(30);
    do { result = fn(); if (accept(result)) return result; std::this_thread::sleep_for(std::chrono::milliseconds(150)); } while (std::chrono::steady_clock::now()<end);
    throw std::runtime_error("C# result did not converge: "+result.dump());
}
int main() {
    if (!fs::is_regular_file(utf8Path(LanguageService::managedExecutable()))) return 77;
    const auto dotnet = toolPath("dotnet"); if (dotnet.empty()) return 77;
    const auto root = fs::temp_directory_path()/utf8Path("cycode-csharp été #"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"Library"); fs::create_directories(root/"App");
    int code = 0;
    try {
        write(root/"NuGet.Config","<configuration><packageSources><clear /></packageSources></configuration>");
        write(root/"global.json","{\"sdk\":{\"version\":\"9.0.100\",\"rollForward\":\"latestFeature\"}}");
        write(root/"Library/Library.csproj","<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net8.0</TargetFramework></PropertyGroup></Project>");
        write(root/"App/App.csproj","<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net8.0</TargetFramework></PropertyGroup><ItemGroup><ProjectReference Include=\"../Library/Library.csproj\" /></ItemGroup></Project>");
        const std::string library = "namespace Demo;\npublic class Counter { public int Value; }\n";
        const std::string app = "using Demo;\npublic class Consumer { public int Read(Counter c) { return c.Value; } }\n";
        write(root/"Library/Counter.cs",library); write(root/"App/Consumer.cs",app);
        auto created = runProcess({dotnet,"new","sln","--name","Managed"},root,std::chrono::seconds(20));
        require(created.exitCode == 0,created.output);
        auto added = runProcess({dotnet,"sln","Managed.sln","add","Library/Library.csproj","App/App.csproj"},root,std::chrono::seconds(20));
        require(added.exitCode == 0,added.output);
        auto restored = runProcess({dotnet,"restore","Managed.sln","--configfile","NuGet.Config"},root,std::chrono::seconds(30));
        require(restored.exitCode == 0,restored.output);
        {
            Engine engine(root/"Managed.sln");
            auto doc = engine.dispatch("documents.read",{{"path","App/Consumer.cs"}});
            auto counter = engine.dispatch("documents.read",{{"path","Library/Counter.cs"}});
            require(!engine.languageStatus().at("servers").at("csharp").at("enabled").get<bool>(),"Readonly opening must not execute C# project imports");
            engine.dispatch("language.configure");
            auto query = [&](const std::string &method) { return engine.dispatch(method,{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]},{"position",position(doc["text"],"Value")}}); };
            const auto definition = eventually([&] { return query("language.definition"); },[](const Json &j) { return !j.at("locations").empty(); });
            require(definition.at("locations")[0].at("path") == "Library/Counter.cs","Cross-project definition: "+definition.dump());
            const auto hover=eventually([&] { return query("language.hover"); },[](const Json &j) { return j.at("available").get<bool>(); });
            require(hover.dump().find("Value")!=std::string::npos,"C# field documentation missing: "+hover.dump());
            const std::string signatureText="using Demo;\npublic class Consumer { public int Read(Counter c) { return System.Math.Max(1, 2); } }\n";
            doc=engine.dispatch("documents.replace",{{"path","App/Consumer.cs"},{"expectedRevision",doc.at("revision")},{"text",signatureText}});
            const auto signature=eventually([&] { return engine.dispatch("language.signature",{{"path","App/Consumer.cs"},{"expectedRevision",doc.at("revision")},{"position",position(signatureText,"2);")}}); },[](const Json &j) { return j.at("available").get<bool>(); });
            const auto &activeSignature=signature.at("signatures")[signature.at("activeSignature").get<int>()];
            require(activeSignature.at("parameters").size()==2,"C# method parameters missing");
            // csharp-ls 0.28 omits the index after the final separator. Do not
            // show the first parameter as active when the provider did not say so.
            require(activeSignature.at("activeParameter")==-1,"C# missing active index was fabricated: "+signature.dump());
            const auto firstArgument=engine.dispatch("language.signature",{{"path","App/Consumer.cs"},{"expectedRevision",doc.at("revision")},{"position",position(signatureText,"1,")}});
            require(firstArgument.at("signatures")[firstArgument.at("activeSignature").get<int>()].at("activeParameter")==0,"C# reported first argument was lost");
            doc=engine.dispatch("documents.replace",{{"path","App/Consumer.cs"},{"expectedRevision",doc.at("revision")},{"text",app}});
            const auto colors=engine.dispatch("language.highlight",{{"path","App/Consumer.cs"},{"expectedRevision",doc.at("revision")}});
            if (colors.at("supported").get<bool>()) require(!colors.at("tokens").empty(),"Advertised C# semantic color provider returned no tokens");
            else require(colors.at("tokens").empty(),"Unsupported C# color provider must retain lexical fallback");
            std::cout << "C# semantic token provider: " << colors.at("supported") << '\n';
            const auto references = query("language.references");
            require(references.at("locations").size() == 2,"Cross-project references: "+references.dump());
            auto symbols = engine.dispatch("language.symbols",{{"query","Counter"},{"language","csharp"},{"typesOnly",true}});
            require(!symbols.at("symbols").empty(),"C# type search");
            auto outline = engine.dispatch("language.documentSymbols",{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]}});
            require(outline.dump().find("Read") != std::string::npos,"C# member outline");
            auto plan = engine.dispatch("language.rename",{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]},{"position",position(app,"Value")},{"newName","Amount"}});
            require(plan.at("files").size() == 2,"C# cross-project rename: "+plan.dump());
            engine.dispatch("changes.apply",{{"id",plan["id"]}});
            doc = engine.dispatch("documents.read",{{"path","App/Consumer.cs"}});
            require(doc.at("text").get<std::string>().find("c.Amount") != std::string::npos && doc.at("dirty"),"C# rename changed live buffers");
            engine.dispatch("changes.undo",{{"id",plan["id"]}});
            doc = engine.dispatch("documents.read",{{"path","App/Consumer.cs"}});
            counter = engine.dispatch("documents.read",{{"path","Library/Counter.cs"}});
            counter = engine.dispatch("documents.replace",{{"path","Library/Counter.cs"},{"expectedRevision",counter["revision"]},{"text","namespace Demo;\npublic class Counter { public int Value; public int UnsavedMember; }\n"}});
            doc = engine.dispatch("documents.replace",{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]},{"text","using Demo;\npublic class Consumer { public int Read(Counter c) { return c.Un; } }\n"}});
            auto completion = eventually([&] { return engine.dispatch("language.complete",{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]},{"position",position(doc["text"],"c.Un",4)}}); },[](const Json &j) { return j.dump().find("UnsavedMember") != std::string::npos; });
            require(!completion.at("result").is_null(),"Completion from unsaved referenced project");
            const auto &items=completion.at("result").at("items");
            const auto selected=std::find_if(items.begin(),items.end(),[](const Json &item) { return item.at("label").get<std::string>().starts_with("UnsavedMember"); });
            require(selected!=items.end(),"Managed completion candidate missing");
            const auto prepared=engine.dispatch("language.completionPrepare",{{"sessionId",completion.at("sessionId")},{"item",std::distance(items.begin(),selected)},{"expectedRevision",doc.at("revision")}});
            const auto preview=engine.dispatch("changes.preview",{{"id",prepared.at("plan").at("id")},{"path","App/Consumer.cs"}});
            require(preview.at("after").get<std::string>().find("c.UnsavedMember")!=std::string::npos,"Managed resolve insertion failed");
            engine.dispatch("changes.apply",{{"id",prepared.at("plan").at("id")}});engine.dispatch("changes.undo",{{"id",prepared.at("plan").at("id")}});
            doc=engine.dispatch("documents.read",{{"path","App/Consumer.cs"}});
            doc = engine.dispatch("documents.replace",{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]},{"text","public  class Consumer{public   int Broken=\"bad\";}\n"}});
            auto diagnostics = eventually([&] { return engine.languageStatus(); },[](const Json &j) { return j.at("diagnostics").dump().find("CS0029") != std::string::npos; });
            require(diagnostics.at("servers").at("csharp").at("ready"),"Managed service state");
            auto format = engine.dispatch("language.format",{{"path","App/Consumer.cs"},{"expectedRevision",doc["revision"]}});
            require(format.at("editCount").get<int>() > 0,"C# Roslyn formatting: "+format.dump());
            engine.dispatch("language.configure",{{"enabled",false}});
            eventually([&] { return engine.languageStatus(); },[](const Json &j) { return !j.at("servers").at("csharp").at("running").get<bool>(); });
            engine.dispatch("language.configure");
            eventually([&] { return engine.languageStatus(); },[](const Json &j) { return j.at("diagnostics").dump().find("CS0029") != std::string::npos; });
            write(root/"native.cpp","int value; int Read() { return value; }\n");
            auto native = engine.dispatch("documents.read",{{"path","native.cpp"}});
            auto cpp = engine.dispatch("language.definition",{{"path","native.cpp"},{"expectedRevision",native["revision"]},{"position",position(native["text"],"value")}});
            require(cpp.at("locations").size() == 1,"C++ routing remains available beside C#");
            std::ifstream disk(root/"Library/Counter.cs"); std::string text((std::istreambuf_iterator<char>(disk)),{});
            require(text == library,"Language operations wrote to disk without save");
        }
        std::cout << "C# Roslyn: cross-project navigation/references/rename, unsaved completion/diagnostics, symbols/format, restart and concurrent C++ passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; code = 1; }
    std::error_code ec; fs::remove_all(root,ec); return code;
}
