#include "cycode/GitSubmodules.h"
#include <functional>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void rejects(const std::function<void()> &action) {
    try {
        action();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Invalid submodule metadata or action was accepted");
}
std::string nul(const std::string &text) {
    return text + std::string(1, '\0');
}
} // namespace
void gitSubmoduleContracts() {
    using cycode::gitSubmoduleAddModel;
    using cycode::gitSubmoduleArguments;
    using cycode::gitSubmoduleSettingsModel;
    using cycode::parseGitSubmodules;
    const Json addition = {{"url", "https://example.invalid/team/library.git"}, {"path", "vendor/outil été"}};
    const auto plan = gitSubmoduleAddModel(addition);
    require(plan.at("request").at("name") == "vendor/outil été" && plan.at("request").at("branch") == "" &&
                plan.at("arguments") ==
                    Json::array({"submodule", "add", "--name", "vendor/outil été", "--",
                                 "https://example.invalid/team/library.git", "vendor/outil été"}),
            "Default add plan lost explicit name, Unicode destination or argv separation");
    auto named = addition;
    named["name"] = "library";
    named["branch"] = "release/stable";
    require(gitSubmoduleAddModel(named).at("arguments") ==
                Json::array({"submodule", "add", "--name", "library", "--branch", "release/stable", "--",
                             "https://example.invalid/team/library.git", "vendor/outil été"}),
            "Named branch add changed the source, branch or command boundaries");
    for (const auto *url : {"git@example.invalid:team/repo.git", "ssh://git@example.invalid/team/repo.git",
                            "ssh://git@[::1]/team/repo.git", "git@[::1]:team/repo.git", "../library.git",
                            "file:///workspace/library.git", "D:\\repositories\\library.git",
                            "https://example.invalid/repo;echo$(test).git"}) {
        auto valid = addition;
        valid["url"] = url;
        require(gitSubmoduleAddModel(valid).at("request").at("url") == url,
                "A supported source address was rewritten or interpreted as shell text");
        require(gitSubmoduleSettingsModel({{"path", "vendor/outil été"}, {"setting", "url"}, {"value", url}})
                        .at("arguments") ==
                    Json::array({"submodule", "set-url", "--quiet", "--", "vendor/outil été", url}),
                "URL editing diverged from source validation or invented command parsing");
    }
    for (const auto &change :
         {Json{{"url", ""}}, Json{{"url", "-option"}}, Json{{"url", "ext::helper command"}},
          Json{{"url", "custom::source"}}, Json{{"url", "https://user:example@example.invalid/repo.git"}},
          Json{{"url", "https://example.invalid/repo?token=example"}},
          Json{{"url", "https://example.invalid/repo#fragment"}},
          Json{{"url", "ssh://user:example@example.invalid/repo.git"}},
          Json{{"url", "https://example.invalid/repo\n"}}, Json{{"url", std::string(4097, 'a')}},
          Json{{"path", "../outside"}}, Json{{"path", ".git/modules/x"}}, Json{{"name", "../x"}},
          Json{{"name", std::string(513, 'a')}}, Json{{"branch", "-option"}}, Json{{"branch", "main\nother"}},
          Json{{"branch", "@{-1}"}}, Json{{"branch", false}}}) {
        auto invalid = addition;
        invalid.update(change);
        rejects([&] { gitSubmoduleAddModel(invalid); });
        if (change.contains("url"))
            rejects([&] {
                gitSubmoduleSettingsModel(
                    {{"path", "vendor/outil été"}, {"setting", "url"}, {"value", change.at("url")}});
            });
    }
    for (const auto *key : {"url", "path"}) {
        auto missing = addition;
        missing.erase(key);
        rejects([&] { gitSubmoduleAddModel(missing); });
    }
    auto newline = addition;
    newline["path"] = "vendor/line\nname";
    newline["name"] = "safe-name";
    require(gitSubmoduleAddModel(newline).at("arguments").back() == newline.at("path"),
            "A path newline became a command separator");
    auto optionPath = addition;
    optionPath["path"] = "-name";
    require(gitSubmoduleAddModel(optionPath).at("arguments").back() == "-name",
            "The path escaped the explicit option terminator");
    for (const auto *branch : {"release/stable", ".", ""}) {
        const auto settings = gitSubmoduleSettingsModel(
            {{"path", "vendor/outil été"}, {"setting", "branch"}, {"value", branch}});
        const auto expected =
            std::string(branch).empty()
                ? Json::array({"submodule", "set-branch", "--default", "--", "vendor/outil été"})
                : Json::array({"submodule", "set-branch", "--branch", branch, "--", "vendor/outil été"});
        require(settings.at("arguments") == expected,
                "Tracking branch default/dot/literal semantics changed");
    }
    for (const auto &branch :
         {std::string("-option"), std::string("main\nother"), std::string("@{-1}"), std::string(257, 'a')})
        rejects([&] {
            gitSubmoduleSettingsModel({{"path", "vendor/x"}, {"setting", "branch"}, {"value", branch}});
        });
    rejects([&] {
        gitSubmoduleSettingsModel({{"path", "../outside"}, {"setting", "branch"}, {"value", "main"}});
    });
    rejects([&] {
        gitSubmoduleSettingsModel({{"path", "vendor/x"}, {"setting", "command"}, {"value", "main"}});
    });
    const auto originalSettings = nul("submodule.tool.path\nvendor/tool") + nul("submodule.tool.url\nold") +
                                  nul("custom.keep\nline\nvalue") + nul("custom.keep\nanother") +
                                  nul("custom.flag");
    const auto changedSettings = nul("submodule.tool.path\nvendor/tool") + nul("custom.keep\nline\nvalue") +
                                 nul("submodule.tool.url\nnew") + nul("custom.keep\nanother") +
                                 nul("custom.flag");
    require(cycode::gitSubmoduleSettingsRemainder(originalSettings, "submodule.tool.url") ==
                cycode::gitSubmoduleSettingsRemainder(changedSettings, "submodule.tool.url"),
            "A setting edit lost duplicate/newline/valueless unrelated configuration records");
    require(cycode::gitSubmoduleSettingsRemainder(originalSettings, "submodule.tool.url") !=
                cycode::gitSubmoduleSettingsRemainder(changedSettings + nul("custom.new\nvalue"),
                                                      "submodule.tool.url"),
            "Unrelated configuration changes were concealed");
    const auto reorderedSettings = nul("submodule.tool.path\nvendor/tool") + nul("custom.keep\nanother") +
                                   nul("custom.keep\nline\nvalue") + nul("custom.flag");
    require(cycode::gitSubmoduleSettingsRemainder(originalSettings, "submodule.tool.url") !=
                reorderedSettings,
            "Duplicate configuration record priority changed silently");
    rejects([&] { cycode::gitSubmoduleSettingsRemainder("missing terminator", "submodule.tool.url"); });
    const auto path = std::string("plugins/outil été");
    const auto configuration =
        nul("submodule.tool.path\n" + path) + nul("submodule.engine.core.path\nengine/core");
    const auto index = nul("160000 " + std::string(40, 'a') + " 0\t" + path) +
                       nul("160000 " + std::string(64, 'b') + " 0\tengine/core");
    const auto entries = parseGitSubmodules(configuration, index);
    require(entries.size() == 2 && entries[0].at("name") == "tool" && entries[0].at("path") == path &&
                entries[0].at("expectedCommit") == std::string(40, 'a') &&
                entries[1].at("expectedCommit") == std::string(64, 'b'),
            "NUL parser lost names, whitespace, Unicode or object-format identity");
    require(parseGitSubmodules("", "").empty(), "An empty repository fabricated submodules");
    require(parseGitSubmodules(configuration, "")[0].at("state") == "notIndexed",
            "A configured folder fabricated a gitlink");
    const auto conflict = nul("160000 " + std::string(40, 'a') + " 1\t" + path) +
                          nul("160000 " + std::string(40, 'b') + " 2\t" + path);
    require(parseGitSubmodules(configuration, conflict)[0].at("state") == "conflicted" &&
                parseGitSubmodules(configuration, conflict)[0].at("expectedCommit") == "",
            "Conflicts fabricated an update commit");
    require(parseGitSubmodules(nul("submodule.x.path\nline\nname"),
                               nul("160000 " + std::string(40, 'a') + " 0\tline\nname"))[0]
                    .at("path") == "line\nname",
            "A newline in a config value/path was confused with a record separator");
    for (const auto &bad : {"", ".", "..", "../outside", "/absolute", "C:/outside", "a\\b", "a//b", "a/./b",
                            "a/../b", "a/", ".git/mod", "x/.GIT/mod", ".cycode/x", "a./b", "a /b"})
        rejects([&] { parseGitSubmodules(nul(std::string("submodule.x.path\n") + bad), ""); });
    rejects([&] { parseGitSubmodules(nul("submodule.x.path\na") + nul("submodule.x.path\nb"), ""); });
    rejects([&] { parseGitSubmodules(nul("submodule.x.path\na") + nul("submodule.y.path\nA"), ""); });
    rejects([&] { parseGitSubmodules(nul("submodule.x.path\na") + nul("submodule.y.path\na/b"), ""); });
    rejects([&] { parseGitSubmodules(nul("submodule.x.path\na") + nul("submodule.y.path\nA/b"), ""); });
    rejects([&] { parseGitSubmodules("submodule.x.path\na", ""); });
    rejects([&] { parseGitSubmodules(nul("other.x.path\na"), ""); });
    rejects([&] { parseGitSubmodules(configuration, index + index); });
    rejects([&] { parseGitSubmodules(configuration, index + conflict); });
    rejects(
        [&] { parseGitSubmodules(configuration, nul("160000 " + std::string(40, '0') + " 0\t" + path)); });
    rejects(
        [&] { parseGitSubmodules(configuration, nul("160000 " + std::string(40, 'a') + " 4\t" + path)); });
    rejects([&] { parseGitSubmodules(configuration, std::string(2 * 1024 * 1024 + 1, 'x')); });
    std::string crowded;
    for (int i = 0; i < 65; ++i)
        crowded += nul("submodule.n" + std::to_string(i) + ".path\np" + std::to_string(i));
    rejects([&] { parseGitSubmodules(crowded, ""); });
    require(gitSubmoduleArguments("init", path) ==
                std::vector<std::string>({"submodule", "init", "--", path}),
            "Init invented a checkout or shell string");
    require(gitSubmoduleArguments("sync", path) ==
                std::vector<std::string>({"submodule", "sync", "--", path}),
            "Sync changed more than the selected module");
    require(gitSubmoduleArguments("update", path) ==
                std::vector<std::string>(
                    {"submodule", "update", "--init", "--checkout", "--jobs=1", "--no-fetch", "--", path}),
            "Offline update lost explicit checkout or transport option");
    require(
        gitSubmoduleArguments("update", path, true) ==
            std::vector<std::string>({"submodule", "update", "--init", "--checkout", "--jobs=1", "--", path}),
        "Explicit clone/fetch changed the destination or checkout mode");
    require(gitSubmoduleArguments("update", "-name;echo $(test)").back() == "-name;echo $(test)",
            "A path was interpreted as command text");
    require(gitSubmoduleArguments("deinit", path) ==
                std::vector<std::string>({"submodule", "deinit", "--quiet", "--", path}),
            "Deinitialization lost quiet output or invented force/bulk removal");
    for (const auto *action : {"foreach", "remove", "remote", "rebase", "merge", "update --force"})
        rejects([&] { gitSubmoduleArguments(action, path); });
    rejects([&] { gitSubmoduleArguments("init", path, true); });
    rejects([&] { gitSubmoduleArguments("sync", path, true); });
    rejects([&] { gitSubmoduleArguments("deinit", path, true); });
    rejects([&] { gitSubmoduleArguments("update", std::string("a\0b", 3)); });
    const Json deinitEntry = {{"name", "library"},
                              {"path", path},
                              {"state", "matching"},
                              {"registered", true},
                              {"checkedOut", true},
                              {"dirty", false},
                              {"head", std::string(40, 'a')},
                              {"expectedCommit", std::string(40, 'a')}};
    const auto childIndex = nul("100644 " + std::string(40, 'a') + " 0\tfile\nname.cpp");
    const auto childFlags = nul("H file\nname.cpp");
    const auto deinit = cycode::gitSubmoduleDeinitModel(deinitEntry, childIndex, childFlags);
    require(deinit.at("trackedFiles") == 1 && deinit.at("arguments") == gitSubmoduleArguments("deinit", path),
            "Deinitialization lost NUL-delimited paths, file counts or exact argv");
    for (const auto *flag : {"h", "S", "s", "M", "m", "?"})
        rejects([&] {
            cycode::gitSubmoduleDeinitModel(deinitEntry, childIndex,
                                            nul(std::string(flag) + " file\nname.cpp"));
        });
    for (const auto &badIndex : {nul("160000 " + std::string(40, 'a') + " 0\tfile\nname.cpp"),
                                 nul("100644 " + std::string(40, 'a') + " 1\tfile\nname.cpp"),
                                 nul("100644X" + std::string(40, 'a') + " 0\tfile\nname.cpp"),
                                 childIndex + childIndex, std::string("missing terminator")})
        rejects([&] { cycode::gitSubmoduleDeinitModel(deinitEntry, badIndex, childFlags); });
    rejects([&] { cycode::gitSubmoduleDeinitModel(deinitEntry, childIndex, ""); });
    rejects([&] { cycode::gitSubmoduleDeinitModel(deinitEntry, childIndex, childFlags + childFlags); });
    for (const auto &change : {Json{{"dirty", true}}, Json{{"dirty", nullptr}}, Json{{"registered", false}},
                               Json{{"checkedOut", false}}, Json{{"state", "different"}},
                               Json{{"name", "../outside"}}, Json{{"head", std::string(40, 'b')}}}) {
        auto unsafe = deinitEntry;
        unsafe.update(change);
        rejects([&] { cycode::gitSubmoduleDeinitModel(unsafe, childIndex, childFlags); });
    }
}
