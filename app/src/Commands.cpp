#include "cycode/Commands.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace cycode {
const std::vector<Command> &commands() {
    static const std::vector<Command> value = {
        {"file.new", "New file...", "File"}, {"file.open", "Open solution...", "File"},
        {"file.folder", "Open workspace...", "File"}, {"file.window", "New window", "File"},
        {"file.openWindow", "Open solution in new window...", "File"},
        {"file.save", "Save", "File", CommandContext::document}, {"file.saveAll", "Save all", "File"},
        {"file.close", "Close file", "File", CommandContext::document}, {"file.closeAll", "Close all files", "File"},
        {"file.reopen", "Reopen closed file", "File"}, {"file.exit", "Exit", "File"},
        {"edit.undo", "Undo", "Edit", CommandContext::editor}, {"edit.redo", "Redo", "Edit", CommandContext::editor},
        {"edit.cut", "Cut", "Edit", CommandContext::editor}, {"edit.copy", "Copy", "Edit", CommandContext::editor},
        {"edit.paste", "Paste", "Edit", CommandContext::editor}, {"edit.selectAll", "Select all", "Edit", CommandContext::editor},
        {"edit.duplicate", "Duplicate selection or line", "Edit", CommandContext::editor},
        {"edit.deleteLine", "Delete line", "Edit", CommandContext::editor},
        {"edit.moveUp", "Move line up", "Edit", CommandContext::editor}, {"edit.moveDown", "Move line down", "Edit", CommandContext::editor},
        {"edit.comment", "Toggle line comment", "Edit", CommandContext::editor},
        {"edit.addComment", "Comment lines", "Edit", CommandContext::editor}, {"edit.removeComment", "Uncomment lines", "Edit", CommandContext::editor},
        {"edit.indent", "Indent lines", "Edit", CommandContext::editor}, {"edit.unindent", "Unindent lines", "Edit", CommandContext::editor},
        {"edit.grow", "Extend selection", "Edit", CommandContext::editor}, {"edit.shrink", "Shrink selection", "Edit", CommandContext::editor},
        {"edit.nextOccurrence", "Add next occurrence", "Edit", CommandContext::editor},
        {"edit.allOccurrences", "Select all occurrences", "Edit", CommandContext::editor},
        {"edit.upper", "Selection to uppercase", "Edit", CommandContext::editor}, {"edit.lower", "Selection to lowercase", "Edit", CommandContext::editor},
        {"edit.trim", "Remove trailing whitespace", "Edit", CommandContext::editor},
        {"search.document", "Find in file", "Search", CommandContext::document},
        {"search.replace", "Replace in file", "Search", CommandContext::document},
        {"search.next", "Find next", "Search", CommandContext::document},
        {"search.workspace", "Find in workspace...", "Search"}, {"search.files", "Go to file...", "Search"},
        {"search.everywhere", "Search Everywhere", "Search"}, {"search.actions", "Find action...", "Search"},
        {"search.types", "Go to type...", "Search"}, {"search.symbols", "Go to symbol...", "Search"},
        {"navigate.members", "File structure", "Navigate", CommandContext::document},
        {"refactor.rename", "Rename symbol", "Refactor", CommandContext::editor},
        {"refactor.format", "Format document", "Refactor", CommandContext::editor},
        {"refactor.formatSelection", "Format selection", "Refactor", CommandContext::editor},
        {"refactor.undo", "Undo refactoring", "Refactor"},
        {"refactor.apply", "Apply refactoring", "Refactor"},
        {"navigate.definition", "Go to definition", "Navigate", CommandContext::editor},
        {"navigate.references", "Find references", "Navigate", CommandContext::editor},
        {"navigate.line", "Go to line...", "Navigate", CommandContext::document},
        {"navigate.back", "Navigate back", "Navigate"}, {"navigate.forward", "Navigate forward", "Navigate"},
        {"navigate.recent", "Recent files", "Navigate"},
        {"navigate.nextTab", "Next document", "Navigate", CommandContext::document},
        {"navigate.previousTab", "Previous document", "Navigate", CommandContext::document},
        {"build.run", "Build project", "Build"}, {"build.configure", "Configure", "Build"},
        {"msbuild.rebuild", "Rebuild MSBuild project", "Build"}, {"msbuild.clean", "Clean MSBuild project", "Build"},
        {"msbuild.restore", "Restore MSBuild packages", "Build"}, {"msbuild.evaluate", "Evaluate MSBuild project", "Build"},
        {"msbuild.prepare", "Prepare MSBuild code analysis", "Build"}, {"view.msbuild", "MSBuild", "View"},
        {"debug.start", "Debug / Continue", "Run"}, {"run.start", "Run without debugging", "Run"},
        {"debug.pause", "Pause program", "Run"}, {"debug.stop", "Stop debugging", "Run"},
        {"debug.next", "Step over", "Run"}, {"debug.stepIn", "Step into", "Run"}, {"debug.stepOut", "Step out", "Run"},
        {"debug.breakpoint", "Toggle breakpoint", "Run", CommandContext::document}, {"view.debugger", "Debugger", "View"},
        {"debug.terminal", "Program terminal", "Run"},
        {"unreal.prepare", "Prepare Unreal code analysis", "Build"},
        {"view.project", "Project", "View"}, {"view.search", "Search", "View"}, {"view.build", "Build", "View"},
        {"view.unreal", "Unreal Engine", "View"}, {"view.cmake", "CMake", "View"}, {"view.vcs", "Version control", "View"},
        {"view.problems", "Problems", "View"}, {"view.references", "References", "View"},
        {"view.refactoring", "Refactoring", "View"},
        {"view.terminal", "Terminal", "View"}, {"terminal.new", "New terminal", "View"},
        {"view.extensions", "Extensions", "View"}, {"view.mcp", "MCP", "View"},
        {"view.options", "Options", "View"}, {"view.keymap", "Keyboard shortcuts", "View"}, {"view.reset", "Reset layout", "View"},
        {"view.wrap", "Toggle word wrap", "View", CommandContext::editor},
        {"view.lineNumbers", "Toggle line numbers", "View", CommandContext::editor},
        {"view.whitespace", "Toggle whitespace", "View", CommandContext::editor},
        {"view.minimap", "Toggle minimap", "View", CommandContext::editor},
        {"view.fold", "Fold region", "View", CommandContext::editor}, {"view.unfold", "Unfold region", "View", CommandContext::editor},
        {"view.unfoldAll", "Unfold all", "View", CommandContext::editor},
        {"project.refresh", "Refresh files", "Project"}, {"vcs.refresh", "Refresh version control", "Version control"},
        {"vcs.changes", "Commit changes", "Version control"}, {"vcs.branches", "Git branches", "Version control"},
        {"vcs.history", "Git history", "Version control"}, {"vcs.remotes", "Git remotes and sync", "Version control"},
        {"vcs.stashes", "Git stashes", "Version control"}, {"vcs.conflicts", "Git conflicts", "Version control"},
        {"vcs.diff", "Show file diff", "Version control", CommandContext::document},
        {"vcs.blame", "Annotate file", "Version control", CommandContext::document}
    };
    return value;
}
const Command *findCommand(const std::string &id) {
    for (const auto &command : commands()) if (command.id == id) return &command;
    return nullptr;
}
static std::string trim(std::string value) {
    const auto a = value.find_first_not_of(" \t\r\n"), b = value.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : value.substr(a, b - a + 1);
}
static std::string lowered(std::string value) {
    for (auto &c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
std::vector<std::string> Keymap::parse(const std::string &shortcut) {
    if (shortcut == "Double Shift") return {"DoubleShift"};
    static const std::set<std::string> keys = {"tab", "enter", "escape", "space", "delete", "backspace", "insert", "home", "end", "pageup", "pagedown", "left", "right", "up", "down", "slash", "leftbracket", "rightbracket", "comma", "period", "minus", "equal", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"};
    std::vector<std::string> strokes;
    std::istringstream sequence(shortcut);
    std::string part;
    while (std::getline(sequence, part, ',')) {
        std::set<std::string> modifiers;
        std::string key, token;
        std::istringstream pieces(part);
        while (std::getline(pieces, token, '+')) {
            token = lowered(trim(token));
            if (token == "ctrl" || token == "shift" || token == "alt" || token == "super") {
                if (!modifiers.insert(token).second) throw std::runtime_error("Repeated modifier: " + shortcut);
            } else {
                if (!key.empty() || (!(token.size() == 1 && std::isalnum(static_cast<unsigned char>(token[0]))) && !keys.contains(token)))
                    throw std::runtime_error("Invalid shortcut: " + shortcut);
                key = token;
            }
        }
        if (key.empty()) throw std::runtime_error("Shortcut needs a key: " + shortcut);
        std::string stroke;
        for (const auto &mod : {"ctrl", "shift", "alt", "super"}) if (modifiers.contains(mod)) stroke += std::string(mod) + "+";
        strokes.push_back(stroke + key);
    }
    if (strokes.empty() || strokes.size() > 3 || shortcut.back() == ',') throw std::runtime_error("Invalid shortcut: " + shortcut);
    return strokes;
}
void Keymap::load(const std::string &profile, const Json &overrides) {
    if (profile != "rider" && profile != "visualstudio" && profile != "vscode") throw std::runtime_error("Unknown keymap: " + profile);
    std::vector<KeyBinding> next;
    auto add = [&](const char *id, std::initializer_list<const char *> shortcuts) {
        for (auto shortcut : shortcuts) next.push_back({id, shortcut, parse(shortcut)});
    };
    add("file.open", {"Ctrl+O"}); add("file.folder", {"Ctrl+Alt+O"}); add("file.window", {"Ctrl+Shift+N"});
    add("file.save", {"Ctrl+S"}); add("file.saveAll", {"Ctrl+Shift+S"}); add("file.close", {"Ctrl+F4"});
    add("edit.undo", {"Ctrl+Z"}); add("edit.cut", {"Ctrl+X"}); add("edit.copy", {"Ctrl+C"}); add("edit.paste", {"Ctrl+V"}); add("edit.selectAll", {"Ctrl+A"});
    add("search.document", {"Ctrl+F"}); add("search.workspace", {"Ctrl+Shift+F"}); add("search.files", {"Ctrl+Alt+F"});
    add("search.everywhere", {"Double Shift"}); add("search.next", {"F3"}); add("view.options", {"Ctrl+Alt+S"});
    add("view.terminal", {"Alt+F12"});
    add("navigate.nextTab", {"Ctrl+Tab"}); add("navigate.previousTab", {"Ctrl+Shift+Tab"});
    add("edit.indent", {"Ctrl+RightBracket"}); add("edit.unindent", {"Ctrl+LeftBracket"});
    if (profile == "rider") {
        add("vcs.changes", {"Ctrl+K"}); add("vcs.remotes", {"Ctrl+Shift+K"});
        add("debug.start", {"Shift+F9", "F5", "F9"}); add("run.start", {"Shift+F10"});
        add("debug.stop", {"Ctrl+F2"}); add("debug.next", {"F8"}); add("debug.stepIn", {"F7"}); add("debug.stepOut", {"Shift+F8"}); add("debug.breakpoint", {"Ctrl+F8"});
        add("search.types", {"Ctrl+N"}); add("search.symbols", {"Ctrl+Alt+Shift+N"}); add("navigate.members", {"Ctrl+F12"});
        add("refactor.rename", {"Shift+F6"}); add("refactor.format", {"Ctrl+Alt+L"}); add("refactor.formatSelection", {"Ctrl+Alt+Shift+L"});
        add("file.new", {"Alt+Insert", "Ctrl+Alt+Insert"}); add("file.open", {"Ctrl+Shift+O"});
        add("file.reopen", {"Ctrl+Shift+T"}); add("search.files", {"Ctrl+Shift+N"});
        // Rider owns Ctrl+Shift+N; new windows remain available through File/Find Action.
        std::erase_if(next, [](const auto &b) { return b.command == "file.window"; });
        add("search.actions", {"Ctrl+Shift+A"}); add("search.replace", {"Ctrl+R"});
        add("edit.redo", {"Ctrl+Shift+Z"}); add("edit.duplicate", {"Ctrl+D"}); add("edit.deleteLine", {"Ctrl+Y"});
        add("edit.moveUp", {"Ctrl+Shift+Up"}); add("edit.moveDown", {"Ctrl+Shift+Down"});
        add("edit.comment", {"Ctrl+Slash"}); add("edit.grow", {"Ctrl+W"}); add("edit.shrink", {"Ctrl+Shift+W"});
        add("edit.nextOccurrence", {"Alt+J"}); add("edit.allOccurrences", {"Ctrl+Alt+Shift+J"});
        add("navigate.definition", {"Ctrl+B", "F12"}); add("navigate.references", {"Alt+F7", "Shift+F12"});
        add("navigate.line", {"Ctrl+G"}); add("navigate.recent", {"Ctrl+E"});
        add("navigate.back", {"Ctrl+Alt+Left"}); add("navigate.forward", {"Ctrl+Alt+Right"});
        add("build.run", {"Ctrl+F9", "Ctrl+Shift+B"}); add("view.project", {"Alt+1"}); add("view.build", {"Alt+0"});
        add("view.vcs", {"Alt+9"}); add("view.fold", {"Ctrl+Minus"}); add("view.unfold", {"Ctrl+Equal"});
    } else if (profile == "visualstudio") {
        add("debug.start", {"F5"}); add("run.start", {"Ctrl+F5"}); add("debug.stop", {"Shift+F5"});
        add("debug.next", {"F10"}); add("debug.stepIn", {"F11"}); add("debug.stepOut", {"Shift+F11"}); add("debug.breakpoint", {"F9"});
        add("search.symbols", {"Ctrl+1, Ctrl+S"}); add("search.types", {"Ctrl+1, Ctrl+T"}); add("navigate.members", {"Ctrl+1, Ctrl+M"});
        add("refactor.rename", {"Ctrl+R, Ctrl+R"}); add("refactor.format", {"Ctrl+K, Ctrl+D"}); add("refactor.formatSelection", {"Ctrl+K, Ctrl+F"});
        add("file.new", {"Ctrl+N"}); add("file.open", {"Ctrl+Shift+O"});
        add("file.closeAll", {"Ctrl+Shift+W"}); add("search.files", {"Ctrl+P"}); add("search.actions", {"Ctrl+Q"});
        add("search.replace", {"Ctrl+H"}); add("edit.redo", {"Ctrl+Y", "Ctrl+Shift+Z"});
        add("edit.duplicate", {"Ctrl+D"}); add("edit.deleteLine", {"Ctrl+Shift+L"});
        add("edit.moveUp", {"Alt+Up"}); add("edit.moveDown", {"Alt+Down"});
        add("edit.addComment", {"Ctrl+K, Ctrl+C"}); add("edit.removeComment", {"Ctrl+K, Ctrl+U"});
        add("edit.grow", {"Shift+Alt+Equal"}); add("edit.shrink", {"Shift+Alt+Minus"});
        add("navigate.definition", {"F12"}); add("navigate.references", {"Shift+F12"}); add("navigate.line", {"Ctrl+G"});
        add("navigate.back", {"Ctrl+Minus"}); add("navigate.forward", {"Ctrl+Shift+Minus"});
        add("build.run", {"Ctrl+Shift+B"}); add("view.project", {"Ctrl+Alt+L"}); add("view.build", {"Ctrl+Alt+O"});
        std::erase_if(next, [](const auto &b) { return b.command == "file.folder"; });
        add("view.fold", {"Ctrl+M, Ctrl+M"}); add("view.unfoldAll", {"Ctrl+M, Ctrl+L"});
        add("view.keymap", {"Ctrl+K, Ctrl+S"});
    } else {
        add("debug.start", {"F5"}); add("run.start", {"Ctrl+F5"}); add("debug.stop", {"Shift+F5"});
        add("debug.next", {"F10"}); add("debug.stepIn", {"F11"}); add("debug.stepOut", {"Shift+F11"}); add("debug.breakpoint", {"F9"});
        add("search.symbols", {"Ctrl+T"}); add("navigate.members", {"Ctrl+Shift+O"});
        add("refactor.rename", {"F2"}); add("refactor.format", {"Shift+Alt+F"}); add("refactor.formatSelection", {"Ctrl+K, Ctrl+F"});
        add("file.new", {"Ctrl+N"}); add("file.close", {"Ctrl+W"}); add("file.closeAll", {"Ctrl+K, Ctrl+W"});
        add("file.reopen", {"Ctrl+Shift+T"}); add("search.files", {"Ctrl+P"}); add("search.actions", {"Ctrl+Shift+P", "F1"});
        add("search.replace", {"Ctrl+H"}); add("edit.redo", {"Ctrl+Y", "Ctrl+Shift+Z"});
        add("edit.duplicate", {"Shift+Alt+Down"}); add("edit.deleteLine", {"Ctrl+Shift+K"});
        add("edit.moveUp", {"Alt+Up"}); add("edit.moveDown", {"Alt+Down"}); add("edit.comment", {"Ctrl+Slash"});
        add("edit.addComment", {"Ctrl+K, Ctrl+C"}); add("edit.removeComment", {"Ctrl+K, Ctrl+U"});
        add("edit.grow", {"Shift+Alt+Right"}); add("edit.shrink", {"Shift+Alt+Left"});
        add("edit.nextOccurrence", {"Ctrl+D"}); add("edit.allOccurrences", {"Ctrl+Shift+L"});
        add("navigate.definition", {"F12"}); add("navigate.references", {"Shift+F12"}); add("navigate.line", {"Ctrl+G"});
        add("navigate.back", {"Alt+Left"}); add("navigate.forward", {"Alt+Right"});
        add("build.run", {"Ctrl+Shift+B"}); add("view.project", {"Ctrl+Shift+E"}); add("view.search", {"Ctrl+Shift+F"});
        std::erase_if(next, [](const auto &b) { return b.command == "view.search"; });
        add("view.build", {"Ctrl+Shift+U"}); add("view.vcs", {"Ctrl+Shift+G"}); add("view.problems", {"Ctrl+Shift+M"});
        add("view.keymap", {"Ctrl+K, Ctrl+S"}); add("view.wrap", {"Alt+Z"});
        add("view.fold", {"Ctrl+Shift+LeftBracket"}); add("view.unfold", {"Ctrl+Shift+RightBracket"}); add("view.unfoldAll", {"Ctrl+K, Ctrl+J"});
    }
    if (!overrides.is_object()) throw std::runtime_error("Shortcut overrides must be an object");
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        if (!findCommand(it.key()) || !it.value().is_array()) throw std::runtime_error("Invalid command override: " + it.key());
        std::erase_if(next, [&](const auto &b) { return b.command == it.key(); });
        for (const auto &shortcut : it.value()) {
            auto text = shortcut.get<std::string>(); next.push_back({it.key(), text, parse(text)});
        }
    }
    for (size_t a = 0; a < next.size(); ++a) for (size_t b = a + 1; b < next.size(); ++b) {
        const auto &x = next[a], &y = next[b];
        const auto count = std::min(x.strokes.size(), y.strokes.size());
        if (std::equal(x.strokes.begin(), x.strokes.begin() + count, y.strokes.begin()))
            throw std::runtime_error("Shortcut conflict: " + x.shortcut + " (" + x.command + ", " + y.command + ")");
    }
    bindings_ = std::move(next); cancel();
}
void Keymap::cancel() { candidates_.clear(); step_ = 0; until_ = 0; }
std::string Keymap::pending(double time) const {
    if (time > until_ || candidates_.empty()) return "";
    auto result = bindings_[candidates_[0]].shortcut;
    auto comma = result.find(',');
    return result.substr(0, comma) + ", ...";
}
Keymap::Result Keymap::press(const std::string &stroke, double time, bool editor, bool document) {
    if (time > until_) cancel();
    if (stroke == "escape" && !candidates_.empty()) { cancel(); return {{}, true}; }
    auto allowed = [&](const KeyBinding &b) {
        const auto &command = *findCommand(b.command);
        if (command.context == CommandContext::global) return true;
        if (!document) return false;
        return command.context != CommandContext::editor || editor;
    };
    if (!candidates_.empty()) {
        std::vector<size_t> matching;
        for (auto i : candidates_) if (allowed(bindings_[i]) && bindings_[i].strokes[step_] == stroke) matching.push_back(i);
        if (matching.empty()) { cancel(); return {{}, true}; }
        candidates_ = std::move(matching); ++step_;
    } else {
        for (size_t i = 0; i < bindings_.size(); ++i) if (allowed(bindings_[i]) && bindings_[i].strokes[0] == stroke) candidates_.push_back(i);
        if (candidates_.empty()) return {};
        step_ = 1;
    }
    const auto &first = bindings_[candidates_[0]];
    if (step_ == first.strokes.size()) { const auto id = first.command; cancel(); return {id, true}; }
    until_ = time + 1.5;
    return {{}, true};
}
std::string Keymap::hint(const std::string &command) const {
    for (const auto &b : bindings_) if (b.command == command) return b.shortcut;
    return "";
}
int fuzzyScore(const std::string &query, const std::string &candidate) {
    if (query.empty()) return 0;
    const auto q = lowered(query), c = lowered(candidate);
    size_t at = 0; int score = 0, run = 0;
    for (size_t i = 0; i < c.size() && at < q.size(); ++i) {
        if (c[i] == q[at]) {
            score += 10 + (++run * 4);
            if (i == 0 || c[i-1] == '/' || c[i-1] == '_' || c[i-1] == ' ' || c[i-1] == '.') score += 25;
            ++at;
        } else run = 0;
    }
    if (at != q.size()) return -1;
    if (auto slash = c.find_last_of('/'); slash != std::string::npos && c.substr(slash + 1).starts_with(q)) score += 150;
    if (c.starts_with(q)) score += 100;
    return std::max(0, score - static_cast<int>(std::min<size_t>(c.size(), 1000)) / 4);
}
} // namespace cycode
