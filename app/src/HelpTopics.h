#pragma once
#include <string_view>

namespace app_help {
struct Topic {
    const char *id;
    const char *page;
    const char *anchor;
};
// Stable addresses shared by panel buttons and Context help. The documentation
// verifier checks these destinations in both editions and covers literal panels.
inline constexpr Topic topics[] = {
    {"search", "search", "find"},
    {"problems", "diagnostics", "problems"},
    {"analysis", "analysis-progress", "persistence"},
    {"references", "symbols", "references"},
    {"information", "languages", "services"},
    {"structure", "symbols", "structure"},
    {"hierarchy", "symbols", "hierarchy"},
    {"occurrences", "symbols", "references"},
    {"codeActions", "refactoring", "rename"},
    {"refactoring", "refactoring", "rename"},
    {"recovery", "recovery", "recover"},
    {"resources", "workspace-files", "files"},
    {"project", "quickstart", "first-work"},
    {"build", "build", "output"},
    {"cmake", "build", "configure"},
    {"cmakePresets", "build", "configure"},
    {"msbuild", "build", "configure"},
    {"projectTasks", "project-tasks", "tasks"},
    {"unreal", "unreal", "engine"},
    {"tests", "testing", "tests"},
    {"debugger", "debugging", "inspection"},
    {"terminal", "terminal", "terminal"},
    {"vcs", "version-control", "providers"},
    {"extensions", "plugins", "extensions"},
    {"mcp", "mcp", "connection"},
    {"updates", "updates", "updates"},
    {"options", "settings", "appearance"},
};
constexpr Topic context(std::string_view id) {
    for (const auto &entry : topics)
        if (id == entry.id) return entry;
    if (id.starts_with("document:")) return {"", "editing", "documents"};
    if (id.starts_with("extension:")) return {"", "plugins", "extensions"};
    return {"", "overview", "getting-started"};
}
inline const char *topic(std::string_view id) { return context(id).page; }
inline const char *topicAnchor(std::string_view id) { return context(id).anchor; }
} // namespace app_help
