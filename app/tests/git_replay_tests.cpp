#include "cycode/Engine.h"
#include "cycode/GitMerge.h"
#include "cycode/GitReplay.h"
#include <fstream>
namespace cycode {
void gitReplayContracts(const std::filesystem::path &root) {
    auto require = [](bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    };
    auto rejects = [&](const std::function<void()> &call) {
        bool rejected = false;
        try {
            call();
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "Expected guarded replay rejection");
    };
    const std::string oid(40, 'a');
    require(gitReplayRequest({{"action", "cherryPick"}, {"commit", oid}}).at("commit") == oid,
            "Exact source object ID");
    for (const auto &bad : {"HEAD", "--abort", "a..b", "abc", ""})
        rejects([&] { gitReplayRequest({{"action", "cherryPick"}, {"commit", bad}}); });
    rejects([&] { gitReplayRequest({{"action", "continue"}, {"commit", oid}}); });
    rejects([&] { gitReplayRequest({{"action", "abort"}}); });
    for (const auto *scenario : {"clean", "continue", "quit"}) {
        const auto folder = root / (std::string("replay-") + scenario);
        std::filesystem::create_directories(folder);
        auto git = [&](std::vector<std::string> args) {
            args.insert(args.begin(), {"git", "-C", pathUtf8(folder), "-c", "core.autocrlf=false", "-c",
                                       "commit.gpgSign=false"});
            const auto result = runProcess(args, folder);
            if (result.exitCode)
                throw std::runtime_error(result.output);
            return result.output;
        };
        auto write = [&](const char *path, const std::string &data) {
            std::ofstream(folder / path, std::ios::binary) << data;
        };
        auto read = [&](const char *path) {
            std::ifstream file(folder / path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        git({"init", "-b", "main"});
        git({"config", "core.autocrlf", "false"});
        git({"config", "user.name", "CyCode fixture"});
        git({"config", "user.email", "fixture@example.invalid"});
        write("file.cpp", "base\n");
        git({"add", "--", "file.cpp"});
        git({"commit", "-m", "Base"});
        git({"checkout", "-b", "incoming"});
        write("file.cpp", "incoming\n");
        git({"commit", "-am", "Incoming"});
        auto source = git({"rev-parse", "HEAD"});
        while (!source.empty() && (source.back() == '\n' || source.back() == '\r'))
            source.pop_back();
        git({"checkout", "main"});
        if (std::string(scenario) != "clean") {
            write("file.cpp", "main\n");
            git({"commit", "-am", "Main"});
        } else {
            write("main.cpp", "main\n");
            git({"add", "--", "main.cpp"});
            git({"commit", "-m", "Main"});
        }
        std::filesystem::create_directories(folder / ".git/hooks");
        write(".git/hooks/pre-commit", "#!/bin/sh\nexit 7\n");
        std::filesystem::permissions(folder / ".git/hooks/pre-commit", std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::add);
        Engine engine(folder);
        auto state = engine.dispatch("vcs.replay.state");
        require(state.at("canStart").get<bool>(), "Clean tracked state permits reviewed replay");
        auto external = state;
        external["kind"] = "other";
        external["otherOperations"] = Json::array({"sequencer", "cherry-pick"});
        require(!gitReplayState(external, "").at("canQuit").get<bool>(),
                "External multi-commit sequence is refused");
        external["otherOperations"] = Json::array({"cherry-pick"});
        external["head"] = "";
        require(!gitReplayState(external, source).at("canQuit").get<bool>(),
                "Malformed unborn pending state is blocked");
        auto request = [&](const char *action, const std::string &commit) {
            return Json{{"action", action},
                        {"commit", commit},
                        {"expectedRepository", state.at("identity")},
                        {"expectedRevision", state.at("revision")}};
        };
        auto params = request("cherryPick", source);
        auto plan = engine.dispatch("vcs.replay.plan", params);
        params["expectedPlanRevision"] = plan.at("revision");
        auto doc = engine.dispatch("documents.read", {{"path", "file.cpp"}});
        rejects([&] { engine.dispatch("vcs.replay.apply", params); });
        engine.dispatch("documents.close", {{"path", "file.cpp"}, {"expectedRevision", doc.at("revision")}});
        state = engine.dispatch("vcs.replay.state");
        params = request("cherryPick", source);
        plan = engine.dispatch("vcs.replay.plan", params);
        params["expectedPlanRevision"] = plan.at("revision");
        const auto result = engine.dispatch("vcs.replay.apply", params);
        require(std::filesystem::exists(folder / utf8Path(result.at("receipt"))), "Persistent replay intent");
        if (std::string(scenario) == "clean") {
            require(result.at("exitCode") == 0 && !result.contains("inspectionError") &&
                        read("file.cpp") == "incoming\n",
                    (std::string("Cherry-pick succeeds with hooks suppressed: ") + result.dump()).c_str());
            state = engine.dispatch("vcs.replay.state");
            params = request("revert", result.at("newCommit"));
            plan = engine.dispatch("vcs.replay.plan", params);
            params["expectedPlanRevision"] = plan.at("revision");
            const auto reverted = engine.dispatch("vcs.replay.apply", params);
            require(reverted.at("exitCode") == 0 && !reverted.contains("inspectionError") &&
                        read("file.cpp") == "base\n",
                    "Revert creates a new reversing commit");
        } else {
            require(result.at("exitCode") != 0 && result.at("state").at("kind") == "cherryPick",
                    "Conflicts retain actual pending operation");
            state = engine.dispatch("vcs.replay.state");
            require(!state.at("canContinue").get<bool>() && state.at("canQuit").get<bool>(),
                    "Unmerged replay can quit but cannot continue");
            if (std::string(scenario) == "continue") {
                write("file.cpp", "resolved\n");
                git({"add", "--", "file.cpp"});
                state = engine.dispatch("vcs.replay.state");
                params = request("continue", "");
                plan = engine.dispatch("vcs.replay.plan", params);
                params["expectedPlanRevision"] = plan.at("revision");
                const auto continued = engine.dispatch("vcs.replay.apply", params);
                require(continued.at("exitCode") == 0 && !continued.contains("inspectionError") &&
                            read("file.cpp") == "resolved\n",
                        "Resolved continuation commits exactly reviewed stage tree");
            } else {
                doc = engine.dispatch("documents.read", {{"path", "file.cpp"}});
                doc = engine.dispatch(
                    "documents.replace",
                    {{"path", "file.cpp"}, {"expectedRevision", doc.at("revision")}, {"text", "unsaved\n"}});
                const auto index = git({"ls-files", "--stage", "-z"}), disk = read("file.cpp");
                state = engine.dispatch("vcs.replay.state");
                params = request("quit", "");
                plan = engine.dispatch("vcs.replay.plan", params);
                params["expectedPlanRevision"] = plan.at("revision");
                const auto quitted = engine.dispatch("vcs.replay.apply", params);
                const auto afterDocument=engine.dispatch("documents.read", {{"path", "file.cpp"}});
                const auto indexUnchanged=git({"ls-files", "--stage", "-z"}) == index;
                const auto diskUnchanged=read("file.cpp") == disk;
                const auto detail=Json{{"result",quitted},{"indexUnchanged",indexUnchanged},
                    {"diskUnchanged",diskUnchanged},{"documentBefore",doc},{"documentAfter",afterDocument}};
                require(quitted.at("exitCode") == 0 && !quitted.contains("inspectionError") &&
                            quitted.at("state").at("kind") == "none" && indexUnchanged && diskUnchanged && afterDocument==doc,
                        ("Quit preserves unresolved index, working file and dirty source buffer: "+detail.dump()).c_str());
            }
        }
    }
}
} // namespace cycode
