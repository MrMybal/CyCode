#include "cycode/Application.h"
#include "cycode/EditorLanguage.h"
#include <algorithm>
#include <cyui/CyUI.h>
#include <imgui_stdlib.h>

namespace cycode {
void Application::vcsTask(const std::string &method, Json params) {
    if (vcsFuture_.valid() || taskFuture_.valid()) { error(tr("A task is already running.")); return; }
    auto engine = engine_; vcsOperation_ = method;
    if (method.starts_with("vcs.")) params["provider"] = provider_;
    vcsFuture_ = std::async(std::launch::async,[engine,method,params] { return engine->dispatch(method,params); });
}
void Application::updateVcs() {
    if (!vcsFuture_.valid() || vcsFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = vcsFuture_.get();
        if (vcsOperation_ == "vcs.conflicts") {
            vcsConflict_ = std::move(result); vcsSection_ = "Conflicts";
            vcsBase_.SetText(vcsConflict_.at("versions").at("base").is_null() ? "" : vcsConflict_.at("versions").at("base").get<std::string>());
            vcsOurs_.SetText(vcsConflict_.at("versions").at("ours").is_null() ? "" : vcsConflict_.at("versions").at("ours").get<std::string>());
            vcsTheirs_.SetText(vcsConflict_.at("versions").at("theirs").is_null() ? "" : vcsConflict_.at("versions").at("theirs").get<std::string>());
            open(vcsConflict_.at("path"));
        } else if (vcsOperation_ == "vcs.diff" || vcsOperation_ == "vcs.show" || vcsOperation_ == "vcs.blame" || vcsOperation_ == "vcs.log") {
            vcsPreviewTitle_ = vcsOperation_; vcsPreview_.SetText(result.value("output","")); vcsSection_ = "Review";
            if (result.value("exitCode",0)) error(tr("Task failed. See output."));
        } else {
            output_ += "\n> "+vcsOperation_+"\n"+result.value("output","");
            if (result.value("exitCode",0)) error(tr("Task failed. See output."));
            else { cyui::notify(tr("Task completed"),cyui::Tone::success); if (vcsOperation_ == "vcs.resolve") { vcsConflict_ = Json::object(); vcsSection_ = "Changes"; } }
            refreshFiles();
        }
    } catch (const std::exception &e) { error(e.what()); }
    // A background status read may have started before the mutation. Let it finish,
    // then invalidate its refresh timestamp so the next frame obtains fresh data.
    lastGitRefresh_ = -10;
    if (!gitFuture_.valid()) refreshGit();
}
static bool conflictStatus(const std::string &status) { return status.find('U') != std::string::npos || status == "AA" || status == "DD"; }
void Application::gitPanel() {
    if (!beginPanel("Version control","vcs")) return;
    const bool working = vcsFuture_.valid() || taskFuture_.valid();
    ImGui::SetNextItemWidth(cyui::dp(140));
    ImGui::BeginDisabled(working || gitFuture_.valid());
    if (ImGui::BeginCombo("##provider",provider_.c_str())) {
        auto choose = [&](const std::string &id, const std::string &title) {
            if (ImGui::Selectable(title.c_str(),provider_ == id)) { provider_ = id; git_ = vcsInfo_ = vcsHistory_ = vcsConflict_ = Json::object(); vcsSection_ = "Changes"; vcsPreviewTitle_.clear(); refreshGit(); }
        };
        choose("git","Git");
        for (const auto &provider : pluginInfo_.value("vcsProviders",Json::array())) choose(provider.at("id"),provider.at("title"));
        ImGui::EndCombo();
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if (cyui::button(tr("Refresh"))) refreshGit();
    ImGui::SameLine();
    if (working || gitFuture_.valid()) cyui::spinner("##git-busy");
    else if (git_.value("repository",false)) {
        cyui::textDim("%s",git_.value("branch","").c_str());
        if (vcsInfo_.value("hasUpstream",false)) { ImGui::SameLine(); cyui::textDim("  +%d / -%d",vcsInfo_.value("ahead",0),vcsInfo_.value("behind",0)); }
    }
    if (!git_.value("repository",false)) {
        cyui::emptyState(tr("No Git repository"),tr("Open a folder containing a Git repository."),cyui::icon::sync);
        if (provider_ == "git") { cyui::ScopedDisabled disabled(working); if (cyui::button(tr("Initialize Git repository"))) vcsTask("vcs.init"); }
        ImGui::End(); return;
    }
    auto button = [&](const char *title,const std::string &action,Json params = {}) {
        cyui::ScopedDisabled disabled(working || !vcsSupports(action));
        if (cyui::button(tr(title))) vcsTask("vcs."+action,params.is_null() ? Json::object() : params);
    };
    if (ImGui::BeginTabBar("##vcs-sections")) {
        // SetSelected is a one-frame request; user tab clicks remain free afterwards.
        const auto requested = vcsSection_; vcsSection_.clear();
        auto begin = [&](const char *name) { return ImGui::BeginTabItem(tr(name),nullptr,requested == name ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None); };
        if (begin("Changes")) {
            ImGui::SetNextItemWidth(cyui::dp(350)); ImGui::InputTextWithHint("##commit",tr("Commit message"),&commitMessage_); ImGui::SameLine();
            { cyui::ScopedDisabled disabled(working || commitMessage_.empty() || !vcsSupports("commit")); if (cyui::primaryButton(tr("Commit staged"))) vcsTask("vcs.commit",{{"message",commitMessage_}}); }
            button("Diff","diff"); ImGui::SameLine(); button("Staged diff","diff",{{"staged",true}}); ImGui::SameLine(); button("History","log");
            if (ImGui::BeginTable("##changes",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,0})) {
                ImGui::TableSetupColumn(tr("Status"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(50)); ImGui::TableSetupColumn(tr("File")); ImGui::TableSetupColumn(tr("Actions"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(270)); ImGui::TableHeadersRow();
                for (const auto &file : git_.value("files",Json::array())) {
                    auto path = file.at("path").get<std::string>(), status = file.at("status").get<std::string>();
                    ImGui::PushID(path.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn(); cyui::textDim("%s",status.c_str()); ImGui::TableNextColumn();
                    if (ImGui::Selectable(path.c_str())) open(path);
                    ImGui::TableNextColumn();
                    if (conflictStatus(status) && vcsSupports("conflicts")) button("Resolve conflict","conflicts",{{"path",path}});
                    else { button("Stage","stage",{{"path",path}}); ImGui::SameLine(); button("Unstage","unstage",{{"path",path}}); }
                    ImGui::SameLine(); button("Diff","diff",{{"path",path}}); ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (vcsSupports("inspect") && begin("Branches")) {
            ImGui::SetNextItemWidth(cyui::dp(210)); ImGui::InputTextWithHint("##new-branch",tr("New branch name"),&vcsNewBranch_); ImGui::SameLine();
            button("Create branch","branch.create",{{"name",vcsNewBranch_},{"start",vcsRevision_}});
            ImGui::SetNextItemWidth(cyui::dp(210)); ImGui::InputTextWithHint("##start",tr("Start revision"),&vcsRevision_);
            if (ImGui::BeginTable("##branches",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn(tr("Branch")); ImGui::TableSetupColumn(tr("Upstream")); ImGui::TableSetupColumn(tr("Actions")); ImGui::TableHeadersRow();
                for (const auto &branch : vcsInfo_.value("branches",Json::array())) {
                    const auto name = branch.at("name").get<std::string>(); ImGui::PushID(name.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn();
                    if (ImGui::Selectable((std::string(branch.at("current").get<bool>() ? "* " : "  ")+name).c_str(),vcsBranch_ == name)) vcsBranch_ = name;
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(branch.at("upstream").get<std::string>().c_str()); ImGui::TableNextColumn();
                    if (!branch.at("current").get<bool>()) { button(branch.at("remote").get<bool>() ? "Track branch" : "Switch branch","branch.switch",{{"name",name},{"track",branch.at("remote")}}); }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (!vcsBranch_.empty()) { cyui::textDim("%s",vcsBranch_.c_str()); button("Rename branch","branch.rename",{{"name",vcsBranch_},{"newName",vcsNewBranch_}}); ImGui::SameLine(); button("Delete merged branch","branch.delete",{{"name",vcsBranch_}}); }
            cyui::divider(); button("Merge revision","merge",{{"revision",vcsRevision_}});
            ImGui::TextWrapped("%s",tr("Switching and merging require saved buffers and a clean tracked working tree."));
            ImGui::EndTabItem();
        }
        if (vcsSupports("history") && begin("History")) {
            if (ImGui::BeginTable("##history",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,0})) {
                ImGui::TableSetupColumn(tr("Commit"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(90)); ImGui::TableSetupColumn(tr("Author")); ImGui::TableSetupColumn(tr("Date")); ImGui::TableSetupColumn(tr("Message")); ImGui::TableHeadersRow();
                for (const auto &commit : vcsHistory_.value("commits",Json::array())) {
                    auto hash = commit.at("hash").get<std::string>(); ImGui::PushID(hash.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn();
                    if (ImGui::Selectable(hash.substr(0,10).c_str())) vcsTask("vcs.show",{{"revision",hash}});
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(commit.at("author").get<std::string>().c_str()); ImGui::TableNextColumn(); ImGui::TextUnformatted(commit.at("date").get<std::string>().c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(commit.at("subject").get<std::string>().c_str()); ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (vcsSupports("inspect") && begin("Remotes")) {
            for (const auto &remote : vcsInfo_.value("remotes",Json::array())) {
                if (ImGui::Selectable((remote.at("name").get<std::string>()+"  "+remote.at("url").get<std::string>()).c_str(),remote.at("name") == vcsRemote_)) { vcsRemote_ = remote.at("name"); vcsRemoteUrl_ = remote.at("url"); }
            }
            if (cyui::beginProperties("##git-remote",140)) {
                cyui::property(tr("Remote name")); ImGui::InputText("##remote",&vcsRemote_); cyui::property(tr("Remote URL")); ImGui::InputText("##url",&vcsRemoteUrl_);
                cyui::property(tr("Branch")); ImGui::InputTextWithHint("##push-branch",git_.value("branch","").c_str(),&vcsBranch_); cyui::endProperties();
            }
            button("Add remote","remote.add",{{"remote",vcsRemote_},{"url",vcsRemoteUrl_}}); ImGui::SameLine(); button("Update remote URL","remote.set",{{"remote",vcsRemote_},{"url",vcsRemoteUrl_}}); ImGui::SameLine(); button("Remove remote","remote.remove",{{"remote",vcsRemote_}});
            cyui::divider(); ImGui::Checkbox(tr("Set upstream on push"),&vcsSetUpstream_);
            Json parameters = {{"remote",vcsRemote_},{"branch",vcsBranch_.empty() ? git_.value("branch","") : vcsBranch_},{"setUpstream",vcsSetUpstream_}};
            button("Fetch","fetch",{{"remote",vcsRemote_}}); ImGui::SameLine(); button("Pull fast-forward","pull",parameters); ImGui::SameLine(); button("Push","push",parameters);
            ImGui::TextWrapped("%s",tr("Uses configured Git credentials and hooks. Pull refuses divergent branches.")); ImGui::EndTabItem();
        }
        if (vcsSupports("stash.save") && begin("Stashes")) {
            ImGui::SetNextItemWidth(cyui::dp(300)); ImGui::InputTextWithHint("##stash-message",tr("Stash message"),&vcsStashMessage_); ImGui::SameLine(); ImGui::Checkbox(tr("Include untracked"),&vcsIncludeUntracked_);
            button("Save stash","stash.save",{{"message",vcsStashMessage_.empty() ? "CyCode stash" : vcsStashMessage_},{"includeUntracked",vcsIncludeUntracked_}});
            int index = 0; for (const auto &stash : vcsInfo_.value("stashes",Json::array())) { ImGui::PushID(index); if (ImGui::Selectable((stash.at("ref").get<std::string>()+"  "+stash.at("message").get<std::string>()).c_str(),vcsStashIndex_ == index)) vcsStashIndex_ = index; ImGui::PopID(); ++index; }
            { cyui::ScopedDisabled disabled(index == 0 || vcsStashIndex_ >= index); button("Apply stash","stash.apply",{{"index",vcsStashIndex_}}); ImGui::SameLine(); button("Pop stash","stash.pop",{{"index",vcsStashIndex_}}); ImGui::SameLine(); if (cyui::button(tr("Drop selected stash..."))) ImGui::OpenPopup(tr("Drop stash?")); }
            if (ImGui::BeginPopupModal(tr("Drop stash?"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::TextWrapped("%s",tr("Permanently remove this saved stash entry?")); ImGui::Text("stash@{%d}",vcsStashIndex_); if (cyui::button(tr("Drop stash"))) { vcsTask("vcs.stash.drop",{{"index",vcsStashIndex_}}); ImGui::CloseCurrentPopup(); } ImGui::SameLine(); if (cyui::button(tr("Cancel"))) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
            ImGui::EndTabItem();
        }
        if (vcsSupports("conflicts") && begin("Conflicts")) {
            if (!vcsConflict_.contains("path")) ImGui::TextWrapped("%s",tr("Choose Resolve conflict from a conflicted file in Changes."));
            else {
                auto path = vcsConflict_.at("path").get<std::string>(); ImGui::TextUnformatted(path.c_str()); ImGui::SameLine(); button("Refresh conflict","conflicts",{{"path",path}});
                auto use = [&](const char *title,const char *version) {
                    cyui::ScopedDisabled disabled(working || vcsConflict_.at("versions").at(version).is_null());
                    if (cyui::button(tr(title))) {
                        try { engine_->dispatch("documents.replace",{{"path",path},{"expectedRevision",vcsConflict_.at("revision")},{"text",vcsConflict_.at("versions").at(version)}}); vcsTask("vcs.conflicts",{{"path",path}}); }
                        catch (const std::exception &e) { error(e.what()); }
                    }
                };
                use("Use ours in editor","ours"); ImGui::SameLine(); use("Use theirs in editor","theirs"); ImGui::SameLine();
                { cyui::ScopedDisabled disabled(working); if (cyui::primaryButton(tr("Save and mark resolved"))) {
                    try { auto document = engine_->dispatch("documents.read",{{"path",path}}); vcsTask("vcs.resolve",{{"path",path},{"expectedRevision",document.at("revision")}}); } catch (const std::exception &e) { error(e.what()); }
                } }
                ImGui::TextWrapped("%s",tr("Edit the working file in its normal tab. These three source versions are read-only."));
                if (ImGui::BeginTable("##conflict-versions",3,ImGuiTableFlags_Resizable)) {
                    for (auto title : {"Base","Ours","Theirs"}) ImGui::TableSetupColumn(tr(title)); ImGui::TableHeadersRow(); ImGui::TableNextRow();
                    int column = 0; for (auto editor : {&vcsBase_,&vcsOurs_,&vcsTheirs_}) { ImGui::TableSetColumnIndex(column++); editor->SetReadOnlyEnabled(true); editor->SetPalette(editorPalette(theme_)); editor->SetLanguage(editorLanguage("source.cpp",unreal_.value("available",false))); editor->SetShowMiniMapEnabled(false); editor->SetShowLineNumbersEnabled(true); ImGui::PushID(column); { cyui::ScopedFont mono(cyui::monoFont()); editor->Render("##conflict-code",{0,std::max(cyui::dp(100),ImGui::GetContentRegionAvail().y)}); } ImGui::PopID(); }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }
        if (!vcsPreviewTitle_.empty() && begin("Review")) {
            cyui::textDim("%s",vcsPreviewTitle_.c_str()); vcsPreview_.SetReadOnlyEnabled(true); vcsPreview_.SetShowMiniMapEnabled(false); vcsPreview_.SetPalette(editorPalette(theme_)); { cyui::ScopedFont mono(cyui::monoFont()); vcsPreview_.Render("##git-review",{0,0}); } ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
} // namespace cycode
