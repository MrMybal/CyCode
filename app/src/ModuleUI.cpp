#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include <cmath>
#include <imgui_internal.h>

namespace cycode {
void Application::viewFilter(const char *id, cyui::TextFilter &filter) {
    viewFilters_[ImGui::GetCurrentWindow()->RootWindow->ID] = &filter;
    cyui::filterBox(id, filter);
}
void Application::optionsProperties() {
    viewFilter("##options-filter", optionsFilter_);
    std::vector<cyui::Property> properties;
    auto add = [&](const char *key, cyui::PropertyValue value) -> cyui::Property & {
        cyui::Property property; property.id = uiId(std::string("options/") + key);
        property.label = key; property.value = std::move(value);
        properties.push_back(std::move(property)); return properties.back();
    };
    add("Language", locale_).choices = {"en", "fr"};
    auto &theme = add("Theme", theme_); theme.choices = {"graphite"};
    for (const auto &entry : cyui::builtInThemes()) theme.choices.push_back(entry.key);
    auto &scale = add("Text size", double(textScale_)); scale.bounded = true; scale.minimum = .8; scale.maximum = 2.;
    add("clangd", clangd_);
    const auto validate = [&](cyui::ItemId id, const cyui::PropertyValue &value) -> std::string {
        if (id == uiId("options/Text size")) {
            const auto size = std::get<double>(value);
            if (!std::isfinite(size) || size < .8 || size > 2.) return "Text size must be between 0.8 and 2.0";
        }
        if (id == uiId("options/clangd") && std::get<std::string>(value).empty()) return "Select a clangd executable";
        return {};
    };
    // Drafts and previews never change settings; only a committed proposal does.
    // Hidden fields still participate so CyUI can emit cancellation.
    const auto edits = cyui::inspector("##options-properties", optionsInspector_, properties,
        optionsRevision_, validate, {&optionsFilter_, "Options"});
    for (const auto &proposal : edits) if (proposal.edit.committed()) {
        const auto &value = proposal.edit.value;
        if (proposal.id == uiId("options/Language")) { locale_ = std::get<std::string>(value); cyui::setLocale(locale_.c_str()); }
        else if (proposal.id == uiId("options/Theme")) { theme_ = std::get<std::string>(value); applyEditorTheme(theme_); }
        else if (proposal.id == uiId("options/Text size")) {
            textScale_ = float(std::get<double>(value)); ImGui::GetStyle().FontScaleMain = textScale_;
        } else if (proposal.id == uiId("options/clangd")) clangd_ = std::get<std::string>(value);
        ++optionsRevision_;
    }
}
void Application::problemRows() {
    const auto diagnostics = languageState_.value("diagnostics", Json::object());
    if (problemBuild_ != buildDiagnostics_ || problemLanguage_ != diagnostics) {
        problemBuild_ = buildDiagnostics_; problemLanguage_ = diagnostics; problems_ = {};
        for (const auto &entry : buildDiagnostics_) {
            const auto path = entry.value("path", "");
            const auto level = entry.value("severity", "error");
            problems_.add("build/" + entry.dump(), level == "warning" ? cyui::LogLevel::warning :
                level == "error" ? cyui::LogLevel::error : cyui::LogLevel::info,
                path.empty() ? entry.value("displayPath", "") : path,
                entry.value("code", "") + " " + entry.value("message", ""),
                {path, entry.value("line", 1), entry.value("column", 1), false});
        }
        for (const auto &[uri, entries] : diagnostics.items()) {
            std::string path;
            try { path = pathUtf8(engine_->resolve(pathUtf8(LanguageClient::fromUri(uri))).lexically_relative(engine_->workspace())); }
            catch (const std::exception &) { continue; }
            for (const auto &entry : entries) {
                const auto &position = entry.at("range").at("start");
                const auto severity = entry.value("severity", 1);
                const auto code = entry.value("code", Json());
                problems_.add("lsp/" + uri + "/" + entry.dump(), severity == 1 ? cyui::LogLevel::error :
                    severity == 2 ? cyui::LogLevel::warning : cyui::LogLevel::info, path,
                    (code.is_string() ? code.get<std::string>() : code.is_number() ? code.dump() : "") +
                    " " + entry.value("message", ""),
                    {path, position.at("line").get<int>() + 1, position.at("character").get<int>() + 1, true});
            }
        }
    }
    viewFilter("##problems-filter", problemsFilter_);
    const auto &rows = problems_.matching(problemsFilter_);
    if (problems_.rows.empty())
        cyui::emptyState(tr("No diagnostics"), tr("Diagnostics from enabled language servers appear here."), cyui::icon::check);
    const auto requested = cyui::diagnosticList("##diagnostics", rows);
    if (requested && problems_.locations.contains(requested)) {
        const auto location = problems_.locations.at(requested);
        if (!location.path.empty()) open(location.path, location.line, location.column, location.utf16);
    }
}
} // namespace cycode
