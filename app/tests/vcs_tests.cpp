#include "cycode/Engine.h"
#include "cycode/GitConflicts.h"
#include <fstream>
#include <iostream>
using namespace cycode;
static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F f, const char *message) { bool caught = false; try { f(); } catch (const std::exception &) { caught = true; } check(caught,message); }
static void write(const std::filesystem::path &path, const std::string &text) { std::ofstream(path,std::ios::binary) << text; }
void gitRepositoryContracts(const std::filesystem::path &);
void gitSubmoduleContracts();
namespace cycode {void gitStructuralConflictContracts(const std::filesystem::path &);}
namespace cycode {void gitMergeContracts(const std::filesystem::path &);}
namespace cycode {void gitReplayContracts(const std::filesystem::path &);}
static void conflictModelContracts() {
    const std::string oid(40,'a'), sha256(64,'b');
    auto record = [](const std::string &mode,const std::string &oid,int stage,const std::string &path) {
        return mode+" "+oid+" "+std::to_string(stage)+"\t"+path+'\0';
    };
    const auto raw=record("100644",oid,1,"old name.cpp")+record("100755",sha256,2,"new, name.cpp");
    const auto parsed=parseGitConflictIndex(raw);
    check(parsed.at("new, name.cpp").at("2").at("oid")==sha256,"NUL records retain spaces/commas and SHA256");
    rejects([&]{parseGitConflictIndex(raw.substr(0,raw.size()-1));},"Incomplete index is rejected");
    rejects([&]{parseGitConflictIndex(raw+raw);},"Duplicate stage is rejected");
    rejects([&]{parseGitConflictIndex(record("100644",oid,0,"f")+record("100644",oid,2,"f"));},"Mixed stages rejected");
    rejects([&]{parseGitConflictIndex(record("100644",std::string(39,'x'),2,"f"));},"Invalid OID rejected");
    for(const auto *path:{"../escape",".git/index","C:/file","a\\b","a/../b","a/","a\nb","a./b"})
        rejects([&]{gitConflictChoices(Json::array({{{"path",path},{"choice","ours"}}}));},"Unsafe path rejected");
    rejects([&]{gitConflictChoices(Json::array({{{"path","A"},{"choice","ours"}},{{"path","a"},{"choice","theirs"}}}));},"Case aliases rejected");
    rejects([&]{gitConflictChoices(Json::array({{{"path","a"},{"choice","ours"}},{{"path","a/b"},{"choice","delete"}}}));},"Directory/file overlap rejected");
    Json entries=Json::array({{{"path","old name.cpp"},{"supported",true},{"base",parsed.at("old name.cpp").at("1")},{"ours",nullptr},{"theirs",nullptr}},
        {{"path","new, name.cpp"},{"supported",true},{"base",nullptr},{"ours",parsed.at("new, name.cpp").at("2")},{"theirs",nullptr}}});
    const auto plan=gitConflictIndexPlan(entries,Json::array({{{"path","old name.cpp"},{"choice","theirs"}},{{"path","new, name.cpp"},{"choice","ours"}}}));
    check(plan.at("changes")[0].at("deleted")==true&&plan.at("changes")[1].at("deleted")==false,"Absent side stages a deletion");
    check(plan.at("arguments")==Json::array({"update-index","--add","--cacheinfo","100755",sha256,"new, name.cpp","--force-remove","--","old name.cpp"}),"One exact argv preserves modes and literal paths");
    entries[1]["supported"]=false;
    rejects([&]{gitConflictIndexPlan(entries,Json::array({{{"path","new, name.cpp"},{"choice","delete"}}}));},"Unsupported conflicts are not silently removed");
}
int main() {
    const auto root = std::filesystem::temp_directory_path() / ("cycode-vcs-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::cout << "VCS stage: conflictModelContracts\n" << std::flush;
        conflictModelContracts();
        std::cout << "VCS stage: gitStructuralConflictContracts\n" << std::flush;
        gitStructuralConflictContracts(root);
        std::cout << "VCS stage: gitMergeContracts\n" << std::flush;
        gitMergeContracts(root);
        std::cout << "VCS stage: gitReplayContracts\n" << std::flush;
        gitReplayContracts(root);
        std::cout << "VCS stage: gitSubmoduleContracts\n" << std::flush;
        gitSubmoduleContracts();
        std::filesystem::create_directories(root/"workspace");
        std::cout << "VCS stage: gitRepositoryContracts\n" << std::flush;
        gitRepositoryContracts(root);
        const auto workspace = root/"workspace";
        auto git = [&](const std::filesystem::path &cwd, std::vector<std::string> args) {
            args.insert(args.begin(),{"git","-C",pathUtf8(cwd),"-c","core.autocrlf=false","-c","commit.gpgsign=false","-c","user.name=CyCode test","-c","user.email=cycode@example.invalid"});
            auto result = runProcess(args,cwd,std::chrono::seconds(15));
            if (result.exitCode) throw std::runtime_error(result.output); return result.output;
        };
        {
            Engine engine(workspace);
            check(!engine.dispatch("vcs.inspect").at("repository"),"Initial repository state");
            auto call = [&](const std::string &method, Json params = {}) {
                if (params.is_null()) params = Json::object();
                auto result = engine.dispatch(method,params);
                if (result.contains("exitCode") && result.at("exitCode") != 0) throw std::runtime_error(method+": "+result.at("output").get<std::string>());
                return result;
            };
            call("vcs.init");
            git(workspace,{"config","user.name","CyCode test"}); git(workspace,{"config","user.email","cycode@example.invalid"}); git(workspace,{"config","commit.gpgsign","false"});
            write(workspace/"file.cpp","base\n"); git(workspace,{"add","."}); git(workspace,{"commit","-m","Initial"});
            auto initial = call("documents.open",{{"path","file.cpp"}});
            call("vcs.branch.create",{{"name","feature"}}); call("vcs.branch.switch",{{"name","feature"}});
            write(workspace/"file.cpp","feature\n"); git(workspace,{"commit","-am","Feature"});
            auto back = call("vcs.branch.switch",{{"name","main"}});
            check(call("documents.read",{{"path","file.cpp"}}).at("text") == "base\n","Clean buffer follows switch");
            call("vcs.branch.switch",{{"name","feature"}});
            auto feature = call("documents.read",{{"path","file.cpp"}});
            check(feature.at("text") == "feature\n" && feature.at("revision") != initial.at("revision"),"Reload has a new revision");
            auto edited = call("documents.replace",{{"path","file.cpp"},{"text","unsaved\n"},{"expectedRevision",feature.at("revision")}});
            rejects([&] { call("vcs.branch.switch",{{"name","main"}}); },"Dirty buffer blocks switch");
            rejects([&] { call("documents.reload",{{"path","file.cpp"},{"expectedRevision",edited.at("revision")}}); },"Reload preserves unsaved edits");
            auto reload = call("documents.reload",{{"path","file.cpp"},{"expectedRevision",edited.at("revision")},{"discard",true}});
            rejects([&] { call("documents.reload",{{"path","file.cpp"},{"expectedRevision",edited.at("revision")}}); },"Reload revision guard");
            write(workspace/"file.cpp","stashed\n");
            rejects([&] { call("vcs.branch.switch",{{"name","main"}}); },"Dirty disk blocks switch");
            call("vcs.stash.save",{{"message","Work in progress"}});
            check(call("vcs.inspect").at("stashes").size() == 1,"Stash metadata");
            call("vcs.stash.pop");
            check(call("documents.read",{{"path","file.cpp"}}).at("text") == "stashed\n","Stash pop reloads clean document");
            call("vcs.stash.save"); call("vcs.stash.drop");
            call("vcs.branch.rename",{{"name","feature"},{"newName","renamed"}});
            check(call("vcs.inspect").at("branch") == "renamed","Rename current branch");
            call("vcs.branch.create",{{"name","disposable"}}); call("vcs.branch.delete",{{"name","disposable"}});
            rejects([&] { call("vcs.branch.create",{{"name","--bad"}}); },"Ref option injection rejected");
            rejects([&] { call("vcs.show",{{"revision","--help"}}); },"Revision option injection rejected");
            git(root,{"init","--bare","remote.git"});
            call("vcs.remote.add",{{"remote","origin"},{"url",pathUtf8(root/"remote.git")}});
            call("vcs.push",{{"remote","origin"},{"branch","renamed"},{"setUpstream",true}});
            check(call("vcs.inspect").at("hasUpstream"),"Push sets tracking");
            git(root,{"clone","--branch","renamed",pathUtf8(root/"remote.git"),"peer"});
            write(root/"peer"/"peer.cpp","peer change\n"); git(root/"peer",{"add","."}); git(root/"peer",{"commit","-m","Peer"}); git(root/"peer",{"push"});
            call("vcs.fetch"); check(call("vcs.inspect").at("behind") == 1,"Fetch reports behind");
            call("vcs.pull",{{"branch","renamed"}}); check(std::filesystem::exists(workspace/"peer.cpp"),"Fast-forward pull");
            call("vcs.remote.set",{{"url",pathUtf8(root/"remote.git")}});
            check(call("vcs.history").at("commits").size() >= 3,"Structured history");
            check(call("vcs.show",{{"revision","HEAD"}}).at("output").get<std::string>().find("Peer") != std::string::npos,"Commit diff");
            check(call("vcs.blame",{{"path","file.cpp"}}).at("output").get<std::string>().find("feature") != std::string::npos,"Blame contents");
            call("vcs.branch.switch",{{"name","main"}}); write(workspace/"file.cpp","main change\n"); git(workspace,{"commit","-am","Main conflict"});
            // Both branches changed the same line. A failing merge must retain all three versions.
            auto merge = engine.dispatch("vcs.merge",{{"revision","renamed"}}); check(merge.at("exitCode") != 0,"Merge produces conflict");
            auto conflict = call("vcs.conflicts",{{"path","file.cpp"}});
            auto catalog=call("vcs.conflict.list");
            auto detail=call("vcs.conflict.inspect",{{"path","file.cpp"},{"expectedRevision",catalog.at("revision")}});
            check(detail.at("versions").at("base").at("text")=="base\n","Catalogue uses the same Git conflict stages as the text editor");
            const auto choices=Json::array({{{"path","file.cpp"},{"choice","ours"}}});
            auto review=call("vcs.conflict.plan",{{"choices",choices},{"expectedRepository",catalog.at("identity")},{"expectedRevision",catalog.at("revision")}});
            rejects([&]{call("vcs.conflict.apply",{{"choices",choices},{"expectedRepository",catalog.at("identity")},{"expectedRevision",catalog.at("revision")},{"expectedPlanRevision","stale"}});},"Reviewed resolution revision is mandatory");
            check(conflict.at("versions").at("base") == "base\n" && conflict.at("versions").at("ours") == "main change\n" && conflict.at("versions").at("theirs") == "feature\n","Three-way conflict contents");
            rejects([&] { call("vcs.resolve",{{"path","file.cpp"},{"expectedRevision",conflict.at("revision")}}); },"Conflict markers not silently staged");
            rejects([&] { call("vcs.resolve",{{"path","file.cpp"},{"expectedRevision",0},{"text","resolved\n"}}); },"Stale conflict resolution rejected");
            call("vcs.resolve",{{"path","file.cpp"},{"expectedRevision",conflict.at("revision")},{"text","resolved\n"}});
            rejects([&]{call("vcs.conflict.apply",{{"choices",choices},{"expectedRepository",catalog.at("identity")},{"expectedRevision",catalog.at("revision")},{"expectedPlanRevision",review.at("revision")}});},"Resolved index invalidates structural preview");
            check(git(workspace,{"ls-files","-u"}).empty(),"Resolution stages file");
            git(workspace,{"commit","-m","Resolve merge"});
            check(call("documents.read",{{"path","file.cpp"}}).at("text") == "resolved\n","Resolution common document model");
            std::filesystem::create_directory(workspace/"nested"); Engine nested(workspace/"nested");
            rejects([&] { nested.dispatch("vcs.branch.switch",{{"name","renamed"}}); },"Nested workspace cannot mutate outside root");
            call("vcs.remote.remove"); check(call("vcs.inspect").at("remotes").empty(),"Remote remove");
        }
        std::filesystem::remove_all(root); std::cout << "Git branches, sync, stashes, history and merge resolution passed\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << "\nFixture: " << pathUtf8(root) << '\n'; return 1; }
}
