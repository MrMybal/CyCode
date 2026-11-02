#pragma once
#include <imgui.h>
#include <string>
namespace cycode {
// Original vector/file-type badges. No OS symbol font or image texture required.
void drawFileIcon(ImDrawList *, ImVec2 position, float size, const std::string &path,
                  const std::string &theme, bool folder = false);
bool fileSelectable(const std::string &name, const std::string &path, bool selected,
                    const std::string &theme);
bool fileTreeNode(const std::string &name, const std::string &path, ImGuiTreeNodeFlags flags,
                  const std::string &theme, bool folder);
std::string documentTitle(const std::string &path);
void documentIcon(const std::string &path, const std::string &theme);
} // namespace cycode
