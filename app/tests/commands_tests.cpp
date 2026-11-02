#include "cycode/Commands.h"
#include <iostream>
using namespace cycode;
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        Keymap map;
        for (const auto &profile : {"rider", "visualstudio", "vscode"}) {
            map.load(profile);
            require(map.press("ctrl+shift+f", 1, false, false).command == "search.workspace", "Global project search");
            require(map.press("ctrl+alt+f", 2, false, false).command == "search.files", "Explicit file search");
            require(map.press("ctrl+f", 3, false, true).command == "search.document", "Find from a tool window");
            require(map.press("ctrl+s", 4, false, true).command == "file.save", "Save from a tool window");
            require(map.press("ctrl+f", 5, true, false).command.empty(), "No document means no document search");
            require(map.press("ctrl+shift+space",6,true,true).command=="code.parameters","Explicit parameter information");
            const auto action=std::string(profile)=="rider" ? "alt+enter" : "ctrl+period";
            require(map.press(action,6.1,true,true).command=="code.actions","Code actions shortcut");
            require(map.press(action,6.2,false,true).command.empty(),"Code actions must not run while typing in a tool window");
            if (std::string(profile)!="rider") {
                map.press("ctrl+k",7,true,true);
                require(map.press("ctrl+i",7.2,true,true).command=="code.documentation","VS/VS Code quick information chord");
            }
        }
        map.load("rider");
        require(map.press("ctrl+q",.01,true,true).command=="code.documentation","Rider quick documentation");
        require(map.press("ctrl+p",.02,true,true).command=="code.parameters","Rider parameter information");
        require(map.press("ctrl+n", .1, false, false).command == "search.types", "Rider type navigation");
        require(map.press("shift+f6", .2, true, true).command == "refactor.rename", "Rider rename");
        require(map.press("ctrl+alt+l", .3, true, true).command == "refactor.format", "Rider formatting overrides widget binding");
        require(map.press("ctrl+y", 1, true, true).command == "edit.deleteLine", "Rider Ctrl+Y deletes line");
        require(map.press("ctrl+b", 2, true, true).command == "navigate.definition", "Rider definition must not build");
        require(map.press("ctrl+d", 3, false, true).command.empty(), "Do not duplicate code while typing in a tool window");
        map.load("vscode");
        require(map.press("ctrl+d", 1, true, true).command == "edit.nextOccurrence", "VS Code multicursor mapping");
        auto prefix = map.press("ctrl+k", 2, true, true);
        require(prefix.consumed && prefix.command.empty() && !map.pending(2.1).empty(), "Wait for chord second stroke");
        require(map.press("ctrl+c", 2.3, true, true).command == "edit.addComment", "Comment chord");
        map.press("ctrl+k", 3, true, true);
        require(map.press("ctrl+u", 3.2, true, true).command == "edit.removeComment", "Uncomment is distinct");
        map.press("ctrl+k", 4, true, true);
        require(map.press("ctrl+c", 7, true, true).command == "edit.copy", "Expired chord returns normal copy");
        map.press("ctrl+k", 8, true, true);
        require(map.press("escape", 8.2, true, true).consumed && map.pending(8.3).empty(), "Escape cancels chord");
        map.press("ctrl+k", 9, true, true);
        require(map.press("x", 9.2, true, true).consumed, "Invalid chord second stroke cannot type into source");
        map.load("vscode", {{"edit.duplicate", Json::array({"Ctrl+Alt+D"})}, {"edit.nextOccurrence", Json::array()}});
        require(map.press("ctrl+alt+d", 10, true, true).command == "edit.duplicate", "Custom binding");
        require(map.press("ctrl+d", 11, true, true).command.empty(), "Removed binding");
        bool conflict = false;
        try { map.load("rider", {{"edit.duplicate", Json::array({"Ctrl+S"})}}); } catch (...) { conflict = true; }
        require(conflict && map.hint("edit.duplicate") == "Ctrl+Alt+D", "Conflict preserves previous working keymap");
        for (const auto &text : {"", "Ctrl+", "Ctrl+Ctrl+F", "Ctrl+Unknown", "Ctrl+K,", "Ctrl+F+G"}) {
            bool rejected = false;
            try { Keymap::parse(text); } catch (...) { rejected = true; }
            require(rejected, "Malformed shortcut accepted");
        }
        require(fuzzyScore("Actor", "Source/Actor.h") > fuzzyScore("Actor", "Source/AbstractController.cpp"), "Filename ranking");
        require(fuzzyScore("zz", "Actor.h") < 0, "Nonmatching file");
        std::cout << "Command profiles, contexts, chords, timeout, overrides, conflicts and ranking passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
