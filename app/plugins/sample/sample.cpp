#include "cycode/PluginAPI.h"
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>
static const char *stats(const char *input) {
    static std::string result;
    try {
        auto text = nlohmann::json::parse(input).value("text", std::string{});
        size_t words = 0, lines = text.empty() ? 0 : 1;
        bool word = false;
        for (unsigned char c : text) {
            if (c == '\n')
                ++lines;
            bool next = !std::isspace(c);
            if (next && !word)
                ++words;
            word = next;
        }
        result = nlohmann::json({{"words", words}, {"lines", lines}, {"bytes", text.size()}}).dump();
    } catch (const std::exception &e) {
        result = nlohmann::json({{"error", e.what()}}).dump();
    }
    return result.c_str();
}
extern "C" CYCODE_PLUGIN_EXPORT int cycode_plugin_init(const CyCodeHostV1 *host) {
    if (!host || host->abi_version != CYCODE_PLUGIN_ABI || host->struct_size < sizeof(CyCodeHostV1))
        return 1;
    host->add_command(host->context, "sample.text_statistics", "Text statistics", stats);
    return 0;
}
