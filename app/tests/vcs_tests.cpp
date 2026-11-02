#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
using namespace cycode;
static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F f, const char *message) { bool caught = false; try { f(); } catch (const std::exception &) { caught = true; } check(caught,message); }
static void write(const std::filesystem::path &path, const std::string &text) { std::ofstream(path,std::ios::binary) << text; }
int main() {
    const auto root = std::filesystem::temp_directory_path() / ("cycode-vcs-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(root/"workspace");
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
            check(conflict.at("versions").at("base") == "base\n" && conflict.at("versions").at("ours") == "main change\n" && conflict.at("versions").at("theirs") == "feature\n","Three-way conflict contents");
            rejects([&] { call("vcs.resolve",{{"path","file.cpp"},{"expectedRevision",conflict.at("revision")}}); },"Conflict markers not silently staged");
            rejects([&] { call("vcs.resolve",{{"path","file.cpp"},{"expectedRevision",0},{"text","resolved\n"}}); },"Stale conflict resolution rejected");
            call("vcs.resolve",{{"path","file.cpp"},{"expectedRevision",conflict.at("revision")},{"text","resolved\n"}});
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
