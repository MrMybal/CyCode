#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include "cycode/EditorLanguage.h"
#include <algorithm>
#include <cyui/CyUI.h>
#include <imgui_stdlib.h>

namespace cycode {
template<class T> static bool completed(std::future<T> &future) { return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
void Application::styleChangePreview() {
    const auto &p = cyui::palette();
    const auto beforeBackground = cyui::mix(p.bg0, p.error, .08f);
    const auto afterBackground = cyui::mix(p.bg0, p.success, .10f);
    const auto beforeGutter = cyui::mix(p.bg0, p.error, .18f);
    const auto afterGutter = cyui::mix(p.bg0, p.success, .20f);
    auto colours = editorPalette(theme_);
    for (auto role : {TextEditor::Color::text, TextEditor::Color::keyword, TextEditor::Color::declaration,
                      TextEditor::Color::number, TextEditor::Color::string, TextEditor::Color::comment,
                      TextEditor::Color::preprocessor, TextEditor::Color::function, TextEditor::Color::macro,
                      TextEditor::Color::attribute, TextEditor::Color::identifier, TextEditor::Color::knownIdentifier,
                      TextEditor::Color::lineNumber, TextEditor::Color::currentLineNumber}) {
        auto &colour = colours[static_cast<size_t>(role)];
        colour = readableText(colour,{p.bg0,beforeBackground,afterBackground,beforeGutter,afterGutter});
    }
    for (auto editor : {&beforePreview_, &afterPreview_}) { editor->SetPalette(colours); editor->ClearMarkers(); }
    std::int64_t delta = 0; size_t marked = 0;
    for (const auto &edit : refactorPreview_.at("edits")) {
        const auto start = edit.at("range").at("start").at("line").get<std::int64_t>();
        const auto end = edit.at("range").at("end").at("line").get<std::int64_t>();
        if (marked++ < 500) {
            // TextEditor's marker colours are backgrounds, including the gutter.
            beforePreview_.AddMarker(static_cast<size_t>(start), beforeGutter, beforeBackground, tr("Before"), tr("Changed line"));
            afterPreview_.AddMarker(static_cast<size_t>(std::max<std::int64_t>(0, start + delta)), afterGutter, afterBackground, tr("After"), tr("Changed line"));
        }
        const auto text = edit.at("newText").get<std::string>();
        delta += static_cast<std::int64_t>(std::count(text.begin(), text.end(), '\n')) - (end - start);
    }
    previewTheme_ = theme_ + ":" + locale_;
}
void Application::changeTask(const std::string &method, Json params) {
    if (refactorFuture_.valid()) return;
    refactorOperation_ = method;
    auto engine = engine_;
    refactorFuture_ = std::async(std::launch::async, [engine, method, params] { return engine->dispatch(method, params); });
}
void Application::beginRename() {
    for (const auto &tab : tabs_) if (tab->path == active_ && !tab->readOnly) {
        auto position = tab->editor.GetMainCursorPosition();
        auto start = tab->editor.FindWordStart(position, true), end = tab->editor.FindWordEnd(position, true);
        refactorName_ = tab->editor.GetSectionText(start, end);
        renameParameters_ = {{"path", active_}, {"expectedRevision", tab->revision},
                             {"position", {{"line", position.line}, {"character", LanguageClient::utf16Length(tab->editor.GetSectionText({position.line, 0}, position))}}}};
        renameOpen_ = true; return;
    }
}
void Application::beginFormat(bool selection) {
    for (const auto &tab : tabs_) if (tab->path == active_ && !tab->readOnly) {
        Json params = {{"path", active_}, {"expectedRevision", tab->revision}, {"tabSize", tab->editor.GetTabSize()}, {"insertSpaces", tab->editor.IsInsertSpacesOnTabs()}};
        if (selection) {
            auto range = tab->editor.GetMainCursorSelection();
            if (range.end < range.start) std::swap(range.start, range.end);
            if (range.start == range.end) return;
            auto position = [&](auto p) { return Json{{"line", p.line}, {"character", LanguageClient::utf16Length(tab->editor.GetSectionText({p.line, 0}, p))}}; };
            params["range"] = {{"start", position(range.start)}, {"end", position(range.end)}};
        }
        changeTask("language.format", params); return;
    }
}
void Application::renamePopup() {
    if (renameOpen_) { ImGui::OpenPopup(tr("Rename symbol")); renameOpen_ = false; }
    if (!ImGui::BeginPopupModal(tr("Rename symbol"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(cyui::dp(400));
    bool enter = ImGui::InputText("##rename-symbol", &refactorName_, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    if (unreal_.value("available", false)) ImGui::TextWrapped("%s", tr("Renames C++ references. Blueprint assets and serialized Unreal names are not modified."));
    ImGui::BeginDisabled(refactorName_.empty() || refactorFuture_.valid());
    if (cyui::primaryButton(tr("Preview changes")) || (enter && !refactorName_.empty())) {
        renameParameters_["newName"] = refactorName_;
        changeTask("language.rename", renameParameters_); ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if (cyui::button(tr("Cancel"))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Application::selectChangeFile(const std::string &path) {
    if (!refactorPlan_.is_object() || refactorFuture_.valid()) return;
    refactorFile_ = path; changeTask("changes.preview", {{"id", refactorPlan_.at("id")}, {"path", path}});
}
void Application::updateSemantic() {
    if (completed(symbolFuture_)) {
        try {
            auto result = symbolFuture_.get();
            if (symbolSentKey_ == symbolWantedKey_) {
                symbolResults_ = std::move(result); symbolError_.clear();
                if (symbolResults_.at("symbols").empty() && symbolAttempts_ < 6) { symbolSentKey_.clear(); symbolChanged_ = ImGui::GetTime()+.7; }
            }
        } catch (const std::exception &e) {
            if (symbolSentKey_ == symbolWantedKey_) symbolError_ = e.what();
        }
    }
    if (!completed(refactorFuture_)) return;
    try {
        auto result = refactorFuture_.get();
        const auto method = refactorOperation_;
        if (method == "changes.preview") {
            refactorPreview_ = result;
            beforePreview_.SetText(result.at("before").get<std::string>());
            afterPreview_.SetText(result.at("after").get<std::string>());
            for (auto editor : {&beforePreview_, &afterPreview_}) {
                editor->SetLanguage(editorLanguage(refactorFile_,unreal_.value("available",false))); editor->SetReadOnlyEnabled(true);
                editor->SetShowMiniMapEnabled(false); editor->SetWordWrapEnabled(true);
                editor->SetShowWhitespacesEnabled(false); editor->SetShowCurrentLineHighlightEnabled(false);
                editor->SetCaretsVisible(false); editor->SetLineSpacing(1.15f);
            }
            styleChangePreview();
        } else if (method == "language.format" || method == "language.rename") {
            refactorPlan_ = result; refactorPreview_ = nullptr;
            if (result.at("files").empty()) { cyui::notify(tr("No changes required"), cyui::Tone::neutral); return; }
            if (method == "language.format") changeTask("changes.apply", {{"id", result.at("id")}});
            else { showPanel("Refactoring", "refactoring"); selectChangeFile(result.at("files")[0].at("path")); }
        } else if (method == "changes.apply" || method == "changes.undo") {
            refactorPlan_ = result;
            // Existing tabs receive transactions in update(); newly affected files
            // become visible without saving the native live-buffer changes.
            auto previous = active_;
            for (const auto &file : result.at("files")) open(file.at("path"), 0, 0, false, false);
            if (!previous.empty()) open(previous);
            cyui::notify(tr(method == "changes.undo" ? "Refactoring undone" : "Changes applied to live buffers"), cyui::Tone::success);
        }
    } catch (const std::exception &e) { error(e.what()); }
}
void Application::refactorPanel() {
    if (!beginPanel("Refactoring", "refactoring")) return;
    if (!refactorPlan_.is_object()) {
        cyui::emptyState(tr("Refactoring"), tr("Rename a C++ symbol to preview changes across its references."), cyui::icon::edit);
        ImGui::End(); return;
    }
    cyui::textDim("%s", refactorPlan_.value("title", "").c_str());
    const auto state = refactorPlan_.value("state", "preview");
    ImGui::BeginDisabled(refactorFuture_.valid() || state != "preview" || refactorPlan_.at("files").empty());
    if (cyui::primaryButton(tr("Apply changes"))) changeTask("changes.apply", {{"id", refactorPlan_.at("id")}});
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(refactorFuture_.valid() || state != "applied");
    if (cyui::button(tr("Undo refactoring"))) changeTask("changes.undo", {{"id", refactorPlan_.at("id")}});
    ImGui::EndDisabled(); ImGui::SameLine();
    if (refactorFuture_.valid()) cyui::spinner("##refactoring");
    else cyui::textDim("%zu %s | %zu %s", refactorPlan_.at("files").size(), tr("files"), refactorPlan_.at("editCount").get<size_t>(), tr("edits"));
    if (state == "applied") cyui::textDim("%s", tr("Changes are in live buffers. Save all to write them to disk."));
    if (ImGui::BeginCombo("##change-file", refactorFile_.c_str())) {
        for (const auto &file : refactorPlan_.at("files")) {
            const auto path = file.at("path").get<std::string>();
            if (ImGui::Selectable(path.c_str(), path == refactorFile_)) selectChangeFile(path);
        }
        ImGui::EndCombo();
    }
    if (refactorPreview_.is_object()) {
        if (previewTheme_ != theme_ + ":" + locale_) styleChangePreview();
        if (ImGui::BeginTable("##change-preview", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted(tr("Before"));
            { cyui::ScopedFont mono(cyui::monoFont()); beforePreview_.Render("##before-preview"); }
            ImGui::TableNextColumn(); ImGui::TextUnformatted(tr("After"));
            { cyui::ScopedFont mono(cyui::monoFont()); afterPreview_.Render("##after-preview"); }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
} // namespace cycode
