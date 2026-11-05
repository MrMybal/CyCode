#include "MercurialClient.h"
#include "cycode/Engine.h"
#include <fstream>
#include <iostream>

using namespace cycode;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F callback) {
    bool failed = false;
    try {
        callback();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected Mercurial input/state/response rejection");
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() /
                        utf8Path("cycode-mercurial-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected bundled Mercurial library");
        const auto workspace = folder / utf8Path("workspace & été");
        std::filesystem::create_directories(workspace / "repo/.hg");
        const auto root = std::filesystem::canonical(workspace / "repo");
        std::ofstream(root / "file[1]@.cpp") << "int original;\n";
        std::ofstream(root / "new.cpp") << "new\n";
        const std::string head(40, 'a'), target(40, 'b');
        const Json record = {{"node", head},
                             {"user", "Tester <test@example.invalid>"},
                             {"branch", "dev"},
                             {"date", {1793743200, 0}},
                             {"desc", "Résumé\nDetail"},
                             {"rev", 1}};
        Json parents = Json::array({record}), lookup = Json::array({{{"node", target}}});
        Json status = Json::array(
            {{{"path", "file[1]@.cpp"}, {"status", "M"}}, {{"path", "new.cpp"}, {"status", "?"}}});
        std::vector<std::vector<std::string>> calls;
        std::vector<int> budgets;
        std::string mode;
        int commandCode = 0;
        mercurial::Client client(
            "/installed/hg", [&](const auto &args, const auto &cwd, int seconds, const auto &environment) {
                require(args.size() >= 13 && cwd == root && args[1] == "--cwd" &&
                            args[3] == "--noninteractive" && args[4] == "--pager" && args[5] == "never",
                        "Mercurial lost deterministic argv/cwd");
                require(environment.at("HGPLAIN") == "1" && environment.at("HGPLAINEXCEPT").empty() &&
                            environment.at("HGENCODING") == "utf-8" &&
                            environment.at("HGENCODINGMODE") == "strict",
                        "Mercurial plain/encoding contract was not enforced");
                calls.push_back(args);
                budgets.push_back(seconds);
                const auto command = args[12];
                ProcessResult response{0, "done\n"};
                if (command == "root") {
                    require(args.size() == 15 && args[13] == "--template" && args[14] == "{hgpath|json}",
                            "Mercurial root did not use its documented administrative-path template");
                    response.output = Json(pathUtf8(mode == "rootOutside" ? workspace : root / ".hg")).dump();
                } else if (command == "log") {
                    if (args[13] == "--rev")
                        response.output = (args[14] == "parents()" ? parents : lookup).dump();
                    else
                        response.output = Json::array({record}).dump();
                } else if (command == "status") {
                    response.output = status.dump();
                    if (mode == "malformed")
                        response.output = "[{\"path\":";
                    if (mode == "partial")
                        response.truncated = true;
                    if (mode == "denied") {
                        response.exitCode = 255;
                        response.output = "abort: permission denied\n";
                    }
                } else if (command == "branch" && args.size() == 13)
                    response.output = "branche-é\r\n";
                else if (command == "paths")
                    response.output = mode == "remoteMissing" ? "other\n" : "default\n";
                else if (command == "diff")
                    response.output =
                        "diff --git a/file b/file\n--- a/file\n+++ b/file\n@@ -1 +1 @@\n-old\n+new\n";
                else if (command == "annotate")
                    response.output = "tester aaaaaa 1: int original;\n";
                else if (command == "commit" || command == "push") {
                    response.exitCode = commandCode;
                    response.output = commandCode == 1 ? "nothing changed\n" : "done\n";
                }
                return response;
            });
        Json p = {{"workspace", pathUtf8(workspace)}, {"root", "repo"}}, selected = p;
        selected["path"] = "repo/file[1]@.cpp";
        auto result = client.run("status", p);
        require(result.at("branch") == "branche-é" && result.at("head") == head &&
                    !result.at("merge").get<bool>() &&
                    result.at("files")[0].at("path") == "repo/file[1]@.cpp",
                "Mercurial status lost Unicode/root/parent metadata");
        const auto initialParents = parents;
        parents = Json::array();
        require(client.run("status", p).at("head") == "",
                "An empty Mercurial repository fabricated a parent");
        parents = initialParents;
        client.run("diff", selected);
        require(calls.back()[12] == "diff" && calls.back().back() == "filepath:file[1]@.cpp",
                "Mercurial file diff became a glob");
        client.run("add", selected);
        require(calls.back()[12] == "add" && calls.back()[13] == "--" &&
                    calls.back().back() == "filepath:file[1]@.cpp",
                "Mercurial add lost its exact file selector");
        client.run("forget", selected);
        require(calls.back()[12] == "forget" && std::filesystem::exists(root / "file[1]@.cpp"),
                "Mercurial forget deleted the file");
        auto before = calls.size();
        rejects([&] { client.run("revert", selected); });
        require(calls.size() == before, "Mercurial revert started before explicit confirmation");
        selected["confirmation"] = "REVERT";
        client.run("revert", selected);
        require(calls.back()[12] == "revert" && calls.back()[13] == "--no-backup" &&
                    calls.back().back() == "filepath:file[1]@.cpp",
                "Mercurial revert exceeded one exact file");
        auto commit = p;
        commit["message"] = "Résumé\n$(literal); --amend";
        client.run("commit", commit);
        require(calls.back().size() == 15 && calls.back()[13] == "--message" &&
                    calls.back()[14] == commit.at("message").get<std::string>(),
                "Mercurial commit split the description or changed its mode");
        commandCode = 1;
        result = client.run("commit", commit);
        require(result.at("exitCode") == 0 && result.at("nativeExitCode") == 1 &&
                    result.at("noChanges").get<bool>(),
                "Mercurial no-op commit was misclassified");
        commandCode = 255;
        require(client.run("commit", commit).at("exitCode") == 255, "Mercurial failure became a no-op");
        commandCode = 0;
        require(client.run("history", p).at("commits")[0].at("subject") == "Résumé",
                "Mercurial history lost Unicode/multiline descriptions");
        require(client.run("blame", selected).at("output").get<std::string>().find("int original") !=
                    std::string::npos,
                "Mercurial annotations did not reach Review");
        rejects([&] { client.run("update", p); });
        status = Json::array({{{"path", "new.cpp"}, {"status", "?"}}});
        auto update = p;
        update["revision"] = "dev/feature";
        client.run("update", update);
        require(calls.back()[12] == "update" && calls.back()[13] == "--check" &&
                    calls.back().back() == target,
                "Mercurial update did not pin the verified changeset or lost its clean-tree guard");
        lookup.push_back({{"node", head}});
        rejects([&] { client.run("update", update); });
        lookup.erase(lookup.end() - 1);
        auto bookmark = p;
        bookmark["name"] = "feature-one";
        client.run("bookmark.create", bookmark);
        require(calls.back()[12] == "bookmarks" && calls.back()[13] == "--",
                "Mercurial bookmark routing failed");
        client.run("branch.create", bookmark);
        require(calls.back()[12] == "branch" && calls.back()[13] == "--",
                "Mercurial named-branch routing failed");
        client.run("pull", p);
        require(calls.back()[12] == "pull" && calls.back().size() == 15 && budgets.back() == 180,
                "Mercurial pull updated files or lost its deadline");
        commandCode = 1;
        result = client.run("push", p);
        commandCode = 0;
        require(result.at("noChanges").get<bool>() && calls.back()[12] == "push" && calls.back().size() == 15,
                "Mercurial push used force or mishandled no changes");
        mode = "remoteMissing";
        rejects([&] { client.run("pull", p); });
        mode.clear();
        parents.push_back(record);
        require(client.run("status", p).at("merge").get<bool>(), "Mercurial merge state was hidden");
        rejects([&] { client.run("commit", commit); });
        rejects([&] { client.run("revert", selected); });
        parents.erase(parents.end() - 1);
        before = calls.size();
        for (const auto &badPath : {"../outside", "repo/../../outside", "repo/.hg/hgrc", "glob:*",
                                    "https://example.invalid/file"}) {
            auto input = selected;
            input["path"] = badPath;
            rejects([&] { client.run("add", input); });
        }
        auto bad = update;
        bad["revision"] = "all()";
        rejects([&] { client.run("update", bad); });
        bad = p;
        bad["remote"] = "https://example.invalid/repo";
        rejects([&] { client.run("pull", bad); });
        bad = p;
        bad["staged"] = true;
        rejects([&] { client.run("diff", bad); });
        bad = p;
        bad["limit"] = 201;
        rejects([&] { client.run("history", bad); });
        require(calls.size() == before, "Invalid Mercurial paths/revsets/options launched a command");
        std::filesystem::create_directories(root / "nested/.hg");
        bad = selected;
        bad["path"] = "repo/nested/new.cpp";
        rejects([&] { client.run("forget", bad); });
        for (const auto &state : {root / ".hg/sharedpath", root / ".hg/graftstate", root / ".hgsub"}) {
            std::ofstream(state) << "state\n";
            before = calls.size();
            rejects([&] { client.run("commit", commit); });
            require(calls.size() == before, "Unsupported shared/subrepo/unfinished state ran a mutation");
            std::filesystem::remove(state);
        }
        mode = "partial";
        result = client.run("update", update);
        require(result.at("truncated").get<bool>() && result.at("exitCode") != 0 &&
                    calls.back()[12] == "status",
                "Mercurial update used partial status");
        mode = "denied";
        require(client.run("status", p).at("exitCode") == 255, "Mercurial denied status became a clean tree");
        mode = "malformed";
        rejects([&] { client.run("status", p); });
        mode = "rootOutside";
        rejects([&] { client.run("add", selected); });
        mode.clear();
        status = Json::array({{{"path", "../outside"}, {"status", "M"}}});
        rejects([&] { client.run("status", p); });
        mercurial::Client missing("", [](const auto &, const auto &, int, const auto &) -> ProcessResult {
            throw std::runtime_error("Must not spawn");
        });
        require(missing.run("status", p).at("exitCode") == -1,
                "Mercurial missing executable was not reported");
        const auto extensions = folder / "extensions";
        std::filesystem::create_directories(extensions);
        std::filesystem::copy_file(
            argv[1],
            extensions / utf8Path("cycode_mercurial" + std::filesystem::path(argv[1]).extension().string()));
        std::ofstream(extensions / "mercurial.json")
            << R"({"id":"cycode.mercurial","name":"Mercurial","version":"1.0.0","abi":2,"library":"cycode_mercurial","permissions":["process.execute","workspace.write","vcs.write"]})";
        {
            Engine engine(workspace);
            engine.plugins().load(extensions);
            const auto providers = engine.dispatch("vcs.providers").at("providers");
            require(providers.size() == 2 && providers[1].at("id") == "cycode.mercurial.provider",
                    "Mercurial production plugin registration failed");
            engine.dispatch("vcs.configure", {{"root", "repo"}, {"provider", "cycode.mercurial.provider"}});
            require(engine.dispatch("vcs.context", {{"contextPath", "repo/file[1]@.cpp"}}).at("provider") ==
                        "cycode.mercurial.provider",
                    "Mercurial mapping did not route");
            auto document = engine.dispatch("documents.read", {{"path", "repo/file[1]@.cpp"}});
            engine.dispatch("documents.replace", {{"path", "repo/file[1]@.cpp"},
                                                  {"expectedRevision", document.at("revision")},
                                                  {"text", "unsaved\n"}});
            rejects([&] {
                engine.dispatch("vcs.providerAction", {{"contextPath", "repo/file[1]@.cpp"},
                                                       {"action", "add"},
                                                       {"input", {{"path", "repo/file[1]@.cpp"}}}});
            });
            engine.dispatch("plugins.configure", {{"id", "cycode.mercurial"}, {"enabled", false}});
            require(!engine.dispatch("vcs.context", {{"contextPath", "repo/file[1]@.cpp"}})
                         .at("available")
                         .get<bool>(),
                    "Mercurial disable silently fell back to Git");
        }
        std::filesystem::remove_all(folder);
        std::cout << "Mercurial contracts passed; no hg subprocess invoked.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nFixture retained: " << pathUtf8(folder) << '\n';
        return 1;
    }
}
