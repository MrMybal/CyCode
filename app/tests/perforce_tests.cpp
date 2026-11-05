#include "PerforceClient.h"
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
    require(failed, "Expected Perforce rejection");
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() /
                        utf8Path("cycode-perforce-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected the bundled Perforce plugin");
        const auto root = folder / utf8Path("workspace & été");
        std::filesystem::create_directories(root / "Game");
        const auto project = std::filesystem::canonical(root / "Game");
        const auto file = project / "a@b#c%.cpp";
        std::ofstream(file) << "int original;\n";
        auto line = [](const Json &record) { return record.dump() + "\n"; };
        const auto empty =
            line({{"code", "error"}, {"severity", 2}, {"generic", 17}, {"data", "Aucun résultat"}});
        const auto denied =
            line({{"code", "error"}, {"severity", "3"}, {"generic", "6"}, {"data", "Permission denied"}});
        std::vector<std::vector<std::string>> calls;
        std::vector<int> budgets;
        std::string mode, openedAction = "edit", openedChange = "default";
        bool opened = true, dirty = false, mappingOutside = false, partial = false, extraMapping = false,
             escapedMapping = false;
        perforce::Client client("/installed/p4", [&](const auto &args, const auto &cwd, int seconds) {
            calls.push_back(args);
            budgets.push_back(seconds);
            require(cwd == project && args[1] == "-ztag" && args[2] == "-Mj" && args[3] == "-d",
                    "Perforce lost tagged argv/cwd");
            const auto command = args.size() > 9 ? args[9] : args[5];
            if (command != "info")
                require(args[5] == "-c" && args[6] == "cycode-test" && args[7] == "-u" && args[8] == "tester",
                        "Perforce client/user identity was not pinned");
            ProcessResult result{0, ""};
            if (command == "info") {
                result.output = line(
                    {{"clientName", "cycode-test"}, {"userName", "tester"}, {"clientRoot", pathUtf8(root)}});
                if (mode == "auth")
                    result.output = denied;
            } else if (command == "where") {
                const bool selected = args.back().find(".cpp") != std::string::npos;
                std::string mappedPath = pathUtf8(mappingOutside ? root : selected ? file : project);
                if (escapedMapping) {
                    std::string encoded;
                    for (const auto c : mappedPath)
                        encoded += c == '%' ? "%25" : c == '@' ? "%40" : c == '#' ? "%23" : std::string(1, c);
                    mappedPath = std::move(encoded);
                }
                result.output = line({{"depotFile", "//depot/Game"},
                                      {"clientFile", "//cycode-test/Game"},
                                      {"path", mappedPath}});
                if (extraMapping)
                    result.output += result.output;
            } else if (command == "fstat") {
                result.truncated = partial;
                if (mode == "malformed")
                    result.output = "{\"clientFile\":";
                else if (mode == "statusDenied")
                    result.output = denied;
                else if (!opened) {
                    result.exitCode = 1;
                    result.output = empty;
                } else
                    result.output =
                        line({{"clientFile", pathUtf8(mode == "escape" ? root / "outside.cpp" : file)},
                              {"action", openedAction},
                              {"change", openedChange},
                              {"type", "text"},
                              {"unresolved", mode == "conflict" ? "1" : "0"}});
            } else if (command == "reconcile") {
                require(args[10] == "-n", "Perforce preflight performed a mutation");
                if (dirty)
                    result.output = line({{"clientFile", pathUtf8(file)}, {"action", "edit"}});
                else {
                    result.exitCode = 1;
                    result.output = empty;
                }
            } else if (command == "changes")
                result.output = line({{"change", "42"},
                                      {"user", "tester"},
                                      {"time", "1793743200"},
                                      {"desc", "Résumé\nDetails"}});
            else if (command == "diff") {
                result.output =
                    line({{"depotFile", "//depot/Game/a.cpp"}, {"clientFile", pathUtf8(file)}}) +
                    line({{"code", "text"}, {"data", "@@ -1 +1 @@\n-int original;\n+int edited;\n"}});
            } else if (command == "annotate")
                result.output = line({{"lower", "42"}, {"line", "int original;"}});
            else
                result.output = line({{"code", "info"}, {"data", "Done\n"}});
            return result;
        });
        Json p = {{"workspace", pathUtf8(root)}, {"root", "Game"}};
        auto selected = p;
        selected["path"] = "Game/a@b#c%.cpp";
        auto status = client.run("status", p);
        require(status.at("files").size() == 1 && status.at("files")[0].at("path") == "Game/a@b#c%.cpp" &&
                    status.at("files")[0].at("status") == "M",
                "Perforce status did not map a local opened file");
        auto diff = client.run("diff", selected);
        require(calls.back().back().ends_with("a%40b%23c%25.cpp") &&
                    diff.at("output").get<std::string>().starts_with("--- //depot"),
                "Perforce filespec escaped incorrectly or Diff lost its header");
        client.run("checkout", selected);
        escapedMapping = true;
        client.run("checkout", selected);
        escapedMapping = false;
        require(calls.back()[9] == "edit" && calls.back()[10] == "-c" && calls.back()[11] == "default",
                "Perforce checkout omitted its changelist");
        client.run("add", selected);
        require(calls.back()[10] == "-f" && calls.back().back() == pathUtf8(file),
                "Perforce add did not preserve literal metacharacters");
        auto numbered = selected;
        numbered["changelist"] = "123";
        client.run("reopen", numbered);
        require(calls.back()[9] == "reopen" && calls.back()[11] == "123",
                "Perforce reopen lost its numeric changelist");
        const auto before = calls.size();
        rejects([&] { client.run("revert", selected); });
        require(calls.size() == before, "Perforce revert started before explicit confirmation");
        selected["confirmation"] = "REVERT";
        client.run("revert", selected);
        require(calls.back()[9] == "revert" && calls.back().size() == 11,
                "Perforce revert expanded beyond one file");
        auto submit = selected;
        submit["message"] = "Résumé\n$(literal); --argument";
        client.run("commit", submit);
        require(calls.back()[9] == "submit" && calls.back()[10] == "-Sx" &&
                    calls.back()[12] == submit.at("message").get<std::string>(),
                "Perforce submit changed stream scope or split its description");
        openedChange = "123";
        rejects([&] { client.run("commit", submit); });
        openedChange = "default";
        openedAction = "move/add";
        rejects([&] { client.run("revert", selected); });
        openedAction = "edit";
        mode = "conflict";
        rejects([&] { client.run("commit", submit); });
        mode.clear();
        rejects([&] { client.run("sync", p); });
        opened = false;
        dirty = true;
        rejects([&] { client.run("sync", p); });
        require(calls.back()[9] == "reconcile", "Perforce sync ignored offline modifications");
        dirty = false;
        client.run("sync", p);
        require(calls.back()[9] == "sync" && calls.back().back().ends_with("/Game/...") &&
                    budgets.back() == 180,
                "Perforce sync lost its mapped scope/deadline");
        opened = true;
        require(client.run("log", p).at("commits")[0].at("subject") == "Résumé",
                "Perforce JSON history lost Unicode/multiline messages");
        require(client.run("blame", selected).at("output").get<std::string>().find("42: int original;") !=
                    std::string::npos,
                "Perforce annotations lost their revision/text");
        for (const auto &action : {"lock", "unlock"}) {
            client.run(action, selected);
            require(calls.back()[9] == action, "Perforce lock routing failed");
        }
        auto start = calls.size();
        for (const auto &path :
             {"../outside", "Game/../../outside", "Game/...", "https://example.invalid/a"}) {
            auto bad = selected;
            bad["path"] = path;
            rejects([&] { client.run("checkout", bad); });
        }
        auto invalid = selected;
        invalid["changelist"] = "--other";
        rejects([&] { client.run("reopen", invalid); });
        require(calls.size() == start, "Invalid Perforce paths/options started a process");
        mode = "statusDenied";
        auto failure = client.run("status", p);
        require(failure.at("exitCode") != 0 && failure.at("files").empty(),
                "Perforce permission failure was treated as clean status");
        mode = "auth";
        require(!client.run("status", p).at("repository").get<bool>(),
                "Perforce authenticated a failed connection");
        mode = "malformed";
        rejects([&] { client.run("status", p); });
        mode = "escape";
        rejects([&] { client.run("status", p); });
        mode.clear();
        partial = true;
        failure = client.run("sync", p);
        partial = false;
        require(failure.at("truncated").get<bool>() && failure.at("exitCode") != 0 &&
                    calls.back()[9] == "fstat",
                "Perforce sync used incomplete status");
        mappingOutside = true;
        rejects([&] { client.run("checkout", selected); });
        mappingOutside = false;
        extraMapping = true;
        rejects([&] { client.run("checkout", selected); });
        extraMapping = false;
        perforce::Client missing("", [](const auto &, const auto &, int) -> ProcessResult {
            throw std::runtime_error("Must not spawn");
        });
        require(missing.run("status", p).at("exitCode") == -1,
                "Missing Perforce executable was not reported");
        const auto extensions = folder / "extensions";
        std::filesystem::create_directories(extensions);
        std::filesystem::copy_file(
            argv[1],
            extensions / utf8Path("cycode_perforce" + std::filesystem::path(argv[1]).extension().string()));
        std::ofstream(extensions / "perforce.json")
            << R"({"id":"cycode.perforce","name":"Perforce","version":"1.0.0","abi":2,"library":"cycode_perforce","permissions":["process.execute","workspace.write","vcs.write"]})";
        {
            Engine engine(root);
            engine.plugins().load(extensions);
            const auto providers = engine.dispatch("vcs.providers").at("providers");
            require(providers.size() == 2 && providers[1].at("id") == "cycode.perforce.provider",
                    "Perforce production ABI registration failed");
            engine.dispatch("vcs.configure", {{"root", "Game"}, {"provider", "cycode.perforce.provider"}});
            require(engine.dispatch("vcs.context", {{"contextPath", "Game/a@b#c%.cpp"}}).at("provider") ==
                        "cycode.perforce.provider",
                    "Perforce project mapping failed");
            auto document = engine.dispatch("documents.read", {{"path", "Game/a@b#c%.cpp"}});
            engine.dispatch("documents.replace", {{"path", "Game/a@b#c%.cpp"},
                                                  {"expectedRevision", document.at("revision")},
                                                  {"text", "unsaved\n"}});
            rejects([&] {
                engine.dispatch("vcs.providerAction", {{"contextPath", "Game/a@b#c%.cpp"},
                                                       {"action", "checkout"},
                                                       {"input", {{"path", "Game/a@b#c%.cpp"}}}});
            });
            engine.dispatch("plugins.configure", {{"id", "cycode.perforce"}, {"enabled", false}});
            require(!engine.dispatch("vcs.context", {{"contextPath", "Game/a@b#c%.cpp"}})
                         .at("available")
                         .get<bool>(),
                    "Disabled Perforce provider silently fell back to Git");
        }
        std::filesystem::remove_all(folder);
        std::cout << "Perforce contracts passed; no p4 subprocess invoked.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nFixture retained: " << pathUtf8(folder) << '\n';
        return 1;
    }
}
