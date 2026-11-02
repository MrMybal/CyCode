#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <sstream>
#include <algorithm>
namespace cycode {
void Application::projectTasksPanel() {
    if (!beginPanel("Project tasks","projectTasks")) return;
    if (!projectActive_) {
        cyui::emptyState(tr("No Cargo, npm or standalone Java project"),tr("Open Cargo.toml, package.json, a Java source file or its folder."),cyui::icon::folder);
        ImGui::End(); return;
    }
    if (projectProfile_.empty()) { cyui::spinner("##project-pending"); ImGui::End(); return; }
    const auto kind=projectProfile_.value("system",""); const bool busy=taskFuture_.valid();
    auto action=[&](const char *text,const char *method,bool enabled=true) { cyui::ScopedDisabled disabled(busy || !enabled); if (cyui::button(tr(text))) task(method); };
    action("Configure","project.configure"); ImGui::SameLine(); action("Build project","project.build");
    ImGui::SameLine(); action("Run project tests","project.test",kind!="java");
    ImGui::SameLine(); action("Run without debugging","project.launch");
    ImGui::SameLine(); { cyui::ScopedDisabled disabled(!commandEnabled("debug.start")); if (cyui::button(tr("Debug / Continue"))) executeCommand("debug.start"); }
    if (busy && taskMethod_.starts_with("project.")) { ImGui::SameLine(); if (cyui::button(tr("Cancel task"))) engine_->dispatch("project.cancel"); }
    cyui::divider();
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##project-properties",155)) {
            auto input=[&](const char *title,const char *key) { cyui::property(tr(title)); auto text=projectProfile_.value(key,""); if (ImGui::InputText((std::string("##project-")+key).c_str(),&text)) { projectProfile_[key]=text; buildEdited_=true; } };
            cyui::property(tr("Project system"));
            if (ImGui::BeginCombo("##project-system",kind.c_str())) {
                for (const auto *system : {"cargo","npm","java"}) if (ImGui::Selectable(system,kind==system)) { projectProfile_["system"]=system; buildEdited_=true; }
                ImGui::EndCombo();
            }
            input("Project directory","directory");
            cyui::property(tr("Configuration")); const auto configuration=projectProfile_.value("configuration","Debug");
            if (ImGui::BeginCombo("##project-configuration",configuration.c_str())) {
                for (const auto *item : {"Debug","Release"}) if (ImGui::Selectable(item,configuration==item)) { projectProfile_["configuration"]=item; buildEdited_=true; }
                ImGui::EndCombo();
            }
            if (kind=="cargo") {
                input("Cargo package","package");
                input("Binary target","target");
                if (!projectState_.value("targets",Json::array()).empty()) {
                    cyui::property(tr("Detected targets"));
                    if (ImGui::BeginCombo("##cargo-targets",tr("Select a binary target"))) {
                        for (const auto &target : projectState_.at("targets")) {
                            const auto &kinds=target.at("kind"); if (std::find(kinds.begin(),kinds.end(),"bin")==kinds.end()) continue;
                            const auto name=target.at("name").get<std::string>(),package=target.at("package").get<std::string>();
                            if (ImGui::Selectable((package+" / "+name).c_str())) { projectProfile_["package"]=package; projectProfile_["target"]=name; buildEdited_=true; }
                        }
                        ImGui::EndCombo();
                    }
                }
                cyui::property(tr("Dependencies")); auto offline=projectProfile_.value("offline",false); if (ImGui::Checkbox(tr("Offline Cargo mode"),&offline)) { projectProfile_["offline"]=offline; buildEdited_=true; }
            } else if (kind=="npm") {
                cyui::property(tr("npm script")); const auto selected=projectProfile_.value("script","");
                if (ImGui::BeginCombo("##npm-script",selected.empty() ? tr("Automatic") : selected.c_str())) {
                    if (ImGui::Selectable(tr("Automatic"),selected.empty())) { projectProfile_["script"]=""; buildEdited_=true; }
                    for (const auto &script : projectState_.value("scripts",Json::array())) { const auto name=script.at("name").get<std::string>(); if (ImGui::Selectable(name.c_str(),name==selected)) { projectProfile_["script"]=name; buildEdited_=true; } }
                    ImGui::EndCombo();
                }
            } else if (kind=="java") {
                input("Java source directory","sourceDirectory"); input("Java main class","mainClass");
                if (!projectState_.value("artifacts",Json::array()).empty()) {
                    cyui::property(tr("Detected targets"));
                    if (ImGui::BeginCombo("##java-mains",tr("Select a main class"))) {
                        for (const auto &artifact : projectState_.at("artifacts")) { const auto name=artifact.value("mainClass",""); if (!name.empty() && ImGui::Selectable(name.c_str())) { projectProfile_["mainClass"]=name; buildEdited_=true; } }
                        ImGui::EndCombo();
                    }
                }
            }
            cyui::property(tr("Arguments (one per line)")); std::string args;
            for (const auto &argument : projectProfile_.value("args",Json::array())) { if (!args.empty()) args+='\n'; args+=argument.get<std::string>(); }
            if (ImGui::InputTextMultiline("##project-args",&args,{-1,cyui::dp(70)})) {
                auto values=Json::array(); std::istringstream lines(args); std::string line; while (std::getline(lines,line)) values.push_back(line);
                projectProfile_["args"]=values; buildEdited_=true;
            }
            cyui::endProperties();
        }
    }
    if (kind=="npm") {
        action("Run npm script","project.script"); ImGui::SameLine(); action("Install npm dependencies","project.install");
        for (const auto &script : projectState_.value("scripts",Json::array())) { cyui::ScopedFont mono(cyui::monoFont(),.9f); ImGui::TextWrapped("%s: %s",script.at("name").get_ref<const std::string &>().c_str(),script.at("command").get_ref<const std::string &>().c_str()); }
    }
    if (kind=="java") ImGui::TextWrapped("%s",tr("Standalone Java: compilation and Run. Maven, Gradle and Java debugging are not connected."));
    const auto message=projectState_.value("message",""); if (!message.empty()) ImGui::TextWrapped("%s",message.c_str());
    if (projectState_.value("stale",false)) cyui::badge(tr("Project metadata changed; configure or build again."),cyui::Tone::warning);
    for (const auto &artifact : projectState_.value("artifacts",Json::array())) cyui::textDim("%s: %s",artifact.value("name","").c_str(),artifact.value("path","").c_str());
    ImGui::End();
}
} // namespace cycode
