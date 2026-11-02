#include "cycode/FileIcons.h"
#include "cycode/EditorTheme.h"
#include "cycode/Process.h"
#include <algorithm>
#include <cyui/CyUI.h>
#include <imgui_internal.h>
namespace cycode {
void drawFileIcon(ImDrawList *draw, ImVec2 pos, float size, const std::string &path, const std::string &theme,
                  bool folder) {
    const auto &p = cyui::palette();
    const auto syntax = syntaxColours(theme);
    auto point = [&](float x, float y) { return ImVec2(pos.x + x * size, pos.y + y * size); };
    const float stroke = cyui::dp(1.1f);
    if (folder) {
        const ImVec2 outline[]{point(.08f, .24f), point(.4f, .24f),  point(.51f, .36f),
                               point(.94f, .36f), point(.94f, .85f), point(.08f, .85f)};
        draw->AddPolyline(outline, 6, p.textDim, ImDrawFlags_Closed, stroke);
        return;
    }
    auto ext = pathUtf8(utf8Path(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    const char *badge = nullptr;
    ImU32 colour = p.textDim;
    if (ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx" || ext == ".inl") {
        badge = ".H";
        colour = syntax.type;
    } else if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
        badge = "C+";
        colour = syntax.keyword;
    } else if (ext == ".c") {
        badge = "C";
        colour = syntax.keyword;
    } else if (ext == ".cs" || ext == ".csproj") {
        badge = "C#";
        colour = p.success;
    } else if (ext == ".rs") {
        badge = "R";
        colour = p.warning;
    } else if (ext == ".java") {
        badge = "J";
        colour = p.warning;
    } else if (ext == ".js" || ext == ".jsx") {
        badge = "JS";
        colour = p.warning;
    } else if (ext == ".ts" || ext == ".tsx") {
        badge = "TS";
        colour = syntax.keyword;
    } else if (ext == ".html" || ext == ".htm") {
        badge = "<>";
        colour = p.warning;
    } else if (ext == ".css" || ext == ".scss") {
        badge = "#";
        colour = syntax.type;
    } else if (ext == ".json" || ext == ".uplugin") {
        badge = "{}";
        colour = syntax.type;
    } else if (ext == ".hlsl" || ext == ".hlsli" || ext == ".glsl" || ext == ".vert" || ext == ".frag" ||
               ext == ".comp" || ext == ".usf" || ext == ".ush") {
        badge = "S";
        colour = p.warning;
    } else if (ext == ".py") {
        badge = "Py";
        colour = syntax.keyword;
    } else if (ext == ".cmake" || utf8Path(path).filename() == "CMakeLists.txt") {
        badge = "CM";
        colour = p.success;
    } else if (ext == ".uproject") {
        draw->AddCircle(point(.5f, .5f), size * .44f, p.info, 16, stroke);
        badge = "U";
        colour = p.info;
    } else if (ext == ".sln" || ext == ".vcxproj") {
        colour = syntax.type;
        draw->AddRect(point(.08f, .08f), point(.43f, .43f), colour, cyui::dp(1), 0, stroke);
        draw->AddRect(point(.57f, .08f), point(.92f, .43f), colour, cyui::dp(1), 0, stroke);
        draw->AddRect(point(.08f, .57f), point(.43f, .92f), colour, cyui::dp(1), 0, stroke);
        draw->AddRect(point(.57f, .57f), point(.92f, .92f), colour, cyui::dp(1), 0, stroke);
        return;
    }
    if (badge) {
        auto font = cyui::boldFont();
        const float fontSize = size * .7f;
        auto text = font->CalcTextSizeA(fontSize, FLT_MAX, 0, badge);
        draw->AddText(font, fontSize, point((1 - text.x / size) * .5f, (1 - text.y / size) * .5f), colour,
                      badge);
        return;
    }
    const ImVec2 outline[]{point(.2f, .08f), point(.6f, .08f), point(.83f, .31f), point(.83f, .92f),
                           point(.2f, .92f)};
    draw->AddPolyline(outline, 5, colour, ImDrawFlags_Closed, stroke);
    draw->AddLine(point(.6f, .08f), point(.6f, .31f), colour, stroke);
    draw->AddLine(point(.6f, .31f), point(.83f, .31f), colour, stroke);
}
bool fileSelectable(const std::string &name, const std::string &path, bool selected,
                    const std::string &theme) {
    ImGui::PushID(path.c_str());
    const float size = std::max(cyui::dp(16), ImGui::GetTextLineHeight());
    const bool clicked = ImGui::Selectable("##file", selected, 0, ImVec2(0, size));
    auto pos = ImGui::GetItemRectMin(), end = ImGui::GetItemRectMax();
    auto draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(pos, end, true);
    drawFileIcon(draw, pos, size, path, theme);
    draw->AddText(ImVec2(pos.x + size + cyui::dp(5), pos.y + (size - ImGui::GetTextLineHeight()) * .5f),
                  ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
    draw->PopClipRect();
    ImGui::PopID();
    return clicked;
}
bool fileTreeNode(const std::string &name, const std::string &path, ImGuiTreeNodeFlags flags,
                  const std::string &theme, bool folder) {
    const auto indent = ImGui::GetTreeNodeToLabelSpacing();
    bool open = ImGui::TreeNodeEx(("     " + name).c_str(), flags);
    auto pos = ImGui::GetItemRectMin();
    pos.x += indent;
    auto height = ImGui::GetItemRectSize().y;
    auto size = std::min(height, cyui::dp(16));
    pos.y += (height - size) * .5f;
    drawFileIcon(ImGui::GetWindowDrawList(), pos, size, path, theme, folder);
    return open;
}
std::string documentTitle(const std::string &path) {
    return "     " + pathUtf8(utf8Path(path).filename()) + "###document:" + path;
}
void documentIcon(const std::string &path, const std::string &theme) {
    auto window = ImGui::GetCurrentWindow();
    auto pos = window->TitleBarRect().Min;
    auto clip = window->TitleBarRect();
    auto draw = window->DrawList;
    float height = window->TitleBarRect().GetHeight();
    if (window->DockNode && window->DockNode->TabBar) {
        auto bar = window->DockNode->TabBar;
        auto tab = ImGui::TabBarFindTabByID(bar, window->TabId);
        if (!tab || tab->Width <= 0)
            return;
        pos = ImVec2(bar->BarRect.Min.x + tab->Offset - bar->ScrollingAnim, bar->BarRect.Min.y);
        height = bar->BarRect.GetHeight();
        clip = bar->BarRect;
        draw = window->DockNode->HostWindow->DrawList;
    }
    const auto size = std::min(height - cyui::dp(2), cyui::dp(16));
    pos.x += ImGui::GetStyle().FramePadding.x;
    pos.y += (height - size) * .5f;
    draw->PushClipRect(clip.Min, clip.Max, false);
    drawFileIcon(draw, pos, size, path, theme);
    draw->PopClipRect();
}
} // namespace cycode
