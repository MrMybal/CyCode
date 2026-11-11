#include "cycode/UserSnippets.h"
#include "cycode/Engine.h"
#include "cycode/NativeFileLease.h"
#include "cycode/TextFile.h"
#include "cycode/Completion.h"
#include <fstream>
#include <stdexcept>
using namespace cycode;
namespace {
static_assert(userSnippetRevision("absent"));
static_assert(userSnippetRevision("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
static_assert(!userSnippetRevision("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"));
static_assert(!userSnippetRevision("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"));
static_assert(!userSnippetRevision("0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"));
static_assert(!userSnippetRevision("g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
static_assert(!userSnippetRevision("disabled"));
static_assert(!userSnippetRevision(""));
static_assert(userSnippetBytes(0));
static_assert(userSnippetBytes(256*1024));
static_assert(!userSnippetBytes(256*1024+1));
static_assert(!userSnippetBytes(size_t(-1)));
static_assert(!captureSnippetBody(""));
static_assert(captureSnippetBody("plain")->text()=="plain$0");
static_assert(captureSnippetBody("$1")->text()=="\\$1$0");
static_assert(captureSnippetBody("${TM_FILENAME}")->text()=="\\${TM_FILENAME\\}$0");
static_assert(captureSnippetBody("\\")->text()=="\\\\$0");
static_assert(captureSnippetBody("}")->text()=="\\}$0");
static_assert(captureSnippetBody("{")->text()=="{$0");
static_assert(captureSnippetBody("a\nb")->text()=="a\nb$0");
static_assert(captureSnippetBody("é🛠")->text()=="é🛠$0");
static_assert(!captureSnippetBody(std::string_view("a\0b",3)));
constexpr bool captureLimit(size_t count,char byte) {
    std::array<char,8193> text{};
    for(size_t i=0;i<count;++i)text[i]=byte;
    return bool(captureSnippetBody({text.data(),count}));
}
static_assert(captureLimit(8190,'a'));
static_assert(!captureLimit(8191,'a'));
static_assert(captureLimit(4095,'$'));
static_assert(!captureLimit(4096,'$'));
void require(bool value) {if(!value)throw std::runtime_error("User snippet contract failed");}
template<class F> void rejects(F action) {bool bad=false;try{action();}catch(...){bad=true;}require(bad);}
}
void userSnippetContracts() {
    namespace fs=std::filesystem;
    rejects([]{UserSnippets invalid("relative");});
    UserSnippets disabled;
    require(disabled.snapshot().at("available")==false && disabled.generation()==0);
    rejects([&]{disabled.save("{}","absent");});
    const auto root=fs::temp_directory_path()/utf8Path("cycode-personal-snippets-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"one");fs::create_directories(root/"two");
    const auto directory=root/"preferences",file=directory/"user.code-snippets";
    const std::string first=Json{{"Personal",{{"prefix","personal"},{"body","hello ${1:$TM_FILENAME_BASE}$0"}}}}.dump();
    const std::string second=Json{{"Other",{{"prefix","other"},{"body","changed"}}}}.dump();
    auto write=[](const fs::path &path,const std::string &text){std::ofstream(path,std::ios::binary)<<text;};
    {
        UserSnippets a(directory),b(directory);
        const auto missing=a.snapshot();require(missing.at("revision")=="absent" && missing.at("valid")==true);
        require(!fs::exists(directory)); // Inspection must not create preferences or lock files.
        const auto draft=a.draft("literal $1 \\ } 🛠","cpp");
        require(!fs::exists(directory) && draft.at("catalogue")==a.snapshot());
        const auto definitions=Json::parse(draft.at("draft").get<std::string>());
        require(definitions.at(draft.at("name").get<std::string>()).at("scope")=="cpp");
        require(plainSnippet(definitions.begin().value().at("body"))=="literal $1 \\ } 🛠");
        const auto saved=a.save(first,"absent");require(userSnippetRevision(saved.at("revision").get<std::string>()));
        require(b.snapshot().at("text")==first && fs::exists(directory/"user.code-snippets.lock"));
        const auto newer=b.save(second,saved.at("revision"));
        rejects([&]{a.save(first,saved.at("revision"));});require(readTextFileBytes(file)==second);
        {
            NativeFileLease held(directory/"user.code-snippets.lock");require(held.acquired());
            rejects([&]{a.save(first,newer.at("revision"));});require(readTextFileBytes(file)==second);
        }
        write(file,"{broken");const auto broken=a.snapshot();
        require(broken.at("editable")==true && broken.at("valid")==false && broken.at("error")!="");
        rejects([&]{a.draft("code","cpp");});require(readTextFileBytes(file)=="{broken");
        a.save(first,broken.at("revision"));require(readTextFileBytes(file)==first);
        write(file,std::string(1,char(0x80)));require(a.snapshot().at("editable")==false);
        rejects([&]{a.save(second,newer.at("revision"));});require(readTextFileBytes(file)==std::string(1,char(0x80)));
        write(file,first);
        rejects([&]{a.save(std::string(262145,' '),a.snapshot().at("revision"));});
        require(readTextFileBytes(file)==first);
        Json full=Json::object();
        for(size_t i=0;i<256;++i)full["Definition "+std::to_string(i)]={{"prefix","p"},{"body","x"}};
        write(file,full.dump());rejects([&]{a.draft("code","cpp");});require(readTextFileBytes(file)==full.dump());
        write(file,first);
    }
    const auto source=root/"one/main.verse";write(source,"");write(root/"two/other.verse","");
    {
        Engine a(root/"one",{},directory),b(root/"two",{},directory),without(root/"one");
        require(without.dispatch("snippets.userRead").at("available")==false);
        const auto doc=a.dispatch("documents.read",{{"path","main.verse"}});
        const auto issued=a.dispatch("snippets.complete",{{"path","main.verse"},{"expectedRevision",doc.at("revision")},
            {"position",{{"line",0},{"character",0}}}});
        const auto &items=issued.at("result").at("items");
        require(items.size()==1 && items[0].at("cycodeSnippetSource").at("kind")=="user");
        const Json params={{"sessionId",issued.at("sessionId")},{"item",0},{"expectedRevision",doc.at("revision")}};
        const auto prepared=a.dispatch("language.completionPrepare",params);
        require(a.dispatch("changes.preview",{{"id",prepared.at("plan").at("id")},{"path","main.verse"}}).at("after")=="hello main");
        const auto current=b.dispatch("snippets.userRead");
        b.dispatch("snippets.userSave",{{"text",second},{"expectedRevision",current.at("revision")}});
        rejects([&]{a.dispatch("language.completionPrepare",params);});
        a.dispatch("changes.apply",{{"id",prepared.at("plan").at("id")}}); // Concrete text plan keeps ordinary guards.
        require(readTextFileBytes(source).empty() && readTextFileBytes(file)==second);
        a.dispatch("changes.undo",{{"id",prepared.at("plan").at("id")}});
        const auto all=mergedSnippetItems(first,Json::array(),"verse",second).at("items");
        require(all.size()==2 && all[0].at("cycodeSnippetSource").at("kind")=="workspace" &&
            all[1].at("cycodeSnippetSource").at("kind")=="user");
    }
    {
        Engine engine(root/"two",{},directory);
        const auto opened=engine.dispatch("documents.read",{{"path","other.verse"}});
        const auto changed=engine.dispatch("documents.replace",{{"path","other.verse"},
            {"expectedRevision",opened.at("revision")},{"text","🛠 $1\\}\n"}});
        const Json selection={{"start",{{"line",0},{"character",0}}},{"end",{{"line",1},{"character",0}}}};
        Json query={{"path","other.verse"},{"expectedRevision",changed.at("revision")},{"selection",selection}};
        const auto draft=engine.dispatch("snippets.userDraft",query);
        require(draft.at("path")=="other.verse" && readTextFileBytes(root/"two/other.verse").empty());
        require(readTextFileBytes(file)==second);
        const auto definitions=Json::parse(draft.at("draft").get<std::string>());
        const auto body=definitions.at(draft.at("name").get<std::string>()).at("body").get<std::string>();
        require(plainSnippet(body)=="🛠 $1\\}\n");
        const auto repeated=engine.dispatch("snippets.userDraft",query);require(repeated.at("name")==draft.at("name"));
        const auto saved=engine.dispatch("snippets.userSave",{{"text",draft.at("draft")},
            {"expectedRevision",draft.at("catalogue").at("revision")}});
        const auto next=engine.dispatch("snippets.userDraft",query);require(next.at("name")!=draft.at("name"));
        require(readTextFileBytes(file)==saved.at("text").get<std::string>());
        query["expectedRevision"]=opened.at("revision");rejects([&]{engine.dispatch("snippets.userDraft",query);});
        query["expectedRevision"]=changed.at("revision");query["selection"]["end"]={{"line",0},{"character",1}};
        rejects([&]{engine.dispatch("snippets.userDraft",query);}); // Split UTF-16 surrogate.
        query["selection"]["end"]=query["selection"]["start"];
        rejects([&]{engine.dispatch("snippets.userDraft",query);});
    }
    fs::remove_all(root);
}
