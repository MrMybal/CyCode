#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
static void check(bool v, const std::string &message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F f, const char *message) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
int main() {
    const auto root =
        fs::temp_directory_path() /
        ("cycode-hunks-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    bool success = false;
    try {
        auto git = [&](std::vector<std::string> args) {
            args.insert(args.begin(), {"git", "-C", pathUtf8(root)});
            auto r = runProcess(args, root);
            check(r.exitCode == 0, r.output);
            return r.output;
        };
        const std::string path = "space [file].cpp";
        auto write = [&](const std::string &text) {
            std::ofstream out(root / path, std::ios::binary);
            out << text;
        };
        git({"init", "--initial-branch=main"});
        git({"config", "user.name", "Fixture"});
        git({"config", "user.email", "fixture@example.invalid"});
        git({"config", "core.autocrlf", "false"});
        std::string before;
        for (int i = 1; i <= 40; ++i)
            before += "line " + std::to_string(i) + "\r\n";
        write(before);
        git({"add", "--", path});
        git({"commit", "-m", "Fixture"});
        auto after = before;
        after.replace(after.find("line 3\r"), 6, "first edit");
        after.replace(after.find("line 35\r"), 7, "second edit");
        write(after);
        {
            Engine engine(root);
            engine.configureLanguage("", false, false);
            auto plan = engine.dispatch("vcs.hunks", {{"path", path}});
            check(plan.at("hunks").size() == 2, "Expected separate hunks: " + plan.dump());
            std::ofstream(root / "other.txt", std::ios::binary) << "another file\n";
            git({"add", "--", "other.txt"});
            rejects([&] { engine.dispatch("vcs.applyHunks", {{"id", plan.at("id")}, {"hunks", {0, 0}}}); },
                    "Duplicate indices accepted");
            check(engine.dispatch("vcs.applyHunks", {{"id", plan.at("id")}, {"hunks", {0}}}).at("exitCode") ==
                      0,
                  "Partial staging failed");
            const auto staged = git({"show", ":" + path});
            check(staged.find("first edit") != std::string::npos &&
                      staged.find("second edit") == std::string::npos,
                  "Staged more than selected hunk");
            std::ifstream disk(root / path, std::ios::binary);
            check(std::string(std::istreambuf_iterator<char>(disk), {}) == after, "Working file was changed");
            rejects([&] { engine.dispatch("vcs.applyHunks", {{"id", plan.at("id")}, {"hunks", {1}}}); },
                    "Consumed plan accepted");
            auto reverse = engine.dispatch("vcs.hunks", {{"path", path}, {"staged", true}});
            engine.dispatch("vcs.applyHunks", {{"id", reverse.at("id")}, {"hunks", {0}}});
            check(git({"diff", "--cached", "--", path}).empty(), "Partial unstaging failed");
            check(git({"show", ":other.txt"}) == "another file\n",
                  "Hunk staging damaged unrelated index entries");
            auto stale = engine.dispatch("vcs.hunks", {{"path", path}});
            write(after + "outside edit\r\n");
            rejects([&] { engine.dispatch("vcs.applyHunks", {{"id", stale.at("id")}, {"hunks", {0}}}); },
                    "Changed disk accepted");
            write(after);
            auto fresh = engine.dispatch("vcs.hunks", {{"path", path}});
            auto doc = engine.dispatch("documents.read", {{"path", path}});
            engine.dispatch("documents.replace",
                            {{"path", path}, {"expectedRevision", doc.at("revision")}, {"text", "unsaved"}});
            rejects([&] { engine.dispatch("vcs.applyHunks", {{"id", fresh.at("id")}, {"hunks", {0}}}); },
                    "Dirty document accepted");
        }
        success = true;
        std::cout << "Actual Git partial stage/unstage, literal paths, CRLF, stale/dirty guards passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
    if (success) {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    return success ? 0 : 1;
}
