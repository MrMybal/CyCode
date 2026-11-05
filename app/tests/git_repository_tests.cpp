#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <stdexcept>
namespace {
using namespace cycode;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void reject(F fn) {
    try {
        fn();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Unsafe repository operation accepted");
}
std::string records(std::initializer_list<std::string> fields) {
    std::string out;
    for (const auto &field : fields) {
        out += field;
        out += '\0';
    }
    return out;
}
} // namespace
void gitRepositoryContracts(const std::filesystem::path &temporary) {
    const Json maintenanceModel = {
        {"action", "move"}, {"path", "linked/outil été"}, {"destination", "moved/outil;$(text)"}};
    require(gitWorktreeMaintenanceRequest(maintenanceModel) == maintenanceModel,
            "Worktree paths were interpreted as commands");
    for (const auto *path : {"", ".", "..", "../outside", "/outside", "C:/outside", "a\\b", "a//b", "a/./b",
                             "a/", ".git/worktrees/x", "x/.CYCODE/y", "a./b", "a /b", "a\nname"}) {
        auto invalid = maintenanceModel;
        invalid["destination"] = path;
        reject([&] { gitWorktreeMaintenanceRequest(invalid); });
    }
    auto invalidMaintenance = maintenanceModel;
    invalidMaintenance["action"] = "prune";
    reject([&] { gitWorktreeMaintenanceRequest(invalidMaintenance); });
    invalidMaintenance = maintenanceModel;
    invalidMaintenance["destination"] = invalidMaintenance.at("path");
    reject([&] { gitWorktreeMaintenanceRequest(invalidMaintenance); });
    const auto parsed = parseGitWorktrees(
        records({"worktree /a/with\nnewline", "HEAD " + std::string(40, 'a'), "branch refs/heads/main",
                 "locked two\nlines", "", "worktree /b", "detached", ""}));
    require(parsed.size() == 2 && parsed[0].at("lockReason") == "two\nlines" && parsed[1].at("detached"),
            "Worktree NUL parser lost filename/lock data");
    reject([&] { parseGitWorktrees("worktree /a"); });
    reject([&] { parseGitWorktrees(records({"worktree /a", "locked", "locked again", ""})); });
    reject([&] { parseGitWorktrees(records({"HEAD abcd", ""})); });
    const auto root = temporary / "multi-repository";
    const auto workspace = root / "workspace";
    std::filesystem::create_directories(workspace / "one");
    std::filesystem::create_directories(workspace / "two");
    auto git = [&](const std::filesystem::path &folder, std::vector<std::string> args) {
        args.insert(args.begin(), {"git", "-C", pathUtf8(folder), "-c", "user.name=CyCode test", "-c",
                                   "user.email=cycode@example.invalid", "-c", "commit.gpgsign=false", "-c",
                                   "core.autocrlf=false"});
        auto result = runProcess(args, folder);
        if (result.exitCode)
            throw std::runtime_error(result.output);
        return result.output;
    };
    for (const auto *name : {"one", "two"}) {
        const auto folder = workspace / name;
        git(folder, {"init", "--initial-branch=main"});
        std::ofstream(folder / "file.cpp") << "int original=1;\n";
        git(folder, {"add", "."});
        git(folder, {"commit", "-m", "Initial"});
    }
    Engine engine(workspace);
    auto call = [&](const std::string &method, const std::string &repository, Json parameters) {
        parameters["repository"] = repository;
        parameters["provider"] = "git";
        auto result = engine.dispatch(method, parameters);
        if (result.contains("exitCode") && result.at("exitCode") != 0)
            throw std::runtime_error(result.value("output", std::string{}));
        return result;
    };
    const auto inventory = engine.dispatch("vcs.repositories");
    require(inventory.at("repositories").size() == 2, "Nested repository inventory");
    const auto context = engine.dispatch("vcs.context", {{"contextPath", "one/file.cpp"}});
    require(context.at("gitRepository") == "one", "Active-file repository selection");
    auto other = engine.dispatch("documents.read", {{"path", "two/file.cpp"}});
    other = engine.dispatch("documents.replace", {{"path", "two/file.cpp"},
                                                  {"text", "// unsaved other repository\n"},
                                                  {"expectedRevision", other.at("revision")}});
    call("vcs.branch.create", "one", {{"name", "feature"}});
    call("vcs.branch.switch", "one", {{"name", "feature"}});
    require(engine.dispatch("documents.read", {{"path", "two/file.cpp"}}).at("revision") ==
                other.at("revision"),
            "Other repository buffer was touched");
    reject([&] { call("vcs.stage", "one", {{"path", "two/file.cpp"}}); });
    std::ofstream(workspace / "one/file.cpp") << "int changed=2;\n";
    const auto status = call("vcs.status", "one", Json::object());
    require(status.at("files")[0].at("path") == "one/file.cpp",
            "Git returned repository-relative editor paths");
    const auto hunks = call("vcs.hunks", "one", {{"path", "one/file.cpp"}});
    reject([&] { call("vcs.applyHunks", "two", {{"id", hunks.at("id")}, {"hunks", Json::array({0})}}); });
    std::filesystem::create_directories(workspace / "one/folder");
    std::ofstream(workspace / "one/folder/nested.cpp") << "int nested;\n";
    call("vcs.stage", "one", {{"path", "one/folder"}});
    call("vcs.unstage", "one", {{"path", "one/folder"}});
    auto catalogue = call("vcs.worktrees", "one", Json::object());
    auto created = call("vcs.worktree.add", "one",
                        {{"destination", "linked"},
                         {"mode", "newBranch"},
                         {"branch", "work"},
                         {"expectedRevision", catalogue.at("revision")}});
    catalogue = created.at("catalog");
    require(std::filesystem::exists(workspace / "linked/.git"), "Linked checkout missing");
    auto opened = engine.dispatch("documents.read", {{"path", "linked/file.cpp"}});
    reject([&] {
        call("vcs.worktree.remove", "one",
             {{"path", "linked"}, {"expectedRevision", catalogue.at("revision")}});
    });
    engine.dispatch("documents.close",
                    {{"path", "linked/file.cpp"}, {"expectedRevision", opened.at("revision")}});
    // Move/repair fixtures are only executed by a separately authorized native run.
    std::filesystem::create_directory(workspace / "trees");
    std::ofstream(workspace / "one/.git/info/exclude", std::ios::app) << "\nignored.bin\n";
    std::ofstream(workspace / "linked/file.cpp") << "int stagedLocal=7;\n";
    git(workspace / "linked", {"add", "file.cpp"});
    std::ofstream(workspace / "linked/untracked.txt") << "keep untracked\n";
    std::ofstream(workspace / "linked/ignored.bin") << "keep ignored\n";
    const auto movedIndex = git(workspace / "linked", {"ls-files", "--stage", "-z"});
    auto maintenanceParameters = [&](const std::string &source, const std::string &destination) {
        const auto actual = call("vcs.worktrees", "one", Json::object());
        return Json{{"path", source},
                    {"destination", destination},
                    {"expectedRepository", actual.at("identity")},
                    {"expectedRevision", actual.at("revision")}};
    };
    auto move = maintenanceParameters("linked", "trees/moved");
    auto inputPlan = move;
    inputPlan["action"] = "move";
    const auto movePreview = call("vcs.worktree.plan", "one", inputPlan);
    require(movePreview.at("preservesLocalFiles") == true && movePreview.at("preservesIndex") == true &&
                !std::filesystem::exists(workspace / "trees/moved") &&
                std::filesystem::exists(workspace / "linked/ignored.bin"),
            "Move preview changed the payload or hid retention effects");
    move["expectedPlanRevision"] = "obsolete-preview";
    reject([&] { call("vcs.worktree.move", "one", move); });
    move["expectedPlanRevision"] = movePreview.at("revision");
    auto metadataName = git(workspace / "linked", {"rev-parse", "--absolute-git-dir"});
    while (!metadataName.empty() && (metadataName.back() == '\n' || metadataName.back() == '\r'))
        metadataName.pop_back();
    const auto metadataPath = pathUtf8((utf8Path(metadataName) / "HEAD").lexically_relative(workspace));
    const auto metadataDoc = engine.dispatch("documents.read", {{"path", metadataPath}});
    reject([&] { call("vcs.worktree.move", "one", move); });
    engine.dispatch("documents.close",
                    {{"path", metadataPath}, {"expectedRevision", metadataDoc.at("revision")}});
    std::filesystem::create_directory(workspace / "trees/moved");
    reject([&] { call("vcs.worktree.move", "one", move); });
    std::filesystem::remove(workspace / "trees/moved");
    std::filesystem::rename(workspace / "trees", workspace / "trees-original");
    std::filesystem::create_directory(workspace / "trees");
    reject([&] { call("vcs.worktree.move", "one", move); });
    std::filesystem::remove(workspace / "trees");
    std::filesystem::rename(workspace / "trees-original", workspace / "trees");
    const auto movingDoc = engine.dispatch("documents.read", {{"path", "linked/file.cpp"}});
    reject([&] { call("vcs.worktree.move", "one", move); });
    engine.dispatch("documents.close",
                    {{"path", "linked/file.cpp"}, {"expectedRevision", movingDoc.at("revision")}});
    const auto moved = call("vcs.worktree.move", "one", move);
    require(!moved.contains("catalogError") && !std::filesystem::exists(workspace / "linked") &&
                readTextFileBytes(workspace / "trees/moved/file.cpp") == "int stagedLocal=7;\n" &&
                readTextFileBytes(workspace / "trees/moved/untracked.txt") == "keep untracked\n" &&
                readTextFileBytes(workspace / "trees/moved/ignored.bin") == "keep ignored\n" &&
                git(workspace / "trees/moved", {"ls-files", "--stage", "-z"}) == movedIndex,
            "Worktree move lost staged, untracked or ignored local files");
    std::filesystem::rename(workspace / "trees/moved", workspace / "trees/manual");
    auto repair = maintenanceParameters("trees/moved", "trees/manual");
    auto repairPlan = repair;
    repairPlan["action"] = "repair";
    const auto repairPreview = call("vcs.worktree.plan", "one", repairPlan);
    require(repairPreview.at("repairsLinks") == true && repairPreview.at("movesDirectory") == false,
            "Repair invented a directory move");
    std::filesystem::create_directory(workspace / "one/.git/worktrees/unreviewed");
    reject([&] { call("vcs.worktree.plan", "one", repairPlan); });
    std::filesystem::remove(workspace / "one/.git/worktrees/unreviewed");
    git(workspace / "one", {"worktree", "add", "--detach", pathUtf8(root / "maintenance-external"), "HEAD"});
    repair = maintenanceParameters("trees/moved", "trees/manual");
    repairPlan = repair;
    repairPlan["action"] = "repair";
    reject([&] { call("vcs.worktree.plan", "one", repairPlan); });
    git(workspace / "one", {"worktree", "remove", pathUtf8(root / "maintenance-external")});
    git(workspace / "one", {"worktree", "add", "--detach", pathUtf8(workspace / "trees/sibling"), "HEAD"});
    repair = maintenanceParameters("trees/moved", "trees/manual");
    repairPlan = repair;
    repairPlan["action"] = "repair";
    const auto siblingDoc = engine.dispatch("documents.read", {{"path", "trees/sibling/file.cpp"}});
    reject([&] { call("vcs.worktree.plan", "one", repairPlan); });
    engine.dispatch("documents.close",
                    {{"path", "trees/sibling/file.cpp"}, {"expectedRevision", siblingDoc.at("revision")}});
    const auto finalRepairPreview = call("vcs.worktree.plan", "one", repairPlan);
    repair["expectedPlanRevision"] = finalRepairPreview.at("revision");
    const auto repaired = call("vcs.worktree.repair", "one", repair);
    require(!repaired.contains("catalogError") &&
                std::filesystem::exists(workspace / "trees/manual/ignored.bin") &&
                git(workspace / "trees/manual", {"ls-files", "--stage", "-z"}) == movedIndex,
            "Link repair lost the moved payload/index or remained unconfirmed");
    git(workspace / "one", {"worktree", "remove", pathUtf8(workspace / "trees/sibling")});
    auto restoreMove = maintenanceParameters("trees/manual", "linked");
    auto restorePlan = restoreMove;
    restorePlan["action"] = "move";
    restoreMove["expectedPlanRevision"] = call("vcs.worktree.plan", "one", restorePlan).at("revision");
    call("vcs.worktree.move", "one", restoreMove);
    git(workspace / "linked", {"restore", "--staged", "--worktree", "--", "file.cpp"});
    std::filesystem::remove(workspace / "linked/untracked.txt");
    std::filesystem::remove(workspace / "linked/ignored.bin");
    catalogue = call("vcs.worktrees", "one", Json::object());
    const auto locked =
        call("vcs.worktree.lock", "one",
             {{"path", "linked"}, {"reason", "test lock"}, {"expectedRevision", catalogue.at("revision")}});
    reject([&] {
        call("vcs.worktree.unlock", "one",
             {{"path", "linked"}, {"expectedRevision", catalogue.at("revision")}});
    });
    catalogue = locked.at("catalog");
    reject([&] {
        call("vcs.worktree.remove", "one",
             {{"path", "linked"}, {"expectedRevision", catalogue.at("revision")}});
    });
    catalogue = call("vcs.worktree.unlock", "one",
                     {{"path", "linked"}, {"expectedRevision", catalogue.at("revision")}})
                    .at("catalog");
    std::ofstream(workspace / "linked/untracked.txt") << "preserve\n";
    const auto refused =
        engine.dispatch("vcs.worktree.remove", {{"provider", "git"},
                                                {"repository", "one"},
                                                {"path", "linked"},
                                                {"expectedRevision", catalogue.at("revision")}});
    require(refused.at("exitCode") != 0 && std::filesystem::exists(workspace / "linked/untracked.txt"),
            "Dirty worktree was forced away");
    std::filesystem::remove(workspace / "linked/untracked.txt");
    catalogue = call("vcs.worktrees", "one", Json::object());
    call("vcs.worktree.remove", "one", {{"path", "linked"}, {"expectedRevision", catalogue.at("revision")}});
    git(workspace / "one", {"show-ref", "--verify", "refs/heads/work"});
    git(workspace / "one", {"worktree", "add", "--detach", pathUtf8(root / "external"), "HEAD"});
    const auto external = call("vcs.worktrees", "one", Json::object());
    require(std::any_of(external.at("entries").begin(), external.at("entries").end(),
                        [](const auto &entry) { return !entry.at("insideWorkspace").template get<bool>(); }),
            "External worktree omitted");
    reject([&] {
        call("vcs.worktree.remove", "one",
             {{"path", "../external"}, {"expectedRevision", external.at("revision")}});
    });
    require(std::filesystem::exists(root / "external/file.cpp"), "External worktree changed");
    git(workspace / "one", {"restore", "--", "file.cpp"});
    // Preview and failing preflights only: these cases never need a remote clone.
    std::filesystem::create_directories(workspace / "one/vendor");
    auto modules = call("vcs.submodules", "one", Json::object());
    Json add = {{"url", "file:///cycode-fixture-missing/repository.git"},
                {"path", "vendor/new-module"},
                {"expectedRepository", modules.at("identity")},
                {"expectedRevision", modules.at("revision")}};
    auto refreshModules = [&] {
        modules = call("vcs.submodules", "one", Json::object());
        add["expectedRepository"] = modules.at("identity");
        add["expectedRevision"] = modules.at("revision");
    };
    const auto indexBefore = git(workspace / "one", {"ls-files", "--stage", "-z"});
    const auto preview = call("vcs.submodule.addPlan", "one", add);
    require(preview.at("remoteCommit").is_null() && preview.at("stagesGitmodules") == true &&
                preview.at("request").at("name") == "vendor/new-module" &&
                !std::filesystem::exists(workspace / "one/vendor/new-module") &&
                !std::filesystem::exists(workspace / "one/.gitmodules") &&
                git(workspace / "one", {"ls-files", "--stage", "-z"}) == indexBefore,
            "Submodule preview fabricated a remote commit or changed the checkout/index");
    require(call("vcs.submodule.addPlan", "one", add).at("revision") == preview.at("revision"),
            "An unchanged preview reused volatile index stat data");
    auto staleAdd = add;
    staleAdd["expectedPlanRevision"] = "obsolete-preview";
    reject([&] { call("vcs.submodule.add", "one", staleAdd); });
    git(workspace / "one", {"switch", "-c", "submodule-preview-context"});
    staleAdd["expectedPlanRevision"] = preview.at("revision");
    reject([&] { call("vcs.submodule.add", "one", staleAdd); });
    git(workspace / "one", {"switch", "feature"});
    std::filesystem::create_directory(workspace / "one/vendor/new-module");
    reject([&] { call("vcs.submodule.addPlan", "one", add); });
    std::filesystem::remove(workspace / "one/vendor/new-module");
    std::filesystem::rename(workspace / "one/vendor", workspace / "one/vendor-original");
    std::filesystem::create_directory(workspace / "one/vendor");
    staleAdd["expectedPlanRevision"] = preview.at("revision");
    reject([&] { call("vcs.submodule.add", "one", staleAdd); });
    std::filesystem::remove(workspace / "one/vendor");
    std::filesystem::rename(workspace / "one/vendor-original", workspace / "one/vendor");
    std::filesystem::create_directories(workspace / "one/.git/modules/vendor/new-module");
    reject([&] { call("vcs.submodule.addPlan", "one", add); });
    std::filesystem::remove(workspace / "one/.git/modules/vendor/new-module");
    std::filesystem::remove(workspace / "one/.git/modules/vendor");
    std::filesystem::remove(workspace / "one/.git/modules");
    git(workspace / "one",
        {"config", "submodule.vendor/new-module.url", "https://example.invalid/existing.git"});
    refreshModules();
    reject([&] { call("vcs.submodule.addPlan", "one", add); });
    git(workspace / "one", {"config", "--remove-section", "submodule.vendor/new-module"});
    refreshModules();
    std::ofstream(workspace / "one/vendor/deleted.cpp") << "tracked but absent\n";
    git(workspace / "one", {"add", "vendor/deleted.cpp"});
    std::filesystem::remove(workspace / "one/vendor/deleted.cpp");
    auto trackedAdd = add;
    trackedAdd["path"] = "vendor/deleted.cpp";
    reject([&] { call("vcs.submodule.addPlan", "one", trackedAdd); });
    git(workspace / "one", {"rm", "--cached", "--", "vendor/deleted.cpp"});
    std::ofstream(workspace / "one/.gitmodules") << "# tracked metadata\n";
    refreshModules();
    reject([&] { call("vcs.submodule.addPlan", "one", add); });
    git(workspace / "one", {"add", ".gitmodules"});
    git(workspace / "one", {"commit", "-m", "Plain submodule metadata fixture"});
    git(workspace / "one", {"update-index", "--skip-worktree", ".gitmodules"});
    std::ofstream(workspace / "one/.gitmodules") << "# hidden metadata edit\n";
    require(git(workspace / "one", {"status", "--porcelain=v1", "--", ".gitmodules"}).empty(),
            "Fixture did not hide its working-tree edit");
    refreshModules();
    reject([&] { call("vcs.submodule.addPlan", "one", add); });
    git(workspace / "one", {"update-index", "--no-skip-worktree", ".gitmodules"});
    git(workspace / "one", {"restore", "--", ".gitmodules"});
    refreshModules();
    const auto openedModules = engine.dispatch("documents.read", {{"path", "one/.gitmodules"}});
    reject([&] { call("vcs.submodule.addPlan", "one", add); });
    engine.dispatch("documents.close",
                    {{"path", "one/.gitmodules"}, {"expectedRevision", openedModules.at("revision")}});
    require(call("vcs.submodule.addPlan", "one", add).at("remoteCommit").is_null(),
            "Closing metadata did not permit a read-only preview");
    // Local fixture only, reserved for an explicitly authorized future native run.
    std::ofstream(workspace / "two/.gitignore") << "build.log\n";
    git(workspace / "two", {"add", ".gitignore"});
    git(workspace / "two", {"commit", "-m", "Ignored output fixture"});
    git(workspace / "one", {"-c", "protocol.file.allow=always", "submodule", "add", "--name", "protected",
                            "--", pathUtf8(workspace / "two"), "vendor/protected"});
    git(workspace / "one", {"commit", "-m", "Protected submodule fixture"});
    // Settings fixtures retain pre-existing staged and unstaged metadata changes.
    git(workspace / "one", {"config", "--file", ".gitmodules", "custom.staged", "retained"});
    git(workspace / "one", {"add", ".gitmodules"});
    std::ofstream(workspace / "one/.gitmodules", std::ios::app) << "\n# retain this metadata comment\n";
    git(workspace / "one", {"config", "--file", ".gitmodules", "custom.unstaged", "retained"});
    const auto settingsIndex = git(workspace / "one", {"ls-files", "--stage", "-z", "--", ".gitmodules"});
    Json settings;
    auto refreshSettings = [&](const char *setting, const std::string &value) {
        const auto actual = call("vcs.submodules", "one", Json::object());
        settings = {{"path", "vendor/protected"},
                    {"setting", setting},
                    {"value", value},
                    {"expectedRepository", actual.at("identity")},
                    {"expectedRevision", actual.at("revision")}};
    };
    refreshSettings("branch", "release/stable");
    const auto settingsBytes = readTextFileBytes(workspace / "one/.gitmodules");
    const auto branchPreview = call("vcs.submodule.settingsPlan", "one", settings);
    require(branchPreview.at("previousValue").is_null() && branchPreview.at("hasConfiguredValue") == false &&
                branchPreview.at("changesCheckout") == false &&
                readTextFileBytes(workspace / "one/.gitmodules") == settingsBytes,
            "Tracking-branch preview mutated metadata or invented a configured value");
    settings["expectedPlanRevision"] = "stale-preview";
    reject([&] { call("vcs.submodule.settings", "one", settings); });
    settings["expectedPlanRevision"] = branchPreview.at("revision");
    git(workspace / "one", {"switch", "-c", "settings-preview-context"});
    reject([&] { call("vcs.submodule.settings", "one", settings); });
    git(workspace / "one", {"switch", "feature"});
    require(!call("vcs.submodule.settings", "one", settings).contains("catalogError"),
            "Tracking branch was not confirmed");
    require(git(workspace / "one", {"config", "--file", ".gitmodules", "--get",
                                    "submodule.protected.branch"}) == "release/stable\n" &&
                git(workspace / "one", {"ls-files", "--stage", "-z", "--", ".gitmodules"}) == settingsIndex &&
                readTextFileBytes(workspace / "one/.gitmodules").find("# retain this metadata comment") !=
                    std::string::npos,
            "Tracking branch editing staged changes or lost unrelated metadata");
    git(workspace / "one", {"config", "submodule.protected.branch", "local-override"});
    refreshSettings("branch", ".");
    auto settingsPreview = call("vcs.submodule.settingsPlan", "one", settings);
    require(settingsPreview.at("hasLocalBranchOverride") == true, "Local branch override was concealed");
    settings["expectedPlanRevision"] = settingsPreview.at("revision");
    call("vcs.submodule.settings", "one", settings);
    require(git(workspace / "one", {"config", "--get", "submodule.protected.branch"}) == "local-override\n",
            "Local tracking override was overwritten");
    refreshSettings("branch", "");
    settingsPreview = call("vcs.submodule.settingsPlan", "one", settings);
    settings["expectedPlanRevision"] = settingsPreview.at("revision");
    call("vcs.submodule.settings", "one", settings);
    refreshSettings("branch", "");
    settingsPreview = call("vcs.submodule.settingsPlan", "one", settings);
    require(settingsPreview.at("alreadyConfigured") == true &&
                settingsPreview.at("writesGitmodules") == false,
            "Remote HEAD default fabricated a metadata change");
    git(workspace / "one", {"config", "--unset", "submodule.protected.branch"});
    refreshSettings("url", "https://example.invalid/updated.git");
    git(workspace / "one", {"config", "submodule.protected.gitdir", "unsupported-custom-metadata"});
    refreshSettings("url", "https://example.invalid/updated.git");
    reject([&] { call("vcs.submodule.settingsPlan", "one", settings); });
    git(workspace / "one", {"config", "--unset", "submodule.protected.gitdir"});
    refreshSettings("url", "https://example.invalid/updated.git");
    settingsPreview = call("vcs.submodule.settingsPlan", "one", settings);
    git(workspace / "one", {"update-index", "--skip-worktree", ".gitmodules"});
    reject([&] { call("vcs.submodule.settingsPlan", "one", settings); });
    git(workspace / "one", {"update-index", "--no-skip-worktree", ".gitmodules"});
    const auto settingsDoc = engine.dispatch("documents.read", {{"path", "one/.gitmodules"}});
    settings["expectedPlanRevision"] = settingsPreview.at("revision");
    reject([&] { call("vcs.submodule.settings", "one", settings); });
    engine.dispatch("documents.close",
                    {{"path", "one/.gitmodules"}, {"expectedRevision", settingsDoc.at("revision")}});
    call("vcs.submodule.settings", "one", settings);
    require(git(workspace / "one/vendor/protected", {"config", "--get", "remote.origin.url"}) ==
                    "https://example.invalid/updated.git\n" &&
                git(workspace / "one", {"ls-files", "--stage", "-z", "--", ".gitmodules"}) == settingsIndex,
            "URL editing did not synchronize local config or changed the index");
    git(workspace / "one",
        {"config", "--file", ".gitmodules", "submodule.protected.url", "custom::unsupported"});
    refreshSettings("url", "https://example.invalid/updated.git");
    const auto hiddenPreview = call("vcs.submodule.settingsPlan", "one", settings);
    require(hiddenPreview.at("previousRedacted") == true && hiddenPreview.at("previousValue").is_null(),
            "Unsupported old URL leaked in preview");
    settings["expectedPlanRevision"] = hiddenPreview.at("revision");
    call("vcs.submodule.settings", "one", settings);
    git(workspace / "one", {"add", ".gitmodules"});
    git(workspace / "one", {"commit", "-m", "Reviewed metadata fixture"});
    Json deinit;
    auto refreshDeinit = [&] {
        const auto actual = call("vcs.submodules", "one", Json::object());
        deinit = {{"path", "vendor/protected"},
                  {"expectedRepository", actual.at("identity")},
                  {"expectedRevision", actual.at("revision")}};
    };
    refreshDeinit();
    const auto protectedFolder = workspace / "one/vendor/protected";
    const auto preservedIndex = git(workspace / "one", {"ls-files", "--stage", "-z"});
    const auto preservedMetadata = readTextFileBytes(workspace / "one/.gitmodules");
    const auto deinitPreview = call("vcs.submodule.deinitPlan", "one", deinit);
    require(deinitPreview.at("removesCheckout") == true && deinitPreview.at("preservesRepository") == true &&
                deinitPreview.at("mayContactRemote") == false &&
                std::filesystem::exists(protectedFolder / ".git") &&
                git(workspace / "one", {"ls-files", "--stage", "-z"}) == preservedIndex,
            "Deinitialization preview mutated the checkout/index or concealed its removal effects");
    auto removeCheckout = deinit;
    removeCheckout["expectedPlanRevision"] = "obsolete-preview";
    reject([&] { call("vcs.submodule.deinit", "one", removeCheckout); });
    removeCheckout["expectedPlanRevision"] = deinitPreview.at("revision");
    std::ofstream(protectedFolder / "build.log") << "ignored output must survive\n";
    reject([&] { call("vcs.submodule.deinit", "one", removeCheckout); });
    require(std::filesystem::exists(protectedFolder / "build.log"), "Ignored output was deleted");
    std::filesystem::remove(protectedFolder / "build.log");
    const auto childDoc = engine.dispatch("documents.read", {{"path", "one/vendor/protected/file.cpp"}});
    refreshDeinit();
    reject([&] { call("vcs.submodule.deinitPlan", "one", deinit); });
    engine.dispatch("documents.close", {{"path", "one/vendor/protected/file.cpp"},
                                        {"expectedRevision", childDoc.at("revision")}});
    git(protectedFolder, {"update-index", "--skip-worktree", "file.cpp"});
    std::ofstream(protectedFolder / "file.cpp") << "hidden local changes\n";
    refreshDeinit();
    reject([&] { call("vcs.submodule.deinitPlan", "one", deinit); });
    git(protectedFolder, {"update-index", "--no-skip-worktree", "file.cpp"});
    git(protectedFolder, {"restore", "--", "file.cpp"});
    std::ofstream(protectedFolder / "untracked.txt") << "preserve\n";
    refreshDeinit();
    reject([&] { call("vcs.submodule.deinitPlan", "one", deinit); });
    std::filesystem::remove(protectedFolder / "untracked.txt");
    git(protectedFolder, {"commit", "--allow-empty", "-m", "Unrecorded local child commit"});
    refreshDeinit();
    reject([&] { call("vcs.submodule.deinitPlan", "one", deinit); });
    git(protectedFolder, {"checkout", "--detach", deinitPreview.at("entry").at("expectedCommit")});
    refreshDeinit();
    const auto latestPreview = call("vcs.submodule.deinitPlan", "one", deinit);
    deinit["expectedPlanRevision"] = latestPreview.at("revision");
    const auto removed = call("vcs.submodule.deinit", "one", deinit);
    require(!removed.contains("catalogError") && !std::filesystem::exists(protectedFolder / ".git") &&
                std::filesystem::exists(workspace / "one/.git/modules/protected/HEAD") &&
                readTextFileBytes(workspace / "one/.gitmodules") == preservedMetadata &&
                git(workspace / "one", {"ls-files", "--stage", "-z"}) == preservedIndex,
            "Deinitialization lost parent index/metadata/history or falsely confirmed removal");
    git(workspace / "one", {"switch", "-c", "merge-conflict"});
    std::ofstream(workspace / "one/file.cpp") << "int theirs=3;\n";
    git(workspace / "one", {"add", "file.cpp"});
    git(workspace / "one", {"commit", "-m", "Conflict branch"});
    git(workspace / "one", {"switch", "feature"});
    std::ofstream(workspace / "one/file.cpp") << "int ours=4;\n";
    git(workspace / "one", {"add", "file.cpp"});
    git(workspace / "one", {"commit", "-m", "Conflicting change"});
    const auto merge =
        runProcess({"git", "-C", pathUtf8(workspace / "one"), "-c", "user.name=CyCode test", "-c",
                    "user.email=cycode@example.invalid", "merge", "--no-edit", "merge-conflict"},
                   workspace / "one");
    require(merge.exitCode && !git(workspace / "one", {"ls-files", "--unmerged"}).empty(),
            "Expected a real nested-repository index conflict");
    reject([&] { call("vcs.hunks", "one", {{"path", "one/file.cpp"}}); });
}
