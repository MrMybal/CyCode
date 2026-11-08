#include "cycode/ProcessText.h"
#include <cstdint>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
constexpr size_t sequence(std::string_view text, size_t at) {
    const auto a = static_cast<unsigned char>(text[at]);
    if (a < 0x80) return 1;
    const size_t count = a >= 0xc2 && a <= 0xdf ? 2 : a >= 0xe0 && a <= 0xef ? 3 : a >= 0xf0 && a <= 0xf4 ? 4 : 0;
    if (!count || text.size() - at < count) return 0;
    for (size_t i = 1; i < count; ++i)
        if ((static_cast<unsigned char>(text[at+i]) & 0xc0) != 0x80) return 0;
    const auto b = static_cast<unsigned char>(text[at+1]);
    if ((a == 0xe0 && b < 0xa0) || (a == 0xed && b >= 0xa0) ||
        (a == 0xf0 && b < 0x90) || (a == 0xf4 && b >= 0x90)) return 0;
    return count;
}
constexpr bool valid(std::string_view text) {
    for (size_t at = 0; at < text.size();) {
        const auto count = sequence(text, at);
        if (!count) return false;
        at += count;
    }
    return true;
}
static_assert(valid("ordinary text"));
static_assert(valid("\xc3\xa9\xf0\x9f\x98\x80"));
static_assert(!valid("\xc0\xaf"));
static_assert(!valid("\xed\xa0\x80"));
static_assert(!valid("\xf4\x90\x80\x80"));
static_assert(!valid("\xf0\x9f"));
void appendLine(std::string &output, std::string_view line, bool console) {
    if (valid(line)) {
        output.append(line);
        return;
    }
#ifdef _WIN32
    if (console && line.size() <= size_t((std::numeric_limits<int>::max)())) {
        const auto count = MultiByteToWideChar(CP_OEMCP, 0, line.data(), int(line.size()), nullptr, 0);
        if (count) {
            std::wstring wide(size_t(count), L'\0');
            if (MultiByteToWideChar(CP_OEMCP, 0, line.data(), int(line.size()), wide.data(), count) == count) {
                const auto size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    const auto offset = output.size();
                    output.resize(offset + size_t(size));
                    if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, output.data() + offset, size, nullptr, nullptr) == size)
                        return;
                    output.resize(offset);
                }
            }
        }
    }
#else
    (void)console;
#endif
    for (size_t at = 0; at < line.size();) {
        const auto count = sequence(line, at);
        if (count) {
            output.append(line.substr(at, count));
            at += count;
        } else {
            output.append("\xef\xbf\xbd");
            ++at;
        }
    }
}
} // namespace
std::string processOutputText(std::string_view input, bool console) {
    std::string output;
    output.reserve(input.size());
    while (!input.empty()) {
        const auto end = input.find('\n');
        const auto count = end == input.npos ? input.size() : end + 1;
        appendLine(output, input.substr(0, count), console);
        input.remove_prefix(count);
    }
    return output;
}
} // namespace cycode
