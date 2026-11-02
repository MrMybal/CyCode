#include "cycode/LanguageInfo.h"
#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace cycode;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F action) { bool rejected=false;try { action(); } catch (...) { rejected=true; }require(rejected,"Invalid language information accepted"); }
static Json position(const std::string &text,size_t offset) {
    const auto line=std::count(text.begin(),text.begin()+offset,'\n');const auto newline=text.rfind('\n',offset);
    return {{"line",line},{"character",LanguageClient::utf16Length(text.substr(newline==std::string::npos ? 0 : newline+1,offset-(newline==std::string::npos ? 0 : newline+1)))}};
}
static void normalization() {
    require(!hoverInformation(nullptr,"").at("available").get<bool>(),"Null hover");
    require(signatureInformation(nullptr).at("activeSignature")==-1,"Null signature");
    const auto range=Json{{"start",{{"line",0},{"character",2}}},{"end",{{"line",0},{"character",7}}}};
    auto hover=hoverInformation({{"contents",Json::array({"**symbol**",{{"language","cpp"},{"value","int greet();"}},{{"kind","plaintext"},{"value","<script>literal</script>"}}})},{"range",range}},"🛠greet");
    require(hover.at("contents").size()==3 && hover.at("contents")[1].at("kind")=="code","MarkedString hover");
    require(hover.at("contents")[2].at("text")=="<script>literal</script>" && hover.at("range")==range,"Literal documentation/range");
    rejects([&] { auto invalid=range;invalid["start"]["character"]=1;hoverInformation({{"contents","bad"},{"range",invalid}},"🛠greet"); });
    rejects([&] { hoverInformation({{"contents",{{"kind","html"},{"value","bad"}}}},""); });
    const auto parameters=Json::array({{{"label",Json::array({5,7})}},{{"label",Json::array({9,15})},{"documentation",{{"kind","markdown"},{"value","**Amount**"}}}}});
    auto signature=signatureInformation({{"signatures",Json::array({{{"label","call(🛠, amount)"},{"parameters",parameters},{"activeParameter",1}}})},{"activeSignature",999},{"activeParameter",0}});
    const auto &selected=signature.at("signatures")[0];
    require(signature.at("activeSignature")==0 && selected.at("activeParameter")==1,"Per-signature active parameter and fallback overload");
    require(selected.at("parameters")[0].at("label")=="🛠" && selected.at("parameters")[0].at("startByte")==5 && selected.at("parameters")[0].at("endByte")==9,"UTF-16 label offsets");
    require(selected.at("parameters")[1].at("label")=="amount" && selected.at("parameters")[1].at("startByte")==11,"Second UTF-16 label offset");
    rejects([&] { auto invalid=parameters;invalid[0]["label"]=Json::array({6,7});signatureInformation({{"signatures",Json::array({{{"label","call(🛠, amount)"},{"parameters",invalid}}})}}); });
    auto repeated=signatureInformation({{"signatures",Json::array({{{"label","call(value, value)"},{"parameters",Json::array({{{"label","value"}},{{"label","value"}}})}}})}});
    require(repeated.at("signatures")[0].at("parameters")[1].at("startByte")==12,"Repeated parameter label mapping");
    require(repeated.at("signatures")[0].at("activeParameter")==-1,"Unreported active parameter was invented");
    const auto unknown=signatureInformation({{"signatures",Json::array({{{"label","f(int x)"},{"parameters",Json::array({{{"label","int x"}}})},{"activeParameter",nullptr}}})},{"activeParameter",0}});
    require(unknown.at("signatures")[0].at("activeParameter")==-1,"Null per-signature selection did not override global index");
    std::string large;for (int i=0;i<30000;++i) large+="é🛠";
    hover=hoverInformation({{"contents",large}},"");
    require(hover.at("truncated").get<bool>() && hover.at("contents")[0].at("text").get<std::string>().size()<=65536,"Documentation byte bound");
    (void)hover.dump();
    auto many=Json::array();for (int i=0;i<40;++i) many.push_back({{"label","f()"}});
    signature=signatureInformation({{"signatures",many}});
    require(signature.at("signatures").size()==32 && signature.at("truncated").get<bool>(),"Overload bound");
    const auto sanitized=hoverInformation({{"contents",std::string("text\0\x1b",6)}},"");
    require(sanitized.at("contents")[0].at("text")=="text  ","Documentation control characters");
}
static int actualCpp() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("cycode-info-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    try {
        const std::string text="/// Combine two values.\nint combine(int left, int right) { return left + right; }\nint main() { return combine(1, 2); }\n";
        { std::ofstream source(root/"main.cpp");source<<text; }
        {
            Engine engine(root);auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
            const auto call=text.rfind("combine(");
            auto query=Json{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"position",position(text,call+2)}};
            const auto hover=engine.dispatch("language.hover",query);
            require(hover.at("available").get<bool>() && hover.dump().find("combine")!=std::string::npos,"clangd hover missing: "+hover.dump());
            query["position"]=position(text,text.find("2);",call));
            const auto signature=engine.dispatch("language.signature",query);
            require(signature.at("available").get<bool>(),"clangd signatures missing: "+signature.dump());
            require(signature.at("signatures")[signature.at("activeSignature").get<int>()].at("activeParameter")==1,"Second argument not selected: "+signature.dump());
            auto live=text;live.replace(live.find("int combine"),3,"long");
            doc=engine.dispatch("documents.replace",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"text",live}});
            query["expectedRevision"]=doc.at("revision");query["position"]=position(live,live.rfind("combine(")+2);
            const auto updated=engine.dispatch("language.hover",query);
            require(updated.dump().find("long")!=std::string::npos,"Hover ignored live buffer: "+updated.dump());
            query["expectedRevision"]=1;rejects([&] { engine.dispatch("language.hover",query); });
            engine.configureLanguage("",false,false);
        }
        fs::remove_all(root);return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\nArtifacts: "<<root<<'\n';return 1; }
}
int main(int argc,char **argv) {
    try { if (argc>1 && std::string(argv[1])=="--cpp") return actualCpp();normalization(); }
    catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
