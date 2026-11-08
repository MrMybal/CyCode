#include "cycode/Paths.h"
namespace cycode {
std::string pathUtf8(const std::filesystem::path &path) {
    const auto value = path.generic_u8string();
    return {value.begin(), value.end()};
}
std::filesystem::path utf8Path(const std::string &text) { return std::filesystem::u8path(text); }
} // namespace cycode
