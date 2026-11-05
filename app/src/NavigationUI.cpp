#include "cycode/Application.h"
#include "cycode/NavigationLocations.h"
#include <algorithm>

namespace cycode {
namespace {
std::string navigationCommand(const std::string &method) {
    return "navigate." + method.substr(9);
}
} // namespace
bool Application::navigationCurrent(const Json &report) const {
    if (!report.contains("path") || !report.contains("documentId") || !report.contains("backend") ||
        !report.contains("epoch") || !report.contains("revision"))
        return false;
    const auto servers = languageState_.find("servers");
    if (servers == languageState_.end())
        return false;
    const auto server = servers->find(report.at("backend").get_ref<const std::string &>());
    if (server == servers->end() || !server->value("enabled", false) || !server->value("ready", false) ||
        server->value("epoch", uint64_t(0)) != report.at("epoch"))
        return false;
    for (const auto &tab : tabs_)
        if (tab->path == report.at("path").get_ref<const std::string &>() && !tab->image)
            return tab->identity == report.at("documentId") && tab->revision == report.at("revision");
    return false;
}
void Application::navigate(bool references) {
    requestNavigation(references ? "language.references" : "language.definition");
}
void Application::requestNavigation(const std::string &method) {
    if (navigationFuture_.valid() || !locationRequest(method))
        return;
    for (const auto &tab : tabs_) {
        if (tab->path != active_ || tab->image || !engine_->supportsLanguage(tab->path) ||
            tab->path.starts_with("debug://"))
            continue;
        navigationPosition_ = tab->editor.GetMainCursorPosition();
        navigationPath_ = tab->path;
        navigationReferences_ = method == "language.references";
        navigationMethod_ = method;
        navigation_ = Json::object();
        navigationModel_.set(Json::array());
        navigationSelected_ = -1;
        navigationFilter_.query.clear();
        navigationError_.clear();
        const auto prefix = tab->editor.GetSectionText({navigationPosition_.line, 0}, navigationPosition_);
        Json parameters = {
            {"path", tab->path},
            {"includeExternal", true},
            {"expectedRevision", tab->revision},
            {"expectedDocumentId", tab->identity},
            {"position",
             {{"line", navigationPosition_.line}, {"character", LanguageClient::utf16Length(prefix)}}}};
        const auto servers = languageState_.find("servers");
        if (servers != languageState_.end()) {
            const auto server = servers->find(engine_->languageBackendFor(tab->path));
            if (server != servers->end() && server->contains("epoch"))
                parameters["expectedEpoch"] = server->at("epoch");
        }
        const auto ticket = ++navigationTicket_;
        auto engine = engine_;
        navigationFuture_ = std::async(std::launch::async, [engine, parameters, method, ticket] {
            try {
                return Json{{"ticket", ticket}, {"result", engine->dispatch(method, parameters)}};
            } catch (const std::exception &e) {
                return Json{{"ticket", ticket}, {"error", e.what()}};
            }
        });
        showPanel("References", "references");
        return;
    }
    cyui::notify(tr("Open a source file with a language server."), cyui::Tone::neutral);
}
void Application::updateNavigation() {
    if (!navigationFuture_.valid() ||
        navigationFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        auto reply = navigationFuture_.get();
        if (reply.at("ticket") != navigationTicket_)
            return;
        if (reply.contains("error")) {
            navigationError_ = reply.at("error");
            return;
        }
        languageState_ = engine_->languageStatus();
        if (!navigationCurrent(reply.at("result"))) {
            navigationError_ = tr("Navigation context changed. Run the query again.");
            return;
        }
        navigation_ = std::move(reply.at("result"));
        navigationError_.clear();
        navigationModel_.set(navigation_.at("locations"));
        const auto &locations = navigation_.at("locations");
        bool sameCaret = false;
        for (const auto &tab : tabs_)
            if (tab->path == navigationPath_ && active_ == navigationPath_)
                sameCaret = tab->editor.GetMainCursorPosition() == navigationPosition_;
        if (locations.empty())
            cyui::notify(tr("No locations found"), cyui::Tone::neutral);
        if (!navigationReferences_ && locations.size() == 1 && sameCaret) {
            const auto &row = locations.front();
            const auto &start = row.at("range").at("start");
            open(row.at("path"), start.at("line").get<size_t>() + 1, start.at("character").get<size_t>() + 1,
                 true);
        }
    } catch (const std::exception &e) {
        navigationError_ = e.what();
    }
}
void Application::referencesPanel() {
    if (!beginPanel("References", "references"))
        return;
    const auto command = findCommand(navigationCommand(navigationMethod_));
    cyui::textDim("%s", tr(command ? command->title.c_str() : "Symbol navigation"));
    if (navigationFuture_.valid()) {
        ImGui::SameLine();
        cyui::spinner("##navigation");
    }
    ImGui::BeginDisabled(!commandEnabled(navigationCommand(navigationMethod_)));
    if (cyui::button(tr("Refresh at caret")))
        requestNavigation(navigationMethod_);
    ImGui::EndDisabled();
    if (!navigationError_.empty())
        cyui::textColoured(cyui::palette().warning, "%s", navigationError_.c_str());
    if (!navigation_.contains("locations")) {
        cyui::emptyState(
            tr("Symbol navigation"),
            tr("Use Navigate to find definitions, declarations, types, implementations or references."),
            cyui::icon::search);
        ImGui::End();
        return;
    }
    const bool current = navigationCurrent(navigation_);
    if (!current)
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Navigation context changed. Run the query again."));
    const auto &source = navigation_.at("position");
    cyui::textDim("%s:%llu:%llu", navigation_.at("path").get_ref<const std::string &>().c_str(),
                  static_cast<unsigned long long>(source.at("line").get<uint64_t>() + 1),
                  static_cast<unsigned long long>(source.at("character").get<uint64_t>() + 1));
    viewFilter("##navigation-filter", navigationFilter_);
    const auto &matches = navigationModel_.matching(navigationFilter_);
    const auto &locations = navigation_.at("locations");
    cyui::textDim("%zu / %zu %s", matches.size(), locations.size(), tr("matches"));
    if (navigation_.value("invalid", size_t(0)))
        cyui::textDim("%s: %zu", tr("Invalid locations ignored"), navigation_.at("invalid").get<size_t>());
    if (navigation_.value("outsideWorkspace", size_t(0)))
        cyui::textDim("%s: %zu", tr("Unavailable locations"),
                      navigation_.at("outsideWorkspace").get<size_t>());
    if (navigation_.value("truncated", false))
        cyui::textDim("%s", tr("Results limited"));
    bool moved = false;
    ImGui::BeginDisabled(matches.empty());
    if (cyui::button(tr("Previous location"))) {
        navigationSelected_ = navigationModel_.adjacent(navigationFilter_, navigationSelected_, true);
        moved = true;
    }
    ImGui::SameLine();
    if (cyui::button(tr("Next location"))) {
        navigationSelected_ = navigationModel_.adjacent(navigationFilter_, navigationSelected_, false);
        moved = true;
    }
    ImGui::SameLine();
    if (cyui::button(tr("Copy locations"))) {
        std::string text;
        for (const auto index : matches) {
            text += navigationModel_.label(index);
            text += '\n';
        }
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::EndDisabled();
    const bool keys =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy) &&
        !ImGui::IsAnyItemActive();
    if (keys && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) {
        navigationSelected_ = navigationModel_.adjacent(navigationFilter_, navigationSelected_,
                                                        ImGui::IsKeyPressed(ImGuiKey_UpArrow));
        moved = true;
    }
    const bool enter = keys && ImGui::IsKeyPressed(ImGuiKey_Enter);
    if (std::find(matches.begin(), matches.end(), static_cast<size_t>(navigationSelected_)) == matches.end())
        navigationSelected_ = matches.empty() ? -1 : static_cast<int>(matches.front());
    int chosen = -1;
    ImGui::BeginChild("##navigation-locations", {}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(matches.size()));
    const auto selected = std::find(matches.begin(), matches.end(), static_cast<size_t>(navigationSelected_));
    if ((moved || enter) && selected != matches.end())
        clipper.IncludeItemByIndex(static_cast<int>(selected - matches.begin()));
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto index = matches.at(static_cast<size_t>(row));
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Selectable(navigationModel_.label(index).c_str(),
                                  navigationSelected_ == static_cast<int>(index))) {
                navigationSelected_ = static_cast<int>(index);
                if (current)
                    chosen = static_cast<int>(index);
            }
            if (enter && current && navigationSelected_ == static_cast<int>(index))
                chosen = static_cast<int>(index);
            if (moved && navigationSelected_ == static_cast<int>(index))
                ImGui::SetScrollHereY();
            ImGui::PopID();
        }
    if (matches.empty())
        cyui::textDim("%s", tr("No matching locations"));
    ImGui::EndChild();
    ImGui::End();
    if (chosen >= 0) {
        const auto &row = locations.at(static_cast<size_t>(chosen));
        const auto &start = row.at("range").at("start");
        open(row.at("path"), start.at("line").get<size_t>() + 1, start.at("character").get<size_t>() + 1,
             true);
    }
}
} // namespace cycode
