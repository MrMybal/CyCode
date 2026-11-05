#include "DiversionClient.h"
#include "cycode/Engine.h"
#include <algorithm>
#include <fstream>
#include <iostream>

using namespace cycode;
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F callback) {
    bool failed = false;
    try {
        callback();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected Diversion input/state/metadata rejection");
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() /
                        utf8Path("cycode-diversion-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected bundled Diversion library");
        const auto workspace = folder / utf8Path("workspace & été");
        std::filesystem::create_directories(workspace / "repo/.diversion");
        std::filesystem::create_directories(workspace / "repo/src");
        const auto root = std::filesystem::canonical(workspace / "repo");
        const auto fileName = std::string("src/file @ été.cpp");
        std::ofstream(root / utf8Path(fileName)) << "int original;\n";
        std::string summary = "M\t" + fileName + "\nA\tnew.cpp\n";
        const auto statusText =
            std::string("In repo game dv.repo.01234567-1234-1234-1234-0123456789ab\n"
                        "On branch feature/été dv.branch.7\n"
                        "Cloud workspace is over commit dv.commit.42\n"
                        "Working in workspace Tester @ Machine (dv.ws.01234567-4321-4321-4321-0123456789ab)\n"
                        "Local workspace is synced to commit: dv.commit.42\n");
        std::string status = statusText, mode;
        int code = 0, statusCount = 0;
        std::vector<std::vector<std::string>> calls;
        std::vector<int> budgets;
        diversion::Client client("/installed/dv", [&](const auto &args, const auto &cwd, int seconds,
                                                      const auto &env) {
            require(args.size() >= 2 && cwd == root && args[0] == "/installed/dv",
                    "Diversion lost argv/cwd ownership");
            require(env.at("NO_COLOR") == "1" && env.at("TERM") == "dumb",
                    "Diversion lost plain-output hints");
            calls.push_back(args);
            budgets.push_back(seconds);
            ProcessResult result{0, "done\n"};
            if (args[1] == "status") {
                require(args ==
                            std::vector<std::string>({"/installed/dv", "status", "--nowait", "--no-limit"}),
                        "Diversion status can wait indefinitely or silently limit changes");
                result.output = status;
                if (mode == "race" && ++statusCount == 2)
                    result.output.replace(result.output.find("dv.branch.7"), 11, "dv.branch.8");
                if (mode == "denied") {
                    result.exitCode = 2;
                    result.output = "Login required\n";
                }
                if (mode == "partial")
                    result.truncated = true;
                if (mode == "timeout")
                    result.timedOut = true;
                if (mode == "cancelled")
                    result.cancelled = true;
            } else if (args[1] == "diff" && args.size() == 5 && args[2] == "--name-status") {
                result.output = summary;
                if (mode == "summaryPartial")
                    result.truncated = true;
            } else {
                result.exitCode = code;
                if (args[1] == "diff")
                    result.output = "--- a/src/main.cpp\n+++ b/src/main.cpp\n@@ -1 +1 @@\n-old\n+new\n";
                if (args[1] == "log")
                    result.output = "dv.commit.42 | Tester | Résumé\n";
                if (args[1] == "annotate")
                    result.output = "dv.commit.42 Tester 2026-11-03 1 int original;\n";
            }
            return result;
        });
        Json p = {{"workspace", pathUtf8(workspace)}, {"root", "repo"}}, selected = p;
        selected["path"] = "repo/" + fileName;
        const auto result = client.run("status", p);
        require(result.at("repository") && result.at("branch") == "feature/été" && result.at("syncReady") &&
                    result.at("head") == "dv.commit.42" &&
                    result.at("files")[0].at("path") == "repo/" + fileName,
                "Diversion lost identity, Unicode, sync or mapped change paths");
        status = "\x1b[32m" + statusText + "\x1b[0m";
        require(client.run("inspect", p).at("syncReady"), "Diversion color escapes broke status");
        status = statusText;
        summary = "R\told.cpp\tnew.cpp\n";
        require(client.run("status", p).at("files").size() == 2,
                "Diversion rename did not retain both paths");
        summary = "M\t" + fileName + "\n";
        require(client.run("diff", selected).at("output").get<std::string>().starts_with("--- a/"),
                "Diversion diff lost Review output");
        require(calls.back() ==
                    std::vector<std::string>({"/installed/dv", "diff", "--color", "never", "./" + fileName}),
                "Diversion diff expanded a shell or option operand");
        require(client.run("log", p).at("output").get<std::string>().find("Résumé") != std::string::npos,
                "Diversion history did not reach read-only Review");
        require(calls.back() ==
                    std::vector<std::string>({"/installed/dv", "log", "-n", "50", "--date", "iso"}),
                "Diversion history became unbounded");
        client.run("blame", selected);
        require(calls.back() == std::vector<std::string>({"/installed/dv", "annotate", "./" + fileName}),
                "Diversion annotations exceeded the selected file");
        auto before = calls.size();
        rejects([&] { client.run("revert", selected); });
        require(calls.size() == before, "Diversion reset started before confirmation");
        auto revert = selected;
        revert["confirmation"] = "REVERT";
        client.run("revert", revert);
        require(calls.back() == std::vector<std::string>({"/installed/dv", "reset", "./" + fileName, "-f"}) &&
                    budgets.back() == 180 && std::filesystem::exists(root / utf8Path(fileName)),
                "Diversion reset used clean/all/nowait or lost its deadline");
        auto commit = selected;
        commit["message"] = "Résumé & $(literal)\nDetail";
        client.run("commit", commit);
        require(calls.back() == std::vector<std::string>(
                                    {"/installed/dv", "commit", "-m", commit.at("message"), "./" + fileName}),
                "Diversion file commit split the message or committed all files");
        commit.erase("path");
        client.run("commit", commit);
        require(calls.back().back() == "-a" && calls.back().size() == 5,
                "Diversion repository commit lost its explicit scope");
        code = 1;
        require(client.run("commit", commit).at("exitCode") == 1,
                "Undocumented Diversion failure was normalized into success");
        code = 0;
        auto branch = p;
        branch["name"] = "feature/code";
        client.run("branch.create", branch);
        require(calls.back() == std::vector<std::string>(
                                    {"/installed/dv", "branch", "-c", "feature/code", "--no-checkout"}),
                "Diversion branch creation changed the workspace branch");
        auto checkout = p;
        checkout["revision"] = "dv.commit.12";
        rejects([&] { client.run("branch.switch", checkout); });
        rejects([&] { client.run("update", p); });
        summary.clear();
        client.run("branch.switch", checkout);
        require(calls.back() ==
                    std::vector<std::string>({"/installed/dv", "checkout", "dv.commit.12", "--ignore-shelf"}),
                "Diversion checkout discarded/carried changes or applied a shelf");
        client.run("update", p);
        require(calls.back() ==
                    std::vector<std::string>({"/installed/dv", "update", "--conflict_resolution", "manual"}),
                "Diversion update chose automatic destructive resolution");
        client.run("lock", selected);
        require(calls.back() == std::vector<std::string>({"/installed/dv", "lock", "./" + fileName}),
                "Diversion lock exceeded the selected path");
        client.run("unlock", selected);
        require(calls.back() == std::vector<std::string>({"/installed/dv", "lock", "-d", "./" + fileName}),
                "Diversion unlock used force/admin semantics");
        for (const auto &line :
             {"Workspace sync is paused.\n", "Sync in progress\n", "Last error: transfer failed\n",
              "Queued filesystem events: 3\n", "Unresolved conflicts\n"}) {
            status = statusText + line;
            require(!client.run("status", p).at("syncReady").get<bool>(),
                    "Diversion incomplete/conflicting state became ready");
            rejects([&] { client.run("commit", commit); });
        }
        status = statusText;
        auto at = status.find("Local workspace is synced to commit: dv.commit.42");
        status.replace(at, status.size() - at, "Local workspace is syncing to commit: dv.commit.43\n");
        require(!client.run("status", p).at("syncReady").get<bool>(),
                "Diversion local/cloud mismatch was hidden");
        rejects([&] { client.run("update", p); });
        status = statusText;
        mode = "race";
        statusCount = 0;
        rejects([&] { client.run("commit", commit); });
        mode.clear();
        for (const auto &bad : {std::string("not a status\n"), statusText + "On branch other dv.branch.9\n",
                                statusText + "\x1b[2J", statusText + std::string("\0", 1)}) {
            status = bad;
            rejects([&] { client.run("status", p); });
        }
        status = statusText;
        for (const auto &bad :
             {"M\t../outside.cpp\n", "M\t.diversion/config\n", "M\t\"escaped.cpp\"\n", "M\t leading.cpp\n",
              "unexpected output\n", "R\told -> new\n", "M\tx\nM\tx\n"}) {
            summary = bad;
            rejects([&] { client.run("status", p); });
        }
        summary.clear();
        for (const auto &failure : {"partial", "timeout", "cancelled", "summaryPartial"}) {
            mode = failure;
            require(client.run("status", p).at("exitCode") == 124,
                    "Partial Diversion response became a clean working copy");
        }
        mode = "denied";
        require(client.run("status", p).at("exitCode") == 2, "Diversion authentication error was hidden");
        mode.clear();
        before = calls.size();
        for (const auto &path :
             {"../outside", "repo/.diversion/config", "repo/*.cpp", "repo/src/[a].cpp", "repo/C:stream"}) {
            auto bad = selected;
            bad["path"] = path;
            rejects([&] { client.run("lock", bad); });
        }
        for (const auto &action : {"add", "stage", "push", "pull", "merge", "login"})
            rejects([&] { client.run(action, p); });
        auto bad = p;
        bad["name"] = "--force";
        rejects([&] { client.run("branch.create", bad); });
        bad = p;
        bad["revision"] = "https://other";
        rejects([&] { client.run("branch.switch", bad); });
        bad = p;
        bad["limit"] = 201;
        rejects([&] { client.run("log", bad); });
        bad = p;
        bad["staged"] = true;
        rejects([&] { client.run("diff", bad); });
        require(calls.size() == before, "Invalid Diversion action/path/options launched a command");
        std::filesystem::create_directories(root / "nested/.diversion");
        bad = selected;
        bad["path"] = "repo/nested/file.cpp";
        rejects([&] { client.run("lock", bad); });
        diversion::Client missing("", [&](const auto &, const auto &, int, const auto &) -> ProcessResult {
            throw std::runtime_error("Must not spawn");
        });
        require(missing.run("status", p).at("exitCode") == -1, "Missing Diversion client was not reported");
        const auto descriptor = diversion::descriptor();
        require(descriptor.at("actions").size() == 10 &&
                    std::find(descriptor.at("capabilities").begin(), descriptor.at("capabilities").end(),
                              "push") == descriptor.at("capabilities").end(),
                "Diversion advertised Git-only operations or lost actions");
        const auto extensions = folder / "extensions";
        std::filesystem::create_directories(extensions);
        std::filesystem::copy_file(
            argv[1],
            extensions / utf8Path("cycode_diversion" + std::filesystem::path(argv[1]).extension().string()));
        std::ofstream(extensions / "diversion.json")
            << R"({"id":"cycode.diversion","name":"Diversion","version":"1.0.0","abi":2,"library":"cycode_diversion","permissions":["process.execute","workspace.write","vcs.write"]})";
        {
            Engine engine(workspace);
            engine.plugins().load(extensions);
            const auto providers = engine.dispatch("vcs.providers").at("providers");
            require(providers.size() == 2 && providers[1].at("id") == "cycode.diversion.provider",
                    "Diversion production ABI registration failed");
            engine.dispatch("vcs.configure", {{"root", "repo"}, {"provider", "cycode.diversion.provider"}});
            require(engine.dispatch("vcs.context", {{"contextPath", "repo/" + fileName}}).at("provider") ==
                        "cycode.diversion.provider",
                    "Diversion mapping failed");
            auto document = engine.dispatch("documents.read", {{"path", "repo/" + fileName}});
            engine.dispatch("documents.replace", {{"path", "repo/" + fileName},
                                                  {"expectedRevision", document.at("revision")},
                                                  {"text", "unsaved\n"}});
            rejects([&] {
                engine.dispatch("vcs.providerAction", {{"contextPath", "repo/" + fileName},
                                                       {"action", "lock"},
                                                       {"input", {{"path", "repo/" + fileName}}}});
            });
            engine.dispatch("plugins.configure", {{"id", "cycode.diversion"}, {"enabled", false}});
            require(!engine.dispatch("vcs.context", {{"contextPath", "repo/" + fileName}})
                         .at("available")
                         .get<bool>(),
                    "Disabled Diversion silently fell back to Git");
        }
        std::filesystem::remove_all(folder);
        std::cout << "Diversion contracts passed; no dv subprocess invoked.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nFixture retained: " << pathUtf8(folder) << '\n';
        return 1;
    }
}
