#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}
int main() {
    const auto folder = std::filesystem::temp_directory_path() /
                        utf8Path("cycode-watch-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(folder / "sub");
        write(folder / "main.cpp", "int initial;\n");
        write(folder / "sub/été.txt", "été\n");
        {
            Engine engine(folder);
            engine.configureLanguage("", false, false);
            auto wait = [&](auto condition) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
                for (;;) {
                    const auto poll = engine.dispatch("workspace.poll");
                    if (condition(poll))
                        return poll;
                    if (std::chrono::steady_clock::now() > deadline)
                        throw std::runtime_error("File watcher reconciliation timed out");
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                }
            };
            const auto opened = engine.dispatch("documents.read", {{"path", "main.cpp"}});
            const auto initial = engine.dispatch("workspace.files", {{"limit", 1}});
            require(initial.at("files").size() == 1 && !initial.at("nextOffset").is_null(),
                    "Path index did not paginate");
            const auto second =
                engine.dispatch("workspace.files", {{"offset", initial.at("nextOffset")}, {"limit", 1}});
            require(second.at("files").size() == 1 && second.at("nextOffset").is_null(),
                    "Path index lost its final page");
            engine.dispatch("workspace.poll");
            write(folder / "main.cpp", "int disk_update;\n");
            wait([&](const Json &) {
                return engine.dispatch("documents.read", {{"path", "main.cpp"}}).at("revision") !=
                       opened.at("revision");
            });
            auto doc = engine.dispatch("documents.read", {{"path", "main.cpp"}});
            require(doc.at("text") == "int disk_update;\n" && !doc.at("dirty").get<bool>(),
                    "Clean external edit did not reload");
            engine.dispatch(
                "documents.replace",
                {{"path", "main.cpp"}, {"text", "int unsaved;\n"}, {"expectedRevision", doc.at("revision")}});
            write(folder / "main.cpp", "int conflicting;\n");
            wait([](const Json &poll) { return !poll.at("conflicts").empty(); });
            doc = engine.dispatch("documents.read", {{"path", "main.cpp"}});
            require(doc.at("text") == "int unsaved;\n" && doc.at("diskConflict") == "changed",
                    "External edit destroyed a dirty buffer");
            bool rejected = false;
            try {
                engine.dispatch("documents.save",
                                {{"path", "main.cpp"}, {"expectedRevision", doc.at("revision")}});
            } catch (const std::exception &) {
                rejected = true;
            }
            require(rejected, "Conflicted document overwrote disk");
            engine.dispatch(
                "documents.reload",
                {{"path", "main.cpp"}, {"expectedRevision", doc.at("revision")}, {"discard", true}});
            write(folder / "added.txt", "new\n");
            wait([&](const Json &poll) { return poll.at("indexRevision") != initial.at("indexRevision"); });
            require(engine.dispatch("workspace.files").at("total") == 3,
                    "Created file did not invalidate the path index");
            std::filesystem::rename(folder / "added.txt", folder / "renamed.txt");
            wait([&](const Json &) {
                const auto files = engine.dispatch("workspace.files").at("files");
                return std::find(files.begin(), files.end(), "renamed.txt") != files.end();
            });
            std::filesystem::remove(folder / "main.cpp");
            wait([](const Json &poll) {
                for (const auto &item : poll.at("conflicts"))
                    if (item.at("path") == "main.cpp" && item.at("reason") == "missing")
                        return true;
                return false;
            });
            require(engine.dispatch("documents.read", {{"path", "main.cpp"}}).at("text") ==
                        "int conflicting;\n",
                    "Deleted file lost its open buffer");
            write(folder / "main.cpp", "int restored;\n");
            wait([&](const Json &poll) {
                return poll.at("conflicts").empty() &&
                       engine.dispatch("documents.read", {{"path", "main.cpp"}}).at("text") ==
                           "int restored;\n";
            });
            write(folder / "main.cpp", std::string("invalid\0binary", 14));
            wait([](const Json &poll) { return !poll.at("conflicts").empty(); });
            require(engine.dispatch("documents.read", {{"path", "main.cpp"}}).at("text") == "int restored;\n",
                    "Invalid external content replaced valid text");
        }
        std::filesystem::remove_all(folder);
        std::cout << "Workspace passed: native notifications, pagination, clean reload, "
                     "dirty/missing/invalid retention and index invalidation.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << folder << '\n';
        return 1;
    }
}
