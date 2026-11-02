#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace cycode;
static void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F fn, const char *message) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
static void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}
int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--sleep-child") {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 0;
    }
    auto root =
        std::filesystem::temp_directory_path() /
        ("cycode-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(root);
        write(root / "test.cpp", "\xef\xbb\xbfint main() { return 0; }\r\n");
        write(root / ".gitignore", "ignored.txt\n");
        write(root / "ignored.txt", "private_needle\n");
        {
            Engine engine(root);
            auto opened = engine.dispatch("documents.open", {{"path", "test.cpp"}});
            check(opened["revision"] == 1, "Initial revision");
            check(opened["lineEnding"] == "CRLF", "CRLF detection");
            check(opened["encoding"] == "UTF-8 BOM", "BOM detection");
            rejects(
                [&] {
                    engine.dispatch("documents.replace",
                                    {{"path", "test.cpp"}, {"text", "oops"}, {"expectedRevision", 0}});
                },
                "Stale edit must fail");
            auto edited = engine.dispatch(
                "documents.replace",
                {{"path", "test.cpp"}, {"text", "int main() { return 42; }\n"}, {"expectedRevision", 1}});
            check(edited["dirty"] == true, "Unsaved edit");
            auto search = engine.dispatch("search.text", {{"query", "42"}});
            check(search["matches"].size() == 1, "Search unsaved buffers");
            check(engine.dispatch("search.text", {{"query", "return 0"}})["matches"].empty(),
                  "Dirty buffer must hide old disk hits");
            check(engine.dispatch("search.text", {{"query", "private_needle"}})["matches"].empty(),
                  "Respect gitignore");
            engine.dispatch("documents.save", {{"path", "test.cpp"}, {"expectedRevision", 2}});
            std::ifstream file(root / "test.cpp", std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(file)), {});
            file.close();
            check(bytes == "\xef\xbb\xbfint main() { return 42; }\r\n", "Preserve BOM and CRLF");
            write(root / "test.cpp", "external\n");
            rejects(
                [&] { engine.dispatch("documents.save", {{"path", "test.cpp"}, {"expectedRevision", 2}}); },
                "External edit must not be overwritten");
            rejects([&] { engine.dispatch("documents.open", {{"path", "../outside.txt"}}); },
                    "Reject path traversal");
            std::error_code ec;
            std::filesystem::create_directory_symlink(root.parent_path(), root / "escape", ec);
            if (!ec)
                rejects([&] { engine.resolve("escape/outside.txt"); }, "Reject symlink escape");
            rejects(
                [&] {
                    engine.dispatch(
                        "documents.replace",
                        {{"path", "test.cpp"}, {"text", std::string("a\0b", 3)}, {"expectedRevision", 2}});
                },
                "Reject binary replacement");
            engine.dispatch("documents.close", {{"path", "test.cpp"}});
            auto reopened = engine.dispatch("documents.open", {{"path", "test.cpp"}});
            check(reopened["revision"].get<std::uint64_t>() > 2, "Reopen must not reuse an old revision");
            rejects(
                [&] {
                    engine.dispatch("documents.replace",
                                    {{"path", "test.cpp"}, {"text", "stale"}, {"expectedRevision", 2}});
                },
                "Reject a revision from an earlier document lifetime");
            auto created = engine.dispatch("documents.create", {{"path", "new/日本語.cpp"}});
            check(created["text"] == "" && std::filesystem::exists(root / utf8Path("new/日本語.cpp")),
                  "Unicode file creation");
            rejects([&] { engine.dispatch("documents.create", {{"path", "new/日本語.cpp"}}); },
                    "Create must not overwrite");
            write(root / "classic.txt", "alpha\rbeta\r");
            auto classic = engine.dispatch("documents.open", {{"path", "classic.txt"}});
            check(classic["lineEnding"] == "CR" && classic["text"] == "alpha\nbeta\n",
                  "Classic CR line endings");
            auto classicEdit =
                engine.dispatch("documents.replace", {{"path", "classic.txt"},
                                                      {"expectedRevision", classic["revision"]},
                                                      {"text", "gamma\nbeta\n"}});
            engine.dispatch("documents.save",
                            {{"path", "classic.txt"}, {"expectedRevision", classicEdit["revision"]}});
            std::ifstream classicFile(root / "classic.txt", std::ios::binary);
            std::string classicBytes((std::istreambuf_iterator<char>(classicFile)), {});
            classicFile.close();
            check(classicBytes == "gamma\rbeta\r", "Preserve CR-only source files when saving");
            auto init = runProcess({"git", "init"}, root);
            check(init.exitCode == 0, "Git init");
            auto status = engine.dispatch("vcs.status");
            check(status["repository"] == true, "Detect Git repository");
            check(engine.dispatch("vcs.stage", {{"path", "test.cpp"}})["exitCode"] == 0, "Git stage");
            status = engine.dispatch("vcs.status");
            bool staged = false;
            for (const auto &f : status["files"])
                if (f["path"] == "test.cpp" && f["status"] == "A ")
                    staged = true;
            check(staged, "Staged status");
            check(engine.dispatch("vcs.unstage", {{"path", "test.cpp"}})["exitCode"] == 0,
                  "Unstage before the initial commit");
            check(std::filesystem::exists(root / "test.cpp"), "Unstage retains the source file");
            Engine nested(root / "new");
            auto nestedStatus = nested.dispatch("vcs.status");
            bool relative = false;
            for (const auto &file : nestedStatus["files"])
                if (file["path"] == "日本語.cpp")
                    relative = true;
            check(relative, "Nested Git workspace paths must be relative to that workspace");
            std::atomic<bool> cancel{false};
            std::thread cancellation([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                cancel = true;
            });
            auto stopped = runProcess({pathUtf8(std::filesystem::absolute(argv[0])), "--sleep-child"}, root,
                                      std::chrono::seconds(3), 4096, &cancel);
            cancellation.join();
            check(stopped.cancelled && stopped.exitCode != 0,
                  "Cancellation terminates the actual child process");
            auto process = runProcess({"git", "-c", "alias.cycode=!echo unsafe", "--version"}, root);
            check(process.exitCode == 0 && process.output.starts_with("git version"),
                  "Process argv execution");
            if (argc > 1) {
                auto library = std::filesystem::path(argv[1]);
                auto pluginDir = root / "plugins";
                std::filesystem::create_directory(pluginDir);
                std::filesystem::copy_file(library, pluginDir / library.filename());
                write(pluginDir / "sample.json",
                      Json({{"id", "sample"}, {"abi", 1}, {"library", library.stem().string()}}).dump());
                engine.plugins().load(pluginDir);
                auto stats = engine.dispatch("plugins.invoke", {{"command", "sample.text_statistics"},
                                                                {"input", {{"text", "hello world\n"}}}});
                check(stats["words"] == 2 && stats["lines"] == 2, "Real dynamic plugin");
            }
        }
        std::filesystem::remove_all(root);
        std::cout << "Core tests passed: revisions, disk conflicts, encoding, boundaries, unsaved search, "
                     "Git, native plugin.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}
