#include "cycode/GitMerge.h"
#include "cycode/GitReplay.h"
#include <stdexcept>
namespace cycode {
using Json = nlohmann::json;
Json gitReplayRequest(const Json &params) {
    const auto action = params.at("action").get<std::string>();
    const auto commit = params.value("commit", std::string{});
    if (action != "cherryPick" && action != "revert" && action != "continue" && action != "quit")
        throw std::runtime_error("Unknown Git replay action");
    if (action == "cherryPick" || action == "revert") {
        const auto heads = parseGitMergeHeads(commit);
        if (heads.size() != 1 || heads[0].get<std::string>() != commit)
            throw std::runtime_error("Choose one complete commit object ID, without ranges or options");
    } else if (!commit.empty())
        throw std::runtime_error("Continue/quit use the active replay commit, not a replacement");
    return {{"action", action}, {"commit", commit}};
}
Json gitReplayState(const Json &base, const std::string &operationCommit) {
    const auto &operations = base.at("otherOperations");
    const bool pending = base.at("kind") != "merge" && operations.size() == 1 &&
                         (operations[0] == "cherry-pick" || operations[0] == "revert");
    const auto kind = pending                     ? (operations[0] == "cherry-pick" ? "cherryPick" : "revert")
                      : base.at("kind") == "none" ? "none"
                                                  : "other";
    if (pending) {
        const auto parsed = parseGitMergeHeads(operationCommit);
        if (parsed.size() != 1 || parsed[0].get<std::string>() != operationCommit)
            throw std::runtime_error("Active Git replay marker is invalid");
    } else if (!operationCommit.empty())
        throw std::runtime_error("Ambiguous Git replay operation");
    Json start = Json::array(), resume = Json::array(), quit = Json::array();
    auto all = [&](const char *code) {
        start.push_back(code);
        resume.push_back(code);
        quit.push_back(code);
    };
    if (base.at("metadataDocuments").get<size_t>())
        all("metadataOpen");
    if (base.at("autostash").get<bool>())
        all("autostash");
    if (kind != std::string("none"))
        start.push_back("activeOperation");
    if (!pending) {
        resume.push_back("notSingleReplay");
        quit.push_back("notSingleReplay");
    }
    auto writing = [&](const char *code) {
        start.push_back(code);
        resume.push_back(code);
    };
    if (base.at("head").get<std::string>().empty())
        all("unborn");
    if (base.at("openDocuments").get<size_t>())
        writing("openDocuments");
    if (base.at("workingDirty").get<bool>())
        writing("workingChanges");
    if (base.at("hiddenIndexFlags").get<bool>())
        writing("hiddenIndexFlags");
    if (!base.at("conflicts").empty())
        writing("unmerged");
    if (!base.at("stagedPaths").empty())
        start.push_back("stagedChanges");
    Json result;
    for (const auto *key : {"repository", "identity", "revision", "head", "branch", "otherOperations",
                            "conflicts", "stagedPaths", "stagedSummary", "workingDirty", "hiddenIndexFlags",
                            "dirtyBuffers", "metadataDocuments", "openDocuments", "autostash"})
        result[key] = base.at(key);
    result.update({{"kind", kind},
                   {"operationCommit", operationCommit},
                   {"canStart", start.empty()},
                   {"canContinue", resume.empty()},
                   {"canQuit", quit.empty()},
                   {"startBlockers", start},
                   {"continueBlockers", resume},
                   {"quitBlockers", quit}});
    return result;
}
} // namespace cycode
