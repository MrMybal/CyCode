#pragma once
#include <string>
#include <string_view>
namespace cycode {
// Display/JSON boundary only. ProcessResult and structured protocol bytes remain
// unchanged. Windows console mode decodes malformed UTF-8 lines with the system
// OEM code page; every other malformed sequence gets explicit U+FFFD characters.
std::string processOutputText(std::string_view, bool windowsConsole = false);
} // namespace cycode
