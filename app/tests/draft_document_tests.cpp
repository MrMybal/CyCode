#include "cycode/Commands.h"
#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include "cycode/WorkspaceSession.h"
#include <fstream>
#include <stdexcept>
namespace {
using namespace cycode;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void reject(F action) {
    try {
        action();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Draft operation unexpectedly accepted");
}
} // namespace
void draftDocumentContracts(const std::filesystem::path &temporary) {
    const auto root = temporary / "draft-documents";
    std::filesystem::create_directory(root);
    std::string abandoned;
    {
        Engine engine(root);
        auto doc =
            engine.dispatch("documents.createUntitled", {{"name", "sample.cpp"}, {"text", "// été\r\n"}});
        const std::string ref = doc.at("path");
        require(isUntitledDocument(ref) && doc.at("untitled") && doc.at("dirty"), "Draft identity/state");
        require(doc.at("text") == "// été\n" && !std::filesystem::exists(root / "sample.cpp"),
                "Draft leaked to source tree");
        require(!engine.supportsLanguage(ref), "Draft unexpectedly enabled a file-backed server");
        reject([&] { engine.dispatch("documents.close", {{"path", ref}, {"discard", true}}); });
        reject([&] { engine.resolve(ref); });
        reject([&] { engine.dispatch("documents.read", {{"path", "untitled://invalid/../escape"}}); });
        auto changed = engine.dispatch(
            "documents.replace",
            {{"path", ref}, {"expectedRevision", doc.at("revision")}, {"text", "// changed\n"}});
        reject([&] {
            engine.dispatch("documents.close",
                            {{"path", ref}, {"expectedRevision", doc.at("revision")}, {"discard", true}});
        });
        reject([&] {
            engine.dispatch("documents.save", {{"path", ref}, {"expectedRevision", changed.at("revision")}});
        });
        std::ofstream(root / "occupied.cpp") << "original\n";
        reject([&] {
            engine.dispatch("documents.saveAs", {{"path", ref},
                                                 {"destination", "occupied.cpp"},
                                                 {"expectedRevision", changed.at("revision")}});
        });
        require(readTextFileBytes(root / "occupied.cpp") == "original\n", "Save as overwrote a file");
        auto saved = engine.dispatch(
            "documents.saveAs",
            {{"path", ref}, {"destination", "saved.cpp"}, {"expectedRevision", changed.at("revision")}});
        require(saved.at("id") == doc.at("id") && !saved.at("dirty").get<bool>() &&
                    !saved.at("untitled").get<bool>(),
                "Save as lost identity/state");
        require(readTextFileBytes(root / "saved.cpp") == "// changed\n", "Draft save bytes");
        reject([&] { engine.dispatch("documents.read", {{"path", ref}}); });
        auto copy = engine.dispatch(
            "documents.saveAs",
            {{"path", "saved.cpp"}, {"destination", "copy.cpp"}, {"expectedRevision", saved.at("revision")}});
        require(copy.at("id") == doc.at("id") && readTextFileBytes(root / "saved.cpp") == "// changed\n",
                "Save as changed original disk file");
        engine.dispatch("documents.close", {{"path", "copy.cpp"}, {"expectedRevision", copy.at("revision")}});
        const auto empty = engine.dispatch("documents.createUntitled");
        require(empty.at("dirty") && empty.at("text") == "", "Empty draft would not be recovered");
        abandoned = empty.at("path");
        engine.dispatch("recovery.flush");
    }
    {
        Engine engine(root);
        const auto items = engine.dispatch("recovery.list").at("items");
        Json record;
        for (const auto &item : items)
            if (item.at("path") == abandoned)
                record = item;
        require(!record.is_null() && !record.at("active").get<bool>(), "Empty draft checkpoint missing");
        Json params = {{"session", record.at("session")},
                       {"id", record.at("id")},
                       {"checkpoint", record.at("checkpoint")}};
        auto preview = engine.dispatch("recovery.preview", params);
        require(preview.at("diskState") == "untitled" && preview.at("targetRevision") == 0,
                "Recovery treated draft as disk file");
        params["expectedRevision"] = preview.at("targetRevision");
        params["expectedDiskState"] = preview.at("diskState");
        auto restored = engine.dispatch("recovery.restore", params).at("document");
        require(restored.at("path") == abandoned && restored.at("dirty"), "Recovery lost draft reference");
        engine.dispatch(
            "documents.close",
            {{"path", abandoned}, {"expectedRevision", restored.at("revision")}, {"discard", true}});
    }
    Keymap keys;
    keys.load("vscode");
    require(keys.press("ctrl+n", 0, true, true).command == "file.newUntitled", "VS Code new draft key");
    require(keys.press("ctrl+shift+s", 1, true, true).command == "file.saveAs", "VS Code Save as key");
    require(keys.press("ctrl+k", 2, true, true).consumed &&
                keys.press("s", 2.1, true, true).command == "file.saveAll",
            "VS Code Save all chord");
    keys.load("rider");
    require(keys.press("ctrl+n", 3, true, true).command == "search.types", "Rider type lookup changed");
    Json state = {{"schemaVersion", 1},
                  {"workspace", "workspace"},
                  {"tabs", Json::array({{{"path", abandoned},
                                         {"windowKey", "draft-window"},
                                         {"line", 2},
                                         {"column", 3},
                                         {"scroll", 1}}})},
                  {"recent", Json::array()},
                  {"active", abandoned},
                  {"panels", Json::object()},
                  {"layout", ""}};
    validateWorkspaceSession(state, "workspace");
}
