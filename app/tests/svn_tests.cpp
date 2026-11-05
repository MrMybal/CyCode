#include "SvnClient.h"
#include "cycode/Engine.h"
#include <fstream>
#include <iostream>

using namespace cycode;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F call) {
    bool failed = false;
    try {
        call();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected SVN input/response rejection");
}
static std::string escape(std::string value) {
    for (const auto &[from, to] : std::vector<std::pair<std::string, std::string>>{
             {"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"}, {"\"", "&quot;"}})
        for (size_t at = 0; (at = value.find(from, at)) != std::string::npos; at += to.size())
            value.replace(at, from.size(), to);
    return value;
}
int main(int argc, char **argv) {
    const auto folder =
        std::filesystem::temp_directory_path() /
        utf8Path("cycode-svn-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected the bundled SVN plugin path");
        const auto root = folder / utf8Path("workspace & été");
        std::filesystem::create_directories(root / "repo");
        std::ofstream(root / "repo/a@b.cpp") << "original\n";
        std::ofstream(root / "repo/new.cpp") << "new\n";
        const auto repository = std::filesystem::canonical(root / "repo");
        auto info = "<info><entry kind=\"dir\" revision=\"42\"><relative-url>^/trunk</relative-url>"
                    "<wc-info><wcroot-abspath>" +
                    escape(pathUtf8(repository)) + "</wcroot-abspath></wc-info></entry></info>";
        std::string status =
            R"(<status><target path="."><entry path="a@b.cpp"><wc-status item="modified" props="none"/></entry><entry path="new.cpp"><wc-status item="unversioned" props="none"/></entry><entry path="props"><wc-status item="normal" props="modified"/></entry><entry path="tree"><wc-status item="normal" props="none" tree-conflicted="true"/></entry></target></status>)";
        std::vector<std::vector<std::string>> calls;
        int seconds = 0;
        bool truncate = false, failedInfo = false;
        svn::Client client("/installed/svn", [&](const auto &args, const auto &cwd, int budget) {
            require(cwd == repository, "SVN cwd escaped its mapped root");
            require(args[1] == "--non-interactive" && args[2] == "--no-auth-cache",
                    "SVN may prompt/cache credentials");
            calls.push_back(args);
            seconds = budget;
            ProcessResult result{0, "done"};
            if (args[3] == "info") {
                result.output = info;
                if (failedInfo) {
                    result.exitCode = 1;
                    result.output = "not a working copy";
                }
            }
            if (args[3] == "status") {
                result.output = status;
                result.truncated = truncate;
            }
            if (args[3] == "log")
                result.output =
                    R"(<log><logentry revision="42"><author>A &amp; B</author><date>2026-11-03</date><msg>First line
Second line</msg></logentry></log>)";
            return result;
        });
        Json parameters = {{"workspace", pathUtf8(root)}, {"root", "repo"}};
        const auto actual = client.run("status", parameters);
        require(actual.at("files").size() == 4 && actual.at("branch") == "^/trunk",
                "SVN status did not parse XML");
        require(actual.at("files")[0].at("path") == "repo/a@b.cpp" &&
                    actual.at("files")[2].at("status") == "M" && actual.at("files")[3].at("status") == "U",
                "SVN lost mapping/property/tree-conflict information");
        auto file = parameters;
        file["path"] = "repo/a@b.cpp";
        client.run("diff", file);
        require(calls.back().back() == "./a@b.cpp@" && calls.back()[4] == "--internal-diff",
                "SVN diff lost literal peg escaping/internal diff");
        client.run("blame", file);
        require(calls.back()[3] == "blame" && seconds == 180, "SVN annotations omitted the network budget");
        auto history = client.run("history", parameters);
        require(history.at("commits")[0].at("author") == "A & B" &&
                    history.at("commits")[0].at("subject") == "First line" &&
                    history.at("output").get<std::string>().find("Second line") != std::string::npos,
                "SVN history lost decoded/multiline data");
        auto start = calls.size();
        rejects([&] { client.run("revert", file); });
        require(calls.size() == start, "SVN revert ran before explicit confirmation");
        file["confirmation"] = "REVERT";
        client.run("revert", file);
        require(calls.back()[3] == "revert" && calls.back()[5] == "empty", "SVN revert became recursive");
        client.run("add", file);
        require(calls.back()[3] == "add" && calls.back()[6] == "--no-auto-props",
                "SVN add lost its explicit file scope");
        rejects([&] { client.run("update", parameters); });
        status =
            R"(<status><target path="."/><changelist name="feature"><entry path="a@b.cpp"><wc-status item="modified" props="none"/></entry></changelist></status>)";
        require(client.run("status", parameters).at("files").size() == 1,
                "SVN omitted changes assigned to a changelist");
        rejects([&] { client.run("update", parameters); });
        status =
            R"(<status><target path="."><entry path="a@b.cpp"><wc-status item="normal" props="none" file-external="true"/></entry></target></status>)";
        require(client.run("status", parameters).at("files")[0].at("fileExternal").get<bool>(),
                "SVN omitted a file external");
        rejects([&] { client.run("revert", file); });
        require(calls.back()[3] == "status", "SVN reverted a file external");
        status =
            R"(<status><target path="."><entry path="new.cpp"><wc-status item="unversioned" props="none"/></entry></target></status>)";
        client.run("update", parameters);
        require(calls.back()[3] == "update" && calls.back()[4] == "--ignore-externals" && seconds == 180,
                "SVN update traverses externals or lacks a bounded deadline");
        auto commit = parameters;
        commit["message"] = "Unicode été\n$(literal); --arguments";
        client.run("commit", commit);
        require(calls.back()[3] == "commit" && calls.back()[10] == commit.at("message").get<std::string>(),
                "SVN commit message was shell-split");
        start = calls.size();
        for (const auto &bad :
             {"../outside", "repo/../../outside", "repo/.svn/wc.db", "https://example.invalid/file"}) {
            auto input = file;
            input["path"] = bad;
            rejects([&] { client.run("diff", input); });
        }
        require(calls.size() == start, "Invalid SVN paths launched a command");
        auto bad = parameters;
        bad["limit"] = 201;
        rejects([&] { client.run("history", bad); });
        bad = parameters;
        bad["staged"] = true;
        rejects([&] { client.run("diff", bad); });
        bad = file;
        bad["path"] = "repo";
        rejects([&] { client.run("revert", bad); });
        std::filesystem::create_directories(root / "repo/nested/.svn");
        bad = file;
        bad["path"] = "repo/nested/a.cpp";
        rejects([&] { client.run("add", bad); });
        truncate = true;
        require(client.run("status", parameters).at("truncated").get<bool>(),
                "SVN reported partial status as complete");
        const auto incomplete = client.run("update", parameters);
        require(incomplete.at("truncated").get<bool>() && incomplete.at("exitCode") != 0 &&
                    calls.back()[3] == "status",
                "SVN update proceeded after an incomplete status response");
        truncate = false;
        status = "<status><target>";
        rejects([&] { client.run("status", parameters); });
        status =
            R"(<status><target><entry path="../outside"><wc-status item="modified" props="none"/></entry></target></status>)";
        rejects([&] { client.run("status", parameters); });
        failedInfo = true;
        require(!client.run("status", parameters).at("repository").get<bool>(),
                "SVN detection failure fabricated a repository");
        const auto missing = svn::Client("", [](const auto &, const auto &, int) -> ProcessResult {
            throw std::runtime_error("must not spawn");
        });
        require(missing.run("status", parameters).at("exitCode") == -1, "Missing SVN tool was not reported");
        // Load the real production plugin to verify the public ABI and provider routing.
        const auto extensions = folder / "extensions";
        std::filesystem::create_directories(extensions);
        std::filesystem::copy_file(
            argv[1],
            extensions / utf8Path("cycode_svn" + std::filesystem::path(argv[1]).extension().string()));
        std::ofstream(extensions / "svn.json")
            << R"({"id":"cycode.svn","name":"SVN","version":"1.0.0","abi":2,"library":"cycode_svn","permissions":["process.execute","workspace.write","vcs.write"]})";
        {
            Engine engine(root);
            engine.plugins().load(extensions);
            const auto providers = engine.dispatch("vcs.providers").at("providers");
            require(providers.size() == 2 && providers[1].at("id") == "cycode.svn.provider",
                    "Production SVN plugin registration failed");
            auto context =
                engine.dispatch("vcs.configure", {{"root", "repo"}, {"provider", "cycode.svn.provider"}});
            require(engine.dispatch("vcs.context", {{"contextPath", "repo/a@b.cpp"}}).at("provider") ==
                        "cycode.svn.provider",
                    "Nested SVN mapping did not route");
            require(engine.dispatch("vcs.context").at("provider") == "git",
                    "SVN mapping replaced the parent Git provider");
            auto document = engine.dispatch("documents.read", {{"path", "repo/a@b.cpp"}});
            std::ofstream(root / "repo/a@b.cpp") << "external change\n";
            rejects([&] {
                engine.dispatch("vcs.providerAction", {{"contextPath", "repo/a@b.cpp"},
                                                       {"action", "add"},
                                                       {"input", {{"path", "repo/a@b.cpp"}}}});
            });
            engine.dispatch("plugins.configure", {{"id", "cycode.svn"}, {"enabled", false}});
            require(!engine.dispatch("vcs.context", {{"contextPath", "repo/a@b.cpp"}})
                         .at("available")
                         .get<bool>(),
                    "Disabling SVN silently fell back to Git");
        }
        std::filesystem::remove_all(folder);
        std::cout << "SVN provider contracts passed; no svn subprocess invoked.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nFixture retained: " << pathUtf8(folder) << '\n';
        return 1;
    }
}
