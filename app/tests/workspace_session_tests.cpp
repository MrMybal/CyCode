#include "cycode/Paths.h"
#include "cycode/WorkspaceSession.h"
#include <fstream>
#include <limits>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid workspace session state was accepted");
}
} // namespace
void workspaceSessionContracts(const std::filesystem::path &root) {
    using namespace cycode;
    const auto directory = root / ".cycode" / utf8Path("session-state & été");
    const std::string key = "canonical/workspace";
    const Json snapshot = {{"schemaVersion", 1},
                           {"workspace", key},
                           {"tabs", Json::array({{{"path", "src/été.cpp"},
                                                  {"windowKey", "stable-before-rename"},
                                                  {"line", 15},
                                                  {"column", 3},
                                                  {"scroll", 10}}})},
                           {"active", "src/été.cpp"},
                           {"recent", Json::array({"src/été.cpp"})},
                           {"panels", {{"project", true}, {"debugger", false}}},
                           {"layout", "[Window][fixture]\nPos=10,20\n"}};
    require(validateWorkspaceSession(snapshot, key).at("tabs")[0].at("windowKey") == "stable-before-rename",
            "Session lost stable window identity");
    auto selected = snapshot;
    selected["git"] = {{"version", 1}, {"root", "plugins/outil été"}, {"identity", "observed-repository"}};
    require(validateWorkspaceSession(selected, key).at("git") == selected.at("git"),
            "Session lost explicit Git selection");
    require(validateSessionGit({{"version", 1}, {"root", ""}, {"identity", ""}}).at("root") == "",
            "Automatic Git selection was lost");
    require(validateSessionGit({{"version", 1}, {"root", "."}, {"identity", "root-repository"}}).at("root") ==
                ".",
            "Workspace Git selection was lost");
    for (const auto *folder : {"../outside", "/absolute", "C:/outside", "a\\b", ".git/objects", "a/.CYCODE/x",
                               "a/./b", "untitled://draft"}) {
        selected["git"]["root"] = folder;
        rejects([&] { validateWorkspaceSession(selected, key); });
    }
    rejects([&] { validateSessionGit({{"version", 1}, {"root", "one"}, {"identity", ""}}); });
    rejects([&] { validateSessionGit({{"version", 1}, {"root", ""}, {"identity", "other"}}); });
    rejects([&] {
        validateSessionGit({{"version", 1}, {"root", "one"}, {"identity", "id"}, {"command", "git"}});
    });
    const Json editor = {{"version", 1},     {"wrap", true},          {"anchorLine", 8},
                         {"folds", {4, 12}}, {"foldsComplete", true}, {"fingerprint", "fnv1a64-example"}};
    const Json image = {{"version", 1}, {"zoom", 2.5},       {"centerX", 120.}, {"centerY", -20.},
                        {"fit", false}, {"pixelated", true}, {"width", 640},    {"height", 480}};
    auto views = snapshot;
    views["tabs"][0]["editor"] = editor;
    auto picture = snapshot.at("tabs")[0];
    picture["path"] = "images/view.png";
    picture["windowKey"] = "image-window";
    picture["image"] = image;
    views["tabs"].push_back(picture);
    require(validateWorkspaceSession(views, key).at("tabs")[1].at("image") == image,
            "Session lost image view or editor wrap/folds");
    auto invalidEditor = editor;
    invalidEditor["folds"] = {12, 4};
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor["folds"] = {4, 4};
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor["folds"] = {-1};
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor = editor;
    invalidEditor["fingerprint"] = "";
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor = editor;
    invalidEditor["foldsComplete"] = false;
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor = editor;
    invalidEditor["wrap"] = 1;
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor = editor;
    invalidEditor["anchorLine"] = 10000001;
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor = editor;
    invalidEditor["unknown"] = "value";
    rejects([&] { validateSessionEditor(invalidEditor); });
    invalidEditor = editor;
    invalidEditor["folds"] = Json::array();
    for (size_t i = 0; i <= sessionMaxFolds; ++i)
        invalidEditor["folds"].push_back(i);
    rejects([&] { validateSessionEditor(invalidEditor); });
    auto excessive = views;
    excessive["tabs"] = Json::array();
    invalidEditor["folds"].erase(invalidEditor["folds"].end() - 1);
    for (int i = 0; i < 5; ++i) {
        auto item = snapshot.at("tabs")[0];
        item["path"] = std::to_string(i) + ".cpp";
        item["windowKey"] = item["path"];
        item["editor"] = invalidEditor;
        excessive["tabs"].push_back(item);
    }
    excessive["active"] = "0.cpp";
    rejects([&] { validateWorkspaceSession(excessive, key); });
    for (const auto *field : {"zoom", "centerX", "centerY"}) {
        auto badImage = image;
        badImage[field] = std::numeric_limits<double>::infinity();
        rejects([&] { validateSessionImage(badImage); });
    }
    auto badImage = image;
    badImage["zoom"] = 0;
    rejects([&] { validateSessionImage(badImage); });
    badImage = image;
    badImage["width"] = 0;
    rejects([&] { validateSessionImage(badImage); });
    badImage = image;
    badImage["height"] = 32769;
    rejects([&] { validateSessionImage(badImage); });
    badImage = image;
    badImage["pixelated"] = 1;
    rejects([&] { validateSessionImage(badImage); });
    badImage = image;
    badImage["command"] = "not-metadata";
    rejects([&] { validateSessionImage(badImage); });
    auto both = views;
    both["tabs"][0]["image"] = image;
    rejects([&] { validateWorkspaceSession(both, key); });
    for (const auto &path : {"../outside.cpp", "sdk://old-reference", "debug://1/frame", "C:/outside.cpp",
                             "/outside.cpp", "a\\b.cpp"}) {
        auto bad = snapshot;
        bad["tabs"][0]["path"] = path;
        bad["active"] = path;
        rejects([&] { validateWorkspaceSession(bad, key); });
    }
    auto bad = snapshot;
    bad["tabs"].push_back(bad["tabs"][0]);
    rejects([&] { validateWorkspaceSession(bad, key); });
    bad = snapshot;
    bad["tabs"][0]["line"] = -1;
    rejects([&] { validateWorkspaceSession(bad, key); });
    bad = snapshot;
    bad["active"] = "missing.cpp";
    rejects([&] { validateWorkspaceSession(bad, key); });
    bad = snapshot;
    bad["layout"] = std::string(512 * 1024 + 1, 'x');
    rejects([&] { validateWorkspaceSession(bad, key); });
    rejects([&] { validateWorkspaceSession(snapshot, "other-workspace"); });
    auto a = std::make_unique<WorkspaceSession>(directory, key, "instance-a");
    a->save(views);
    auto b = std::make_unique<WorkspaceSession>(directory, key, "instance-b");
    require(b->latest().is_null(), "A second instance restored a live session's state");
    rejects([&] { WorkspaceSession duplicate(directory, key, "instance-a"); });
    a.reset();
    require(b->latest().at("active") == "src/été.cpp",
            "Closed/crashed session was not available to another instance");
    require(b->latest().at("tabs")[0].at("editor") == editor &&
                b->latest().at("tabs")[1].at("image") == image,
            "View metadata did not survive persistence");
    auto empty = snapshot;
    empty["tabs"] = Json::array();
    empty["active"] = "";
    b->save(empty, true);
    b.reset();
    WorkspaceSession c(directory, key, "instance-c");
    const auto restored = c.latest();
    require(restored.at("tabs").empty() && restored.at("closed"),
            "An intentionally empty closed session resurrected old tabs");
    c.save(snapshot);
    std::ofstream(directory / "instance-c.json", std::ios::trunc) << "external edit\n";
    rejects([&] { c.save(empty); });
    std::error_code error;
    std::filesystem::create_symlink(directory / "instance-a.lock", directory / "alias.lock", error);
    if (!error)
        rejects([&] { NativeFileLease lease(directory / "alias.lock"); });
}
