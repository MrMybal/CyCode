#include "cycode/Application.h"
#include "cycode/DebugRuntime.h"
#include <algorithm>
#include <cyui/CyUI.h>
#include <imgui_stdlib.h>
#include <sstream>
namespace cycode {
Json Application::debugParameters() const {
    Json args = Json::array(); std::istringstream lines(debugArgs_); std::string line;
    while (std::getline(lines,line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); if (!line.empty()) args.push_back(line); }
    Json environment = Json::object(); std::istringstream variables(debugEnv_);
    while (std::getline(variables,line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); if (line.empty()) continue;
        const auto at = line.find('=');
        if (at == std::string::npos && line.front() == '-') environment[line.substr(1)] = nullptr;
        else environment[line.substr(0,at)] = at == std::string::npos ? "" : line.substr(at+1);
    }
    return {{"program",debugProgram_},{"cwd",debugCwd_},{"args",args},{"env",environment},{"adapter",debugAdapter_},{"runtime",debugRuntime_},{"dotnet",debugDotnet_},{"stopOnEntry",debugStopOnEntry_},{"terminal",debugTerminal_ ? "integrated" : "console"},{"buildBeforeLaunch",debugBuildBefore_}};
}
void Application::debugTask(const std::string &method, Json params) {
    if (debugFuture_.valid() || smoke_) return;
    debugOperation_ = method; auto engine = engine_;
    if (method == "debug.breakpoints") debugBreakpointApplyPath_ = params.at("path");
    debugVariablesStop_ = debugState_.value("stopGeneration",uint64_t(0));
    debugFuture_ = std::async(std::launch::async,[engine,method,params] {
        if (method == "debug.launch" || method == "debug.run") engine->dispatch("debug.configure",params);
        return engine->dispatch(method,params);
    });
    if (method != "debug.variables" && method != "debug.source") showPanel("Debugger","debugger");
}
void Application::startDebug(bool build) {
    if (debugFuture_.valid() || taskFuture_.valid()) return;
    showPanel("Debugger","debugger");
    if (debugProgram_.empty()) { cyui::notify(tr("Select an executable in the debug profile."),cyui::Tone::warning); return; }
    saveAll();
    if (std::any_of(tabs_.begin(),tabs_.end(),[](const auto &tab) { return tab->dirty; })) return;
    if (build && debugBuildBefore_ && (msbuildActive_ || unreal_.value("available",false) || buildState_.value("configured",false))) {
        pendingDebugBuild_ = true; task("build.run"); if (!taskFuture_.valid()) pendingDebugBuild_ = false;
    } else debugTask("debug.launch",debugParameters());
}
void Application::toggleBreakpoint(const std::string &path, size_t line) {
    if (debugFuture_.valid()) return;
    auto points = debugState_.value("breakpoints",Json::object()).value(path,Json::array());
    auto found = std::find_if(points.begin(),points.end(),[&](const Json &p) { return p.value("line",size_t(0)) == line; });
    if (found == points.end()) points.push_back({{"line",line}}); else points.erase(found);
    debugTask("debug.breakpoints",{{"path",path},{"breakpoints",points}});
}
void Application::openDebugSource(const std::string &path, size_t line, size_t column) {
    for (auto &tab : tabs_) if (tab->path == path) {
        active_ = focusTab_ = path; tab->editor.SetFocus();
        if (line) { tab->editor.SetCursor({line-1,column ? column-1 : 0}); tab->editor.ScrollToLine(line-1); } return;
    }
    if (debugFuture_.valid()) return;
    auto end = path.find('/',8); if (end == std::string::npos) return;
    debugSourcePath_ = path; debugSourceLine_ = line; debugSourceColumn_ = column;
    debugTask("debug.source",{{"sourceReference",std::stoi(path.substr(8,end-8))}});
}
void Application::debugFrame(const Json &frame) {
    const auto source = frame.value("source",Json::object());
    const auto path = source.value("path",""); const auto line = std::max(1,frame.value("line",1));
    const auto column = std::max(1,frame.value("column",1));
    if (!path.empty()) {
        auto range = Json{{"start",{{"line",line-1},{"character",column-1}}},{"end",{{"line",line-1},{"character",column-1}}}};
        try { auto location = engine_->navigationSource(utf8Path(path),range); if (location.is_object()) {
            debugCurrentPath_ = location.at("path"); debugCurrentLine_ = line; open(debugCurrentPath_,line,column,true); return;
        } } catch (const std::exception &) {}
    }
    if (source.value("sourceReference",0) > 0) {
        debugCurrentPath_ = "debug://"+std::to_string(source.at("sourceReference").get<int>())+"/"+source.value("name","Disassembly"); debugCurrentLine_ = line;
        openDebugSource(debugCurrentPath_,line,column);
    }
}
void Application::updateDebug() {
    if (!debugProfileLoaded_) {
        debugProfile_ = engine_->debugger().profile(); debugProgram_ = debugProfile_.value("program",""); debugCwd_ = debugProfile_.value("cwd",".");
        debugAdapter_ = debugProfile_.value("adapter","automatic"); debugStopOnEntry_ = debugProfile_.value("stopOnEntry",false); debugTerminal_ = debugProfile_.value("terminal","integrated") != "console";
        debugRuntime_ = debugProfile_.value("runtime","automatic"); debugDotnet_ = debugProfile_.value("dotnet","automatic");
        debugBuildBefore_ = debugProfile_.value("buildBeforeLaunch",true); debugEnv_.clear();
        for (auto i = debugProfile_.at("env").begin(); i != debugProfile_.at("env").end(); ++i) debugEnv_ += (i->is_null() ? "-"+i.key() : i.key()+"="+i->get<std::string>())+"\n";
        debugArgs_.clear(); for (const auto &arg : debugProfile_.value("args",Json::array())) { if (!debugArgs_.empty()) debugArgs_ += '\n'; debugArgs_ += arg.get<std::string>(); }
        if (debugProgram_.empty() && unreal_.value("available",false)) {
#ifdef _WIN32
            auto candidate = utf8Path(unrealEngine_)/"Engine/Binaries/Win64/UnrealEditor.exe";
#elif defined(__APPLE__)
            auto candidate = utf8Path(unrealEngine_)/"Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor";
#else
            auto candidate = utf8Path(unrealEngine_)/"Engine/Binaries/Linux/UnrealEditor";
#endif
            if (std::filesystem::is_regular_file(candidate)) { debugProgram_ = pathUtf8(candidate); debugArgs_ = pathUtf8(engine_->resolve(unrealProject_)); }
        }
        debugProfileLoaded_ = true;
    }
    if (debugFuture_.valid() && debugFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = debugFuture_.get();
            if (debugOperation_ == "debug.breakpoints") debugBreakpointDrafts_.erase(debugBreakpointApplyPath_);
            if (debugOperation_ == "debug.evaluate") debugEvaluation_ = result.value("result","");
            else if (debugOperation_ == "debug.variables" && engine_->debugger().status().value("stopGeneration",uint64_t(0)) == debugVariablesStop_) debugChildren_[std::to_string(debugVariableReference_)] = result.value("variables",Json::array());
            else if (debugOperation_ == "debug.run") { terminalId_ = result.at("terminalId"); showPanel("Terminal","terminal"); focusTerminal_ = true; terminalScrollBottom_ = true; }
            else if (debugOperation_ == "debug.source") {
                auto text = result.value("content",""); if (text.size() > 4*1024*1024 || text.find('\0') != std::string::npos) throw std::runtime_error("Debug source exceeds editor limits");
                auto tab = std::make_shared<Tab>(); tab->path = debugSourcePath_; tab->displayPath = debugSourcePath_.substr(debugSourcePath_.find('/',8)+1); tab->readOnly = true;
                tab->editor.SetText(text); configureEditor(tab); tabs_.push_back(tab); active_ = focusTab_ = tab->path; tab->editor.SetFocus();
                if (debugSourceLine_) { tab->editor.SetCursor({debugSourceLine_-1,debugSourceColumn_ ? debugSourceColumn_-1 : 0}); tab->editor.ScrollToLine(debugSourceLine_-1); }
            }
        } catch (const std::exception &e) { error(e.what()); }
        lastDebugPoll_ = 0;
    }
    if (ImGui::GetTime()-lastDebugPoll_ < .05) return;
    lastDebugPoll_ = ImGui::GetTime(); const auto previousStop = debugState_.value("stopGeneration",uint64_t(0)); const auto previousFrame = debugState_.value("frameId",0);
    debugState_ = engine_->debugger().status();
    if (debugState_.value("stopGeneration",uint64_t(0)) != previousStop || debugState_.value("frameId",0) != previousFrame) debugChildren_ = Json::object();
    if (debugState_.value("state","") == "stopped" && !debugFuture_.valid()) {
        auto frames = debugState_.value("frames",Json::array());
        for (const auto &frame : frames) if (frame.value("id",0) == debugState_.value("frameId",0)) {
            const auto key = std::to_string(debugState_.value("stopGeneration",uint64_t(0)))+":"+std::to_string(frame.value("id",0));
            if (key != debugJumpKey_) { debugJumpKey_ = key; debugFrame(frame); } break;
        }
    }
}
void Application::debugPanel() {
    if (!beginPanel("Debugger","debugger")) return;
    const auto state = debugState_.value("state","idle");
    auto action = [&](const char *id, const char *icon, const char *title) {
        ImGui::BeginDisabled(!commandEnabled(id)); const auto tip = std::string(tr(title))+" ("+keymap_.hint(id)+")";
        if (cyui::iconButton(icon,tip.c_str())) executeCommand(id); ImGui::EndDisabled(); ImGui::SameLine();
    };
    action("debug.start",cyui::icon::play,"Debug / Continue"); action("debug.pause",cyui::icon::pause,"Pause program");
    action("debug.stop",cyui::icon::stop,"Stop debugging");
    action("debug.next",cyui::icon::chevronRight,"Step over"); action("debug.stepIn",cyui::icon::chevronDown,"Step into"); action("debug.stepOut",cyui::icon::undo,"Step out");
    cyui::badge(tr(state.c_str()),state == "stopped" ? cyui::Tone::warning : cyui::Tone::neutral);
    if (debugState_.contains("runtime")) { ImGui::SameLine(); cyui::badge(debugState_.at("runtime") == "dotnet" ? "C# / .NET" : "C / C++",cyui::Tone::neutral); }
    if (debugFuture_.valid() || pendingDebugBuild_) { ImGui::SameLine(); cyui::spinner("##debug-task"); }
    if (!debugState_.value("terminalId","").empty()) { ImGui::SameLine(); if (cyui::ghostButton(tr("Program terminal"))) executeCommand("debug.terminal"); }
    if (ImGui::BeginTabBar("##debug-tabs")) {
        if (ImGui::BeginTabItem(tr("Variables"))) {
            if (state != "stopped") cyui::textDim("%s",tr("Pause or hit a breakpoint to inspect variables."));
            if (ImGui::BeginTable("##debug-variables",3,ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, {0,0})) {
                ImGui::TableSetupColumn(tr("Name")); ImGui::TableSetupColumn(tr("Value")); ImGui::TableSetupColumn(tr("Type")); ImGui::TableHeadersRow();
                std::function<void(const Json &,int)> row = [&](const Json &variable,int depth) {
                    if (depth > 8) return; ImGui::PushID(variable.value("name","").c_str());
                    const auto reference = variable.value("variablesReference",0); ImGui::TableNextRow(); ImGui::TableNextColumn();
                    bool open = ImGui::TreeNodeEx(variable.value("name","").c_str(),reference > 0 ? ImGuiTreeNodeFlags_SpanAvailWidth : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(variable.value("value","").c_str());
                    ImGui::TableNextColumn(); cyui::textDim("%s",variable.value("type","").c_str());
                    if (open && reference > 0) {
                        const auto key = std::to_string(reference);
                        if (!debugChildren_.contains(key) && !debugFuture_.valid()) { debugVariableReference_ = reference; debugTask("debug.variables",{{"variablesReference",reference}}); }
                        if (debugChildren_.contains(key)) for (const auto &child : debugChildren_.at(key)) row(child,depth+1);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                };
                for (const auto &scope : debugState_.value("scopes",Json::array())) {
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    const auto name = scope.value("name","");
                    if (ImGui::TreeNodeEx(tr(name.c_str()),name == "Local" || name == "Locals" ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None)) {
                        for (const auto &variable : debugState_.value("variables",Json::array())) if (variable.value("scope","") == name) row(variable,0);
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Call stack"))) {
            const auto threads = debugState_.value("threads",Json::array());
            ImGui::BeginDisabled(debugFuture_.valid() || state != "stopped");
            if (ImGui::BeginCombo("##debug-thread",std::to_string(debugState_.value("threadId",0)).c_str())) {
                for (const auto &thread : threads) if (ImGui::Selectable(thread.value("name","").c_str(),thread.value("id",0) == debugState_.value("threadId",0))) debugTask("debug.thread",{{"id",thread.at("id")}});
                ImGui::EndCombo();
            }
            for (const auto &frame : debugState_.value("frames",Json::array())) {
                auto title = frame.value("name","")+"   "+frame.value("source",Json::object()).value("name","")+":"+std::to_string(frame.value("line",0));
                if (ImGui::Selectable((title+"###frame"+std::to_string(frame.value("id",0))).c_str(),frame.value("id",0) == debugState_.value("frameId",0))) debugTask("debug.frame",{{"id",frame.at("id")}});
            }
            ImGui::EndDisabled(); ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Watch"))) {
            ImGui::SetNextItemWidth(cyui::dp(360)); const auto enter = ImGui::InputTextWithHint("##debug-expression",tr("C++ / C# expression"),&debugWatch_,ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine(); ImGui::BeginDisabled(debugFuture_.valid() || state != "stopped" || debugWatch_.empty());
            if (cyui::button(tr("Evaluate")) || enter) debugTask("debug.evaluate",{{"expression",debugWatch_}});
            ImGui::EndDisabled(); ImGui::SameLine(); ImGui::BeginDisabled(debugFuture_.valid() || debugWatch_.empty());
            if (cyui::button(tr("Add watch"))) { auto watches = engine_->debugger().profile().value("watches",Json::array()); watches.push_back(debugWatch_); debugTask("debug.configure",{{"watches",watches}}); }
            ImGui::EndDisabled(); ImGui::TextWrapped("%s",debugEvaluation_.c_str());
            for (const auto &watch : debugState_.value("watches",Json::array())) ImGui::TextWrapped("%s = %s",watch.value("expression","").c_str(),watch.value("result",watch.value("error","")).c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Breakpoints"))) {
            const auto points = debugState_.value("breakpoints",Json::object());
            for (auto file = points.begin(); file != points.end(); ++file) {
                ImGui::PushID(file.key().c_str());
                if (ImGui::TreeNodeEx(file.key().c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (!debugBreakpointDrafts_.contains(file.key())) debugBreakpointDrafts_[file.key()] = {{"base",file.value()},{"points",file.value()}};
                    auto &draft = debugBreakpointDrafts_.at(file.key());
                    if (ImGui::BeginTable("##breakpoint-options",5,ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg)) {
                        for (const auto *column : {"Enabled","Line","Condition","Hit condition","Log message"}) ImGui::TableSetupColumn(tr(column)); ImGui::TableHeadersRow();
                        for (size_t i = 0; i < draft.at("points").size(); ++i) {
                            auto &point = draft.at("points")[i]; ImGui::PushID(int(i)); ImGui::TableNextRow(); ImGui::TableNextColumn();
                            bool enabled = point.value("enabled",true); if (ImGui::Checkbox("##bp-enabled",&enabled)) point["enabled"] = enabled;
                            ImGui::TableNextColumn(); ImGui::Text("%d",point.value("line",0));
                            for (auto field : {"condition","hitCondition","logMessage"}) { ImGui::TableNextColumn(); auto text = point.value(field,""); ImGui::SetNextItemWidth(-1); if (ImGui::InputText((std::string("##")+field).c_str(),&text)) point[field] = text; }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    const bool conflict = draft.at("base") != file.value();
                    if (conflict) cyui::textColoured(cyui::palette().warning,"%s",tr("Breakpoints changed. Reload before applying."));
                    ImGui::BeginDisabled(debugFuture_.valid() || conflict);
                    if (cyui::button(tr("Apply breakpoint changes"))) debugTask("debug.breakpoints",{{"path",file.key()},{"breakpoints",draft.at("points")},{"expectedBreakpoints",draft.at("base")}});
                    ImGui::EndDisabled(); ImGui::SameLine();
                    if (cyui::button(tr("Reload"))) draft = {{"base",file.value()},{"points",file.value()}};
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Debug console"))) {
            cyui::textDim("%s",tr(debugState_.value("runtime","") == "dotnet" ? "Managed debug output appears here. Run without debugging supports terminal input." : "Program input/output is available in Program terminal."));
            auto text = debugState_.value("output",""); ImGui::InputTextMultiline("##debug-output",&text,{0,0},ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Launch configuration"))) {
            ImGui::BeginDisabled(debugFuture_.valid() || (state != "idle" && state != "exited" && state != "failed"));
            if (cyui::beginProperties("##debug-profile",150)) {
                cyui::property(tr("Program / assembly")); ImGui::InputText("##debug-program",&debugProgram_);
                if (debugProgram_.empty()) for (const auto &artifact : msbuildState_.value("artifacts",Json::array())) if (isRunnableArtifact(utf8Path(artifact.value("path","")))) {
                    if (ImGui::Selectable(artifact.value("name","").c_str())) debugProgram_ = artifact.at("path");
                }
                if (debugProgram_.empty()) for (const auto &target : buildState_.value("targets",Json::array())) if (target.value("type","") == "EXECUTABLE") for (const auto &artifact : target.value("artifacts",Json::array())) {
                    if (ImGui::Selectable(target.value("name","").c_str())) debugProgram_ = pathUtf8(utf8Path(buildState_.value("profile",Json::object()).value("buildDirectory","build"))/utf8Path(artifact.value("path","")));
                }
                cyui::property(tr("Working directory")); ImGui::InputText("##debug-cwd",&debugCwd_);
                cyui::property(tr("Arguments (one per line)")); ImGui::InputTextMultiline("##debug-args",&debugArgs_,{0,cyui::dp(60)});
                cyui::property(tr("Environment (KEY=VALUE)")); ImGui::InputTextMultiline("##debug-env",&debugEnv_,{0,cyui::dp(50)});
                cyui::property(tr("Debug adapter")); ImGui::InputText("##debug-adapter",&debugAdapter_);
                cyui::property(tr("Debug engine"));
                const auto engineName = debugRuntime_ == "dotnet" ? "C# / .NET" : debugRuntime_ == "native" ? "C / C++" : tr("Automatic");
                if (ImGui::BeginCombo("##debug-runtime",engineName)) {
                    for (const auto &engine : {"automatic","native","dotnet"}) if (ImGui::Selectable(engine == std::string("automatic") ? tr("Automatic") : engine == std::string("native") ? "C / C++" : "C# / .NET",debugRuntime_ == engine)) debugRuntime_ = engine;
                    ImGui::EndCombo();
                }
                cyui::property(tr(".NET host")); ImGui::InputText("##debug-dotnet",&debugDotnet_);
                cyui::property(tr("Build before debugging")); ImGui::Checkbox("##debug-build",&debugBuildBefore_);
                cyui::property(tr("Stop on entry")); ImGui::Checkbox("##debug-entry",&debugStopOnEntry_);
                cyui::property(tr("Integrated program terminal")); ImGui::Checkbox("##debug-terminal",&debugTerminal_);
                cyui::endProperties();
            }
            ImGui::TextWrapped("%s",tr("Managed debug output appears here. Run without debugging supports terminal input."));
            if (cyui::button(tr("Save debug profile"))) debugTask("debug.configure",debugParameters());
            ImGui::SameLine(); if (cyui::button(tr("Reload profile"))) debugProfileLoaded_ = false;
            ImGui::EndDisabled(); ImGui::Separator();
            ImGui::SetNextItemWidth(cyui::dp(180)); ImGui::InputInt(tr("Attach to process ID"),&debugAttachPid_);
            ImGui::SameLine(); ImGui::BeginDisabled(debugFuture_.valid() || debugAttachPid_ <= 0 || state == "running" || state == "stopped");
            if (cyui::button(tr("Attach"))) { auto params = debugParameters(); params["pid"] = debugAttachPid_; debugTask("debug.attach",params); }
            ImGui::EndDisabled(); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (!debugState_.value("error","").empty()) cyui::textColoured(cyui::palette().error,"%s",debugState_.at("error").get_ref<const std::string &>().c_str());
    ImGui::End();
}
} // namespace cycode
