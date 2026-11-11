#include "cycode/Commands.h"
#include <iostream>
using namespace cycode;
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        Keymap map;
        require(findCommand("edit.snippet")!=nullptr,"Snippet command missing");
        require(findCommand("edit.surroundSnippet")!=nullptr,"Surround snippet command missing");
        require(findCommand("edit.snippetCatalogue")!=nullptr,"Snippet editing command missing");
        require(findCommand("edit.userSnippets")!=nullptr,"Personal snippet editing command missing");
        require(findCommand("edit.captureSnippet")!=nullptr,"Selection capture command missing");
        require(findCommand("navigate.include")!=nullptr,"Include navigation command missing");
        require(findCommand("navigate.relatedFiles")!=nullptr,"Related-file inspector command missing");
        map.load("rider");
        require(map.press("F2",0,true,true).command=="problem.nextInFile","Display-form Rider diagnostic shortcut");
        require(map.press("Alt+Shift+F2",0,true,true).command=="problem.previous","Display modifier order");
        require(map.press("DoubleShift",0,true,true).command=="search.everywhere","Canonical double shift");
        require(map.press("Double Shift",0,true,true).command=="search.everywhere","Displayed double shift");
        require(map.press("Ctrl+K, Ctrl+C",0,true,true).command.empty(),"A complete chord is not a stroke");
        require(map.press("unknown key",0,true,true).command.empty(),"Invalid host event must not dispatch");
        require(map.press("ctrl+j",.001,true,true).command=="edit.snippet","Rider snippet shortcut");
        require(map.press("ctrl+alt+j",.0015,true,true).command=="edit.surroundSnippet","Rider surround snippet shortcut");
        require(map.press("alt+o",.0016,true,true).command=="navigate.counterpart","Rider header/source binding missing");
        map.load("visualstudio");
        map.press("ctrl+k",.002,true,true);
        require(map.press("ctrl+x",.003,true,true).command=="edit.snippet","Visual Studio snippet chord");
        map.press("ctrl+k",.004,true,true);
        require(map.press("ctrl+o",.005,true,true).command=="navigate.counterpart","Visual Studio header/source chord missing");
        for (const auto &profile : {"rider", "visualstudio", "vscode"}) {
            map.load(profile);
            const bool rider=std::string(profile)=="rider";
            require(map.press("shift+f1",.009,true,true).command=="help.context","Context help binding missing");
            require(map.press(rider ? "ctrl+alt+b" : "ctrl+f12",.01,true,true).command=="navigate.implementation","Implementation binding missing");
            if (rider) {
                require(map.press("ctrl+shift+f7",.005,true,true).command=="code.occurrences","Rider occurrence shortcut missing");
                require(map.press("ctrl+shift+b",.02,true,true).command=="navigate.typeDefinition","Type navigation collided with build");
                require(map.press("ctrl+f9",.03,true,true).command=="build.run","Rider build binding lost");
            }
            require(findCommand("navigate.declaration") && findCommand("navigate.typeDefinition"),"Navigation command catalogue incomplete");
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
        require(map.press("alt+f9",.001,true,true).command == "debug.runToCursor","Rider run to cursor binding");
        require(map.press("alt+f9",.002,false,false).command.empty(),"Run to cursor requires a document");
        map.load("visualstudio");
        require(map.press("ctrl+f10",.003,true,true).command == "debug.runToCursor","Visual Studio run to cursor binding");
        map.load("rider",{{"debug.runToCursor",Json::array({"Ctrl+Alt+F10"})}});
        require(map.press("ctrl+alt+f10",.004,true,true).command == "debug.runToCursor","Custom run-to-cursor binding was overwritten");
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
        for (const auto *id:{"view.occurrences","code.selectOccurrences","code.clearOccurrences","code.nextOccurrence","code.previousOccurrence"}) require(findCommand(id),"Occurrence catalogue incomplete");
        require(fuzzyScore("Actor", "Source/Actor.h") > fuzzyScore("Actor", "Source/AbstractController.cpp"), "Filename ranking");
        require(fuzzyScore("zz", "Actor.h") < 0, "Nonmatching file");
        std::cout << "Command profiles, contexts, chords, timeout, overrides, conflicts and ranking passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
