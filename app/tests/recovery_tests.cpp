#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
namespace fs = std::filesystem;
static void require(bool ok, const std::string &message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void write(const fs::path &file, const std::string &text) {
    std::ofstream(file, std::ios::binary | std::ios::trunc) << text;
}
static void rejects(const std::function<void()> &call) {
    bool rejected = false;
    try {
        call();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Expected a guarded recovery rejection");
}
static constexpr auto document = "été.cpp";
static const std::string original = "\xef\xbb\xbf// original\r\nint value=1;\r\n",
                         edited = "// récupéré 🛠\nint value=42;\n";
static Json reference(const Json &entry) {
    return {{"session", entry.at("session")}, {"id", entry.at("id")}, {"checkpoint", entry.at("checkpoint")}};
}
static Json abandoned(Engine &engine) {
    const auto inventory = engine.dispatch("recovery.list");
    for (const auto &entry : inventory.at("items"))
        if (!entry.at("active").get<bool>())
            return entry;
    throw std::runtime_error("No abandoned checkpoint");
}
static Json restore(Engine &engine, const Json &entry) {
    const auto preview = engine.dispatch("recovery.preview", reference(entry));
    auto parameters = reference(entry);
    parameters["expectedRevision"] = preview.at("targetRevision");
    parameters["expectedDiskState"] = preview.at("diskState");
    parameters["includePreviousText"] = true;
    return engine.dispatch("recovery.restore", parameters);
}
int main(int argc, char **argv) {
    if (argc == 3 && std::string(argv[1]) == "--crash") {
        Engine engine(utf8Path(argv[2]));
        engine.configureLanguage("", false, false);
        const auto doc = engine.dispatch("documents.read", {{"path", document}});
        engine.dispatch("documents.replace",
                        {{"path", document}, {"text", edited}, {"expectedRevision", doc.at("revision")}});
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        while (engine.dispatch("recovery.status").at("documents") == 0) {
            if (std::chrono::steady_clock::now() > deadline)
                std::_Exit(69);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::_Exit(61); // No destructor/normal shutdown: simulate an abrupt process exit.
    }
    const auto root = fs::temp_directory_path() /
                      utf8Path("cycode-recovery-" +
                               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root);
        const auto executable = pathUtf8(fs::absolute(utf8Path(argv[0])));
        auto crash = [&](const std::string &name) {
            const auto folder = root / name;
            fs::create_directory(folder);
            write(folder / utf8Path(document), original);
            const auto process =
                runProcess({executable, "--crash", pathUtf8(folder)}, folder, std::chrono::seconds(10));
            require(process.exitCode == 61, "Crash fixture did not persist automatically: " + process.output);
            return folder;
        };
        {
            const auto folder = crash("ordinary");
            Engine engine(folder);
            engine.configureLanguage("", false, false);
            require(readTextFileBytes(folder / utf8Path(document)) == original,
                    "Autosnapshot modified the source file");
            const auto entry = abandoned(engine),
                       preview = engine.dispatch("recovery.preview", reference(entry));
            require(preview.at("text") == edited && !preview.at("diskChanged").get<bool>(),
                    "Recovery preview lost Unicode or baseline");
            auto stale = reference(entry);
            stale["checkpoint"] = "forged";
            rejects([&] { engine.dispatch("recovery.preview", stale); });
            auto params = reference(entry);
            params["expectedRevision"] = 999;
            params["expectedDiskState"] = preview.at("diskState");
            rejects([&] { engine.dispatch("recovery.restore", params); });
            auto opened = engine.dispatch("documents.read", {{"path", document}});
            params["expectedRevision"] = 0;
            rejects([&] { engine.dispatch("recovery.restore", params); });
            opened = engine.dispatch("documents.replace", {{"path", document},
                                                           {"text", "local changes\n"},
                                                           {"expectedRevision", opened.at("revision")}});
            rejects([&] { restore(engine, entry); });
            engine.dispatch(
                "documents.reload",
                {{"path", document}, {"expectedRevision", opened.at("revision")}, {"discard", true}});
            const auto result = restore(engine, entry);
            require(result.at("document").at("text") == edited &&
                        result.at("checkpointProtected").get<bool>() &&
                        !result.at("sourceRetained").get<bool>(),
                    "Recovery did not safely transfer its checkpoint");
            require(readTextFileBytes(folder / utf8Path(document)) == original,
                    "Restore silently saved the source file");
            const auto current = engine.dispatch("recovery.list");
            require(current.at("items").size() == 1 && current.at("items").at(0).at("active").get<bool>(),
                    "Restored buffer has no live owner checkpoint");
            rejects([&] { engine.dispatch("recovery.discard", reference(current.at("items").at(0))); });
            engine.dispatch("documents.save",
                            {{"path", document}, {"expectedRevision", result.at("document").at("revision")}});
            require(readTextFileBytes(folder / utf8Path(document)) ==
                        encodeEditorText(edited, true, true, false),
                    "Explicit recovered save lost BOM/CRLF");
            require(engine.dispatch("recovery.list").at("items").empty(),
                    "Successful save left a stale checkpoint");
        }
        {
            const auto folder = crash("disk-conflict");
            write(folder / utf8Path(document), "// external\n");
            Engine engine(folder);
            const auto entry = abandoned(engine);
            auto preview = engine.dispatch("recovery.preview", reference(entry));
            require(preview.at("diskChanged").get<bool>(), "Changed disk baseline was ignored");
            auto params = reference(entry);
            params["expectedRevision"] = preview.at("targetRevision");
            params["expectedDiskState"] = preview.at("diskState");
            write(folder / utf8Path(document), "// newer external\n");
            rejects([&] { engine.dispatch("recovery.restore", params); });
            const auto result = restore(engine, entry);
            require(result.at("document").at("diskConflict") == "changed",
                    "Recovery did not retain its external conflict");
            rejects([&] {
                engine.dispatch(
                    "documents.save",
                    {{"path", document}, {"expectedRevision", result.at("document").at("revision")}});
            });
            require(readTextFileBytes(folder / utf8Path(document)) == "// newer external\n",
                    "Recovery overwrote external edits");
            engine.dispatch("documents.close", {{"path", document}, {"discard", true}});
            require(engine.dispatch("recovery.list").at("items").empty(),
                    "Explicit discard left a checkpoint");
        }
        {
            const auto folder = crash("missing");
            fs::remove(folder / utf8Path(document));
            Engine engine(folder);
            const auto result = restore(engine, abandoned(engine));
            require(!fs::exists(folder / utf8Path(document)),
                    "Restoring a missing document created a file implicitly");
            engine.dispatch("documents.save",
                            {{"path", document}, {"expectedRevision", result.at("document").at("revision")}});
            require(readTextFileBytes(folder / utf8Path(document)) ==
                        encodeEditorText(edited, true, true, false),
                    "Explicit Save could not recreate a recovered file");
        }
        {
            const auto folder = crash("corrupt");
            Engine engine(folder);
            const auto entry = abandoned(engine);
            const auto directory = folder / ".cycode/recovery" / entry.at("session").get<std::string>();
            const auto index = Json::parse(readTextFileBytes(directory / "session.json"));
            write(directory / index.at("buffers").at(0).at("textFile").get<std::string>(), "corrupt");
            rejects([&] { engine.dispatch("recovery.preview", reference(entry)); });
            engine.dispatch("recovery.discard", reference(entry));
            require(readTextFileBytes(folder / utf8Path(document)) == original,
                    "Discard deleted/modified the original source");
        }
        {
            const auto folder = root / "active";
            fs::create_directory(folder);
            write(folder / "main.cpp", "initial\n");
            Engine first(folder), second(folder);
            auto doc = first.dispatch("documents.read", {{"path", "main.cpp"}});
            first.dispatch(
                "documents.replace",
                {{"path", "main.cpp"}, {"text", "one instance\n"}, {"expectedRevision", doc.at("revision")}});
            require(first.dispatch("recovery.flush").at("errors").empty(), "Active checkpoint failed");
            auto inventory = second.dispatch("recovery.list");
            require(inventory.at("items").size() == 1 && inventory.at("items").at(0).at("active").get<bool>(),
                    "A live instance was reported as crashed");
            rejects([&] { second.dispatch("recovery.preview", reference(inventory.at("items").at(0))); });
            rejects([&] { second.dispatch("recovery.discard", reference(inventory.at("items").at(0))); });
            first.dispatch("documents.close", {{"path", "main.cpp"}, {"discard", true}});
        }
        {
            const auto folder = root / "large";
            fs::create_directory(folder);
            write(folder / "large.txt", "initial\n");
            Engine engine(folder);
            const auto doc = engine.dispatch("documents.read", {{"path", "large.txt"}});
            const std::string large(maxTextFileBytes, 'x');
            engine.dispatch(
                "documents.replace",
                {{"path", "large.txt"}, {"text", large}, {"expectedRevision", doc.at("revision")}});
            const auto status = engine.dispatch("recovery.flush");
            require(status.at("documents") == 1 && status.at("errors").empty(),
                    "4 MiB text checkpoint failed");
        }
        {
            const auto folder = crash("failed-transfer");
            write(folder / "another.cpp", "initial\n");
            Engine engine(folder);
            const auto source = abandoned(engine);
            const auto another = engine.dispatch("documents.read", {{"path", "another.cpp"}});
            engine.dispatch("documents.replace", {{"path", "another.cpp"},
                                                  {"text", "another dirty buffer\n"},
                                                  {"expectedRevision", another.at("revision")}});
            const auto status = engine.dispatch("recovery.flush");
            const auto own =
                folder / ".cycode/recovery" / status.at("session").get<std::string>() / "session.json";
            // An external edit invalidates the atomic index baseline and prevents transfer.
            write(own, readTextFileBytes(own) + " ");
            const auto result = restore(engine, source);
            require(result.at("sourceRetained").get<bool>() && !result.at("checkpointProtected").get<bool>(),
                    "Failed checkpoint transfer consumed the source backup");
            require(engine.dispatch("recovery.preview", reference(source)).at("text") == edited,
                    "Retained source cannot be read after a transfer failure");
            require(readTextFileBytes(folder / utf8Path(document)) == original,
                    "Failed transfer changed the project file");
        }
        {
            const auto folder = crash("unsafe-path");
            Engine engine(folder);
            const auto entry = abandoned(engine);
            const auto indexFile =
                folder / ".cycode/recovery" / entry.at("session").get<std::string>() / "session.json";
            auto index = Json::parse(readTextFileBytes(indexFile));
            index.at("buffers").at(0)["path"] = "../../outside.cpp";
            write(indexFile, index.dump());
            rejects([&] { engine.dispatch("recovery.preview", reference(entry)); });
            rejects([&] { engine.dispatch("recovery.discard", reference(entry)); });
            require(!engine.dispatch("recovery.list").at("errors").empty(),
                    "Unsafe checkpoint path was not reported");
        }
        {
            const auto folder = root / "quota";
            fs::create_directory(folder);
            RecoveryStore store(folder);
            for (size_t i = 0; i < RecoveryStore::maxDocuments; ++i)
                store.checkpoint({"file" + std::to_string(i) + ".cpp", "dirty\n", "original\n", 1, false,
                                  false, false, true});
            rejects([&] {
                store.checkpoint({"overflow.cpp", "dirty\n", "original\n", 1, false, false, false, true});
            });
            require(store.status().at("documents") == RecoveryStore::maxDocuments &&
                        store.list().at("items").size() == RecoveryStore::maxDocuments,
                    "Quota failure destroyed previous checkpoints");
        }
        fs::remove_all(root);
        std::cout << "Recovery passed: real abrupt exits, automatic checkpoints, Unicode/BOM/CRLF, source "
                     "retention, active-instance isolation, conflicts, missing files, corruption and 4 MiB "
                     "payloads.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}
