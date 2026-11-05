#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include "cycode/FileTransactions.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void write(const fs::path &path, const std::string &text) {
    std::ofstream(path, std::ios::binary) << text;
}
static void rejects(const std::function<void()> &action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Expected a guarded resource rejection");
}
static Json apply(Engine &engine, const Json &params) {
    const auto plan = engine.dispatch("resources.preview", params);
    return engine.dispatch("resources.apply", {{"id", plan.at("id")}});
}
int main() {
    const auto root = fs::temp_directory_path() /
                      utf8Path("cycode-resources-" +
                               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root / "src");
        const std::string binary("one\0two",7);
        writeFileExclusive(root/"exclusive.bin",binary);
        rejects([&]{writeFileExclusive(root/"exclusive.bin","replacement");});
        {std::ifstream file(root/"exclusive.bin",std::ios::binary);require(std::string(std::istreambuf_iterator<char>(file),{})==binary,"Exclusive binary writer must never truncate an existing destination");}
        fs::create_directory(root / "target");
        const std::string text = "// été 🛠\nint value=1;\n";
        write(root / "src/main.cpp", text);
        std::string trashId;
        {
            Engine engine(root);
            engine.configureLanguage("", false, false);
            const auto doc = engine.dispatch("documents.read", {{"path", "src/main.cpp"}});
            const auto before = engine.dispatch("workspace.files").at("indexRevision");
            auto plan = engine.dispatch(
                "resources.preview", {{"operation", "move"}, {"path", "src"}, {"destination", "target/été"}});
            require(!fs::exists(root / utf8Path("target/été")), "Preview changed the filesystem");
            auto dirty = engine.dispatch(
                "documents.replace",
                {{"path", "src/main.cpp"}, {"text", "unsaved\n"}, {"expectedRevision", doc.at("revision")}});
            rejects([&] { engine.dispatch("resources.apply", {{"id", plan.at("id")}}); });
            auto blocked = engine.dispatch("resources.preview", {{"operation", "delete"}, {"path", "src"}});
            require(blocked.at("blocked").get<bool>(), "Dirty directory was not marked blocked");
            rejects([&] { engine.dispatch("resources.apply", {{"id", blocked.at("id")}}); });
            engine.dispatch(
                "documents.replace",
                {{"path", "src/main.cpp"}, {"text", text}, {"expectedRevision", dirty.at("revision")}});
            const auto moved =
                apply(engine, {{"operation", "move"}, {"path", "src"}, {"destination", "target/été"}});
            const auto renamed = engine.dispatch("documents.read", {{"path", "target/été/main.cpp"}});
            require(renamed.at("id") == doc.at("id") && renamed.at("revision") != doc.at("revision"),
                    "Move lost document identity or retained its old revision");
            require(!fs::exists(root / "src") &&
                        readTextFileBytes(root / utf8Path("target/été/main.cpp")) == text,
                    "Directory move lost content");
            const auto files = engine.dispatch("workspace.files");
            require(files.at("indexRevision") != before,
                    "Move did not invalidate the path index immediately");
            engine.dispatch("resources.undo", {{"id", moved.at("id")}});
            require(fs::exists(root / "src/main.cpp") && !fs::exists(root / utf8Path("target/été")),
                    "Undo did not restore the directory");
            require(engine.dispatch("documents.read", {{"path", "src/main.cpp"}}).at("id") == doc.at("id"),
                    "Undo replaced the live document identity");
            const auto removed = apply(engine, {{"operation", "delete"}, {"path", "src"}});
            trashId = removed.at("trash");
            require(!fs::exists(root / "src") &&
                        engine.dispatch("documents.list").at("documents").at(0).at("diskConflict") ==
                            "missing",
                    "Delete did not preserve the missing open buffer");
            require(engine.dispatch("resources.trash").at("items").size() == 1,
                    "Deletion has no persistent trash entry");
        }
        {
            Engine engine(root);
            apply(engine, {{"operation", "restore"}, {"trash", trashId}});
            require(readTextFileBytes(root / "src/main.cpp") == text,
                    "Trash restore after restart lost content");
            require(engine.dispatch("resources.trash").at("items").empty(),
                    "Restored trash entry stayed selectable");
            write(root / "exists.cpp", "occupied\n");
            rejects([&] {
                engine.dispatch(
                    "resources.preview",
                    {{"operation", "move"}, {"path", "src/main.cpp"}, {"destination", "exists.cpp"}});
            });
            for (const auto *unsafe :
                 {"../outside.cpp", ".cycode/hidden", ".git/config", "runtime/tool", "data/file"})
                rejects([&] {
                    engine.dispatch("resources.preview", {{"operation", "createFile"}, {"path", unsafe}});
                });
            rejects([&] { engine.dispatch("resources.preview", {{"operation", "delete"}, {"path", "."}}); });
            rejects([&] {
                engine.dispatch("resources.preview",
                                {{"operation", "move"}, {"path", "src"}, {"destination", "src/child"}});
            });
            auto changed =
                engine.dispatch("resources.preview", {{"operation", "delete"}, {"path", "src/main.cpp"}});
            write(root / "src/main.cpp", "external\n");
            rejects([&] { engine.dispatch("resources.apply", {{"id", changed.at("id")}}); });
            require(readTextFileBytes(root / "src/main.cpp") == "external\n",
                    "Stale plan overwrote external changes");
            const auto created = apply(engine, {{"operation", "createFile"}, {"path", "target/new.hlsl"}});
            write(root / "target/new.hlsl", "float4 main() : SV_Target { return 1; }\n");
            rejects([&] { engine.dispatch("resources.undo", {{"id", created.at("id")}}); });
            const auto folder = apply(engine, {{"operation", "createDirectory"}, {"path", "target/empty"}});
            engine.dispatch("resources.undo", {{"id", folder.at("id")}});
            require(!fs::exists(root / "target/empty"), "Undo folder creation failed");
            const auto deleted = apply(engine, {{"operation", "delete"}, {"path", "exists.cpp"}});
            write(root / "exists.cpp", "replacement\n");
            rejects([&] { engine.dispatch("resources.undo", {{"id", deleted.at("id")}}); });
            require(readTextFileBytes(root / "exists.cpp") == "replacement\n",
                    "Restore overwrote a newer file");
            const std::string binary("\0\1\2\3", 4);
            write(root / "binary.png", binary);
            const auto binaryDeleted = apply(engine, {{"operation", "delete"}, {"path", "binary.png"}});
            engine.dispatch("resources.undo", {{"id", binaryDeleted.at("id")}});
            require(readTextFileBytes(root / "binary.png") == binary, "Binary restoration changed bytes");
            const auto events = engine.dispatch("resources.events");
            require(!events.at("events").empty() && !events.at("gap").get<bool>(),
                    "Resource events are missing");
            const auto cursor = events.at("revision");
            require(engine.dispatch("resources.events", {{"after", cursor}}).at("events").empty(),
                    "Event cursor replayed old operations");
        }
        {
            write(root / "active.sln", "Microsoft Visual Studio Solution File, Format Version 12.00\n");
            Engine engine(root / "active.sln");
            rejects([&] {
                engine.dispatch("resources.preview", {{"operation", "delete"}, {"path", "active.sln"}});
            });
        }
        {
            fs::create_directory(root / "wide");
            for (int i = 0; i < 4096; ++i)
                write(root / "wide" / (std::to_string(i) + ".txt"), "");
            Engine engine(root);
            rejects(
                [&] { engine.dispatch("resources.preview", {{"operation", "delete"}, {"path", "wide"}}); });
            require(fs::is_directory(root / "wide") && fs::exists(root / "wide/4095.txt"),
                    "Oversized resource preview modified its source");
            std::error_code linkError;
            fs::create_directory_symlink(root / "src", root / "link", linkError);
            if (!linkError) {
                rejects([&] {
                    engine.dispatch("resources.preview", {{"operation", "delete"}, {"path", "link"}});
                });
                rejects([&] {
                    engine.dispatch("resources.preview",
                                    {{"operation", "createFile"}, {"path", "link/escaped.cpp"}});
                });
                fs::remove(root / "link");
            }
        }
        fs::remove_all(root);
        std::cout << "Resource operations passed: guarded preview/apply/undo, persistent trash, document "
                     "identities, dirty/stale rejection, binaries, protected paths, project entry and index "
                     "invalidation.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}
