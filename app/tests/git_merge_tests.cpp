#include "cycode/Engine.h"
#include "cycode/GitMerge.h"
#include <fstream>
namespace cycode {
void gitMergeContracts(const std::filesystem::path &root) {
    auto require = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    auto rejects = [&](const std::function<void()> &action) {
        bool rejected = false;
        try {
            action();
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "Expected guarded merge rejection");
    };
    const std::string sha1(40, 'a'), sha256(64, 'b');
    require(parseGitMergeHeads(sha1 + "\r\n") == Json::array({sha1}), "CRLF merge parent metadata");
    require(parseGitMergeHeads(sha256 + "\n") == Json::array({sha256}), "SHA256 merge parents");
    rejects([&] { parseGitMergeHeads(sha1 + "\n" + sha1 + "\n"); });
    rejects([&] { parseGitMergeHeads("--bad\n"); });
    const auto rawCommit = "tree " + sha1 + "\nparent " + sha1 + "\nparent " + sha256 +
                           "\nauthor Fixture <fixture@example.invalid> 0 +0000\n" +
                           "committer Fixture <fixture@example.invalid> 0 +0000\n\nparent ignored\n";
    require(parseGitMergeCommitParents(rawCommit) == Json::array({sha1, sha256}),
            "Raw parents exclude message body and ignore pretty-log configuration");
    rejects([&] { parseGitMergeCommitParents("tree --bad\n\nmessage"); });
    rejects([&] { parseGitMergeCommitParents("tree " + sha1); });
    rejects([&] { gitMergeRequest({{"action", "commit"}, {"message", " \r\n\t"}}); });
    rejects([&] { gitMergeRequest({{"action", "quit"}, {"message", "discard"}}); });
    rejects([&] { gitMergeRequest({{"action", "quit"}, {"runHooks", true}}); });
    require(!gitMergeRequest({{"action", "commit"}, {"message", "Resolve"}}).at("runHooks").get<bool>(),
            "Hooks are opt-in");
    require(gitMergeRequest({{"action", "commit"}, {"message", std::string(16384, 'x')}})
                    .at("message")
                    .get<std::string>()
                    .size() == 16384,
            "Exact 16 KiB merge message is accepted");
    rejects([&] { gitMergeRequest({{"action", "commit"}, {"message", std::string(16385, 'x')}}); });
    const std::string tree = "100644 blob " + sha1 + "\tname, été.cpp" + '\0';
    require(parseGitMergeTree(tree).at("name, été.cpp").at("0").at("oid") == sha1,
            "Committed tree retains literal paths");
    rejects([&] { parseGitMergeTree(tree.substr(0, tree.size() - 1)); });
    const std::string changed = "M" + std::string(1, '\0') + "name, été.cpp" + '\0';
    require(parseGitMergeChanges(changed)[0].at("path") == "name, été.cpp",
            "Staged NUL records preserve filenames");
    rejects([&] { parseGitMergeChanges(changed + changed); });
    for (const auto *scenario : {"quit", "complete"}) {
        const auto folder = root / (std::string("merge-") + scenario);
        std::filesystem::create_directories(folder);
        auto git = [&](std::vector<std::string> args, bool success = true) {
            args.insert(args.begin(), {"git", "-C", pathUtf8(folder), "-c", "core.autocrlf=false", "-c",
                                       "commit.gpgsign=false"});
            const auto r = runProcess(args, folder);
            if (success && r.exitCode)
                throw std::runtime_error(r.output);
            return r;
        };
        auto write = [&](const char *name, const std::string &data) {
            std::ofstream(folder / name, std::ios::binary) << data;
        };
        auto read = [&](const char *name) {
            std::ifstream file(folder / name, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        git({"init", "-b", "main"});
        git({"config", "user.name", "CyCode fixture"});
        git({"config", "user.email", "fixture@example.invalid"});
        write("file.cpp", "base\n");
        git({"add", "--", "."});
        git({"commit", "-m", "Base"});
        git({"checkout", "-b", "incoming"});
        write("file.cpp", "incoming\n");
        git({"commit", "-am", "Incoming"});
        git({"checkout", "main"});
        if (std::string(scenario) == "quit") {
            write("file.cpp", "main\n");
            git({"commit", "-am", "Main"});
        } else {
            write("main.cpp", "main\n");
            git({"add", "--", "main.cpp"});
            git({"commit", "-m", "Main"});
        }
        const auto merge = git({"merge", "--no-commit", "--no-ff", "incoming"}, false);
        require((merge.exitCode != 0) == (std::string(scenario) == "quit"), "Fixture merge outcome");
        Engine engine(folder);
        auto state = engine.dispatch("vcs.merge.state");
        require(state.at("kind") == "merge", "Actual merge state");
        auto request = [&](const std::string &action, const std::string &message, bool hooks = false) {
            return Json{{"action", action},
                        {"message", message},
                        {"runHooks", hooks},
                        {"expectedRepository", state.at("identity")},
                        {"expectedRevision", state.at("revision")}};
        };
        if (std::string(scenario) == "quit") {
            require(!state.at("canCommit").get<bool>() && state.at("canQuit").get<bool>() &&
                        state.at("conflicts").size() == 1,
                    "Unresolved merge can quit but cannot commit");
            auto doc = engine.dispatch("documents.read", {{"path", "file.cpp"}});
            doc = engine.dispatch("documents.replace", {{"path", "file.cpp"},
                                                        {"expectedRevision", doc.at("revision")},
                                                        {"text", "unsaved source\n"}});
            state = engine.dispatch("vcs.merge.state");
            const auto index = git({"ls-files", "--stage", "-z"}).output, disk = read("file.cpp");
            auto params = request("quit", "");
            auto plan = engine.dispatch("vcs.merge.plan", params);
            params["expectedPlanRevision"] = plan.at("revision");
            {
                std::ofstream message(folder / ".git/MERGE_MSG", std::ios::app);
                message << "\nChanged review\n";
            }
            rejects([&] { engine.dispatch("vcs.merge.quit", params); });
            state = engine.dispatch("vcs.merge.state");
            params = request("quit", "");
            plan = engine.dispatch("vcs.merge.plan", params);
            params["expectedPlanRevision"] = plan.at("revision");
            const auto result = engine.dispatch("vcs.merge.quit", params);
            require(result.at("exitCode") == 0 && !result.contains("inspectionError") &&
                        result.at("state").at("kind") == "none",
                    "Quit removes actual merge bookkeeping");
            require(git({"ls-files", "--stage", "-z"}).output == index && read("file.cpp") == disk &&
                        engine.dispatch("documents.read", {{"path", "file.cpp"}}) == doc,
                    "Quit preserves unresolved index, disk and dirty document");
            require(std::filesystem::exists(folder / utf8Path(result.at("receipt"))),
                    "Intent receipt persists");
        } else {
            require(state.at("canCommit").get<bool>(), "Resolved synchronized merge can commit");
            write("intent.cpp", "not staged yet\n");
            git({"add", "-N", "--", "intent.cpp"});
            const auto intent = engine.dispatch("vcs.merge.state");
            require(intent.at("hiddenIndexFlags").get<bool>() && !intent.at("canCommit").get<bool>(),
                    "Intent-to-add cannot silently disappear from a reviewed merge commit");
            git({"add", "--", "intent.cpp"});
            const auto metadata = engine.dispatch("documents.read", {{"path", ".git/MERGE_MSG"}});
            const auto blocked = engine.dispatch("vcs.merge.state");
            require(!blocked.at("canCommit").get<bool>() && !blocked.at("canQuit").get<bool>(),
                    "Open Git metadata blocks both mutations");
            engine.dispatch("documents.close",
                            {{"path", ".git/MERGE_MSG"}, {"expectedRevision", metadata.at("revision")}});
            state = engine.dispatch("vcs.merge.state");
            auto params = request("commit", "Reviewed merge\n\nDetails\n");
            auto plan = engine.dispatch("vcs.merge.plan", params);
            params["expectedPlanRevision"] = plan.at("revision");
            write("extra.cpp", "staged later\n");
            git({"add", "--", "extra.cpp"});
            rejects([&] { engine.dispatch("vcs.merge.commit", params); });
            std::filesystem::create_directories(folder / ".git/hooks");
            write(".git/hooks/pre-commit", "#!/bin/sh\nexit 7\n");
            std::filesystem::permissions(folder / ".git/hooks/pre-commit", std::filesystem::perms::owner_exec,
                                         std::filesystem::perm_options::add);
            state = engine.dispatch("vcs.merge.state");
            params = request("commit", "Reviewed merge\n\nDetails\n", true);
            plan = engine.dispatch("vcs.merge.plan", params);
            params["expectedPlanRevision"] = plan.at("revision");
            const auto failed = engine.dispatch("vcs.merge.commit", params);
            require(failed.at("exitCode") != 0 && failed.at("state").at("kind") == "merge",
                    "Enabled failing hook retains actual failed merge result");
            state = engine.dispatch("vcs.merge.state");
            git({"config", "log.showSignature", "true"});
            params = request("commit", std::string(16384, 'x'));
            plan = engine.dispatch("vcs.merge.plan", params);
            params["expectedPlanRevision"] = plan.at("revision");
            const auto result = engine.dispatch("vcs.merge.commit", params);
            require(result.at("exitCode") == 0 && !result.contains("inspectionError") &&
                        result.at("state").at("kind") == "none" &&
                        !result.at("newCommit").get<std::string>().empty(),
                    "Default-disabled hook commit verifies real parents/tree/index");
            require(parseGitMergeCommitParents(git({"cat-file", "commit", "HEAD"}).output) ==
                        plan.at("expectedParents"),
                    "Exactly the reviewed merge parents");
        }
    }
}
} // namespace cycode
