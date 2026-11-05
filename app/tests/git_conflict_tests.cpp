#include "cycode/Engine.h"
#include <fstream>
#include <stdexcept>
namespace cycode {
void gitStructuralConflictContracts(const std::filesystem::path &root) {
    const auto folder = root / "structural-conflicts";
    std::filesystem::create_directories(folder);
    auto require = [](bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    };
    auto git = [&](std::vector<std::string> args, bool success = true) {
        args.insert(args.begin(),
                    {"git", "-C", pathUtf8(folder), "-c", "core.autocrlf=false", "-c", "commit.gpgsign=false",
                     "-c", "user.name=CyCode fixture", "-c", "user.email=fixture@example.invalid"});
        const auto result = runProcess(args, folder);
        if (success && result.exitCode)
            throw std::runtime_error(result.output);
        return result;
    };
    auto write = [&](const char *name, const std::string &bytes) {
        std::ofstream(folder / name, std::ios::binary) << bytes;
    };
    auto read = [&](const char *name) {
        std::ifstream file(folder / name, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    git({"init", "-b", "main"});
    write("binary.dat", std::string("base\0binary", 11));
    write("deleted.cpp", "base\n");
    write("original.cpp", "rename identity\n");
    git({"add", "--", "."});
    git({"commit", "-m", "Base"});
    git({"checkout", "-b", "other"});
    write("binary.dat", std::string("theirs\0binary", 13));
    git({"rm", "--", "deleted.cpp"});
    git({"mv", "--", "original.cpp", "right.cpp"});
    git({"add", "--", "."});
    git({"commit", "-m", "Their binary deletion and rename"});
    git({"checkout", "main"});
    write("binary.dat", std::string("ours\0binary", 11));
    write("deleted.cpp", "ours\n");
    git({"mv", "--", "original.cpp", "left.cpp"});
    git({"add", "--", "."});
    git({"commit", "-m", "Our binary modification and rename"});
    require(git({"merge", "--no-edit", "other"}, false).exitCode != 0,
            "Fixture must produce structural conflicts");
    Engine engine(folder);
    const auto catalog = engine.dispatch("vcs.conflict.list");
    require(catalog.at("entries").size() == 5,
            "Binary, modify/delete and rename/rename retain five actual index paths");
    const auto binary = engine.dispatch(
        "vcs.conflict.inspect", {{"path", "binary.dat"}, {"expectedRevision", catalog.at("revision")}});
    require(binary.at("versions").at("ours").at("kind") == "binary" &&
                binary.at("versions").at("ours").at("text").is_null(),
            "Binary bytes never enter the text model");
    const auto deleted = engine.dispatch(
        "vcs.conflict.inspect", {{"path", "deleted.cpp"}, {"expectedRevision", catalog.at("revision")}});
    require(deleted.at("versions").at("theirs").at("kind") == "absent",
            "Deleted side remains absent, not an empty text file");
    auto doc = engine.dispatch("documents.read", {{"path", "deleted.cpp"}});
    doc = engine.dispatch(
        "documents.replace",
        {{"path", "deleted.cpp"}, {"expectedRevision", doc.at("revision")}, {"text", "unsaved user text\n"}});
    const auto disk = read("binary.dat"), workingDelete = read("deleted.cpp");
    const auto choices = Json::array({{{"path", "binary.dat"}, {"choice", "theirs"}},
                                      {{"path", "deleted.cpp"}, {"choice", "theirs"}},
                                      {{"path", "original.cpp"}, {"choice", "delete"}},
                                      {{"path", "left.cpp"}, {"choice", "ours"}},
                                      {{"path", "right.cpp"}, {"choice", "delete"}}});
    const Json request = {{"choices", choices},
                          {"expectedRepository", catalog.at("identity")},
                          {"expectedRevision", catalog.at("revision")}};
    auto plan = engine.dispatch("vcs.conflict.plan", request);
    auto apply = request;
    apply["expectedPlanRevision"] = plan.at("revision");
    write("unrelated.cpp", "retained staged change\n");
    git({"add", "--", "unrelated.cpp"});
    bool stale = false;
    try {
        engine.dispatch("vcs.conflict.apply", apply);
    } catch (const std::exception &) {
        stale = true;
    }
    require(stale && !git({"ls-files", "-u"}).output.empty(),
            "An unrelated index edit invalidates preview before mutation");
    const auto current = engine.dispatch("vcs.conflict.list");
    apply["expectedRevision"] = current.at("revision");
    auto revised = apply;
    revised.erase("expectedPlanRevision");
    plan = engine.dispatch("vcs.conflict.plan", revised);
    apply["expectedPlanRevision"] = plan.at("revision");
    const auto result = engine.dispatch("vcs.conflict.apply", apply);
    require(result.at("exitCode") == 0 && !result.contains("catalogError") &&
                result.at("catalog").at("entries").empty(),
            "Batch verifies actual resolved index");
    require(git({"show", ":binary.dat"}).output == std::string("theirs\0binary", 13),
            "Exact binary bytes selected without conversion");
    require(read("binary.dat") == disk && read("deleted.cpp") == workingDelete &&
                std::filesystem::exists(folder / "right.cpp"),
            "Files are retained even for indexed deletions");
    require(engine.dispatch("documents.read", {{"path", "deleted.cpp"}}) == doc,
            "Dirty buffer and revision remain unchanged");
    require(git({"show", ":unrelated.cpp"}).output == "retained staged change\n",
            "Unselected staged record survives resolution");
    const auto indexBefore = git({"ls-files", "--stage", "-z"}).output;
    auto preview = [&](const char *path, const char *action) {
        return engine.dispatch(
            "vcs.checkout.plan",
            {{"path", path}, {"action", action}, {"expectedRepository", catalog.at("identity")}});
    };
    auto execute = [&](const char *method, const Json &review) {
        return engine.dispatch(method, {{"id", review.at("id")},
                                        {"expectedPlanRevision", review.at("revision")},
                                        {"expectedRepository", catalog.at("identity")}});
    };
    auto working = preview("binary.dat", "materialize");
    require(!working.at("blocked").get<bool>() && working.at("replacementBytes") == 13,
            "Binary materialization preview stays outside text buffers");
    auto installed = execute("vcs.checkout.apply", working);
    require(installed.at("state") == "applied" && !installed.contains("inspectionError") &&
                read("binary.dat") == std::string("theirs\0binary", 13),
            "Exact binary bytes are installed");
    require(!installed.at("backup").get<std::string>().empty() &&
                git({"ls-files", "--stage", "-z"}).output == indexBefore,
            "Persistent original backup and unchanged index");
    const auto journalPath = ".cycode/trash/" + working.at("id").get<std::string>() + "/install.json";
    const auto journal = engine.dispatch("documents.read", {{"path", journalPath}});
    require(preview("binary.dat", "materialize").at("blocked").get<bool>(),
            "Open managed backup metadata blocks another working-file action");
    engine.dispatch("documents.close", {{"path", journalPath}, {"expectedRevision", journal.at("revision")}});
    bool genericRejected = false;
    try {
        engine.dispatch("resources.undo", {{"id", working.at("id")}});
    } catch (const std::exception &) {
        genericRejected = true;
    }
    require(genericRejected, "Generic resource API cannot bypass the Git-owned install contract");
    write("binary.dat", "newer user bytes");
    bool undoRejected = false;
    try {
        execute("vcs.checkout.undo", working);
    } catch (const std::exception &) {
        undoRejected = true;
    }
    require(undoRejected && read("binary.dat") == "newer user bytes",
            "Undo refuses to overwrite newer bytes");
    write("binary.dat", std::string("theirs\0binary", 13));
    const auto undone = execute("vcs.checkout.undo", working);
    require(undone.at("state") == "undone" && read("binary.dat") == disk &&
                !undone.at("undoBackup").get<std::string>().empty(),
            "Undo restores the exact original and retains displaced bytes");
    const auto blocked = preview("deleted.cpp", "remove");
    require(blocked.at("blocked").get<bool>(), "Dirty/open document blocks physical removal");
    bool blockedRejected = false;
    try {
        execute("vcs.checkout.apply", blocked);
    } catch (const std::exception &) {
        blockedRejected = true;
    }
    require(blockedRejected && read("deleted.cpp") == workingDelete,
            "Blocked removal preserves disk content");
    const auto right = read("right.cpp");
    working = preview("right.cpp", "remove");
    installed = execute("vcs.checkout.apply", working);
    require(installed.at("state") == "applied" && !std::filesystem::exists(folder / "right.cpp"),
            "Explicit absent-index removal moves to persistent trash");
    require(execute("vcs.checkout.undo", working).at("state") == "undone" && read("right.cpp") == right,
            "Physical deletion is reversible");
    working = preview("binary.dat", "materialize");
    write("unrelated.cpp", "a newer index version\n");
    git({"add", "--", "unrelated.cpp"});
    bool indexRejected = false;
    try {
        execute("vcs.checkout.apply", working);
    } catch (const std::exception &) {
        indexRejected = true;
    }
    require(indexRejected && read("binary.dat") == disk,
            "A changed full index invalidates materialization preview");
    std::filesystem::remove(folder / "unrelated.cpp");
    working = preview("unrelated.cpp", "materialize");
    require(execute("vcs.checkout.apply", working).at("state") == "applied" &&
                read("unrelated.cpp") == "a newer index version\n",
            "Missing indexed file is materialized exclusively");
    require(execute("vcs.checkout.undo", working).at("state") == "undone" &&
                !std::filesystem::exists(folder / "unrelated.cpp"),
            "Undo of a creation retains its displaced payload in trash");
}
} // namespace cycode
