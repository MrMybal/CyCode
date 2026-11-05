#include "cycode/CompilerLocation.h"
#include "cycode/Paths.h"
#include <charconv>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace cycode {
namespace {
std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == value.npos)
        return {};
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}
std::optional<int> number(std::string_view text, int maximum) {
    int result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result < 1 || result > maximum)
        return std::nullopt;
    return result;
}
struct Location {
    std::string_view path;
    int line = 1, column = 1;
    const char *format = "colon";
};
std::optional<Location> parse(std::string_view text) {
    text = trim(text);
    // MSBuild's parallel-project prefix is not part of the filename.
    if (const auto end = text.find('>'); end != text.npos && end > 0 && end < 12 &&
                                         text.substr(0, end).find_first_not_of("0123456789") == text.npos)
        text = trim(text.substr(end + 1));
    for (const auto prefix : {std::string_view("-->"), std::string_view(":::"), std::string_view("ERROR:"),
                              std::string_view("WARNING:")})
        if (text.starts_with(prefix)) {
            text = trim(text.substr(prefix.size()));
            break;
        }
    for (size_t at = 0; at < text.size(); ++at) {
        if (text[at] == '(') {
            const auto end = text.find(')', at + 1);
            if (end == text.npos || !trim(text.substr(end + 1)).starts_with(':'))
                continue;
            const auto coordinates = text.substr(at + 1, end - at - 1);
            const auto comma = coordinates.find(',');
            const auto line = number(trim(coordinates.substr(0, comma)), 10000000);
            const auto column = comma == coordinates.npos
                                    ? std::optional<int>(1)
                                    : number(trim(coordinates.substr(comma + 1)), 1000000);
            if (line && column)
                return Location{text.substr(0, at), *line, *column, "parentheses"};
        } else if (text[at] == ':') {
            auto end = at + 1;
            while (end < text.size() && text[end] >= '0' && text[end] <= '9')
                ++end;
            const auto line = number(text.substr(at + 1, end - at - 1), 10000000);
            // rustc ends the location at EOL; GCC/clang/javac add a colon.
            if (!line || (end != text.size() && text[end] != ':'))
                continue;
            int column = 1;
            if (end < text.size()) {
                auto last = end + 1;
                while (last < text.size() && text[last] >= '0' && text[last] <= '9')
                    ++last;
                if (last > end + 1) {
                    const auto candidate = number(text.substr(end + 1, last - end - 1), 1000000);
                    if (!candidate || (last != text.size() && text[last] != ':'))
                        continue;
                    column = *candidate;
                }
            }
            return Location{text.substr(0, at), *line, column, "colon"};
        }
    }
    return std::nullopt;
}
} // namespace
nlohmann::json compilerLocation(const nlohmann::json &parameters, const std::filesystem::path &root,
                                const std::function<std::filesystem::path(const std::string &)> &resolve) {
    if (!parameters.is_object() || !parameters.contains("text") || !parameters.at("text").is_string())
        throw std::runtime_error("build.location requires a text line");
    for (const auto &[key, value] : parameters.items())
        if (key != "text" && key != "directory")
            throw std::runtime_error("Unknown build.location parameter");
    const auto &text = parameters.at("text").get_ref<const std::string &>();
    if (text.size() > 16384 || text.find_first_of("\n\0", 0, 2) != text.npos)
        throw std::runtime_error("Compiler location requires one line of at most 16 KiB");
    for (unsigned char value : text)
        if ((value < 32 && value != '\t' && value != '\r') || value == 127)
            throw std::runtime_error("Compiler location contains control bytes");
    const auto directory = parameters.value("directory", std::string("."));
    if (directory.empty() || directory.size() > 8192 || directory.find('\0') != directory.npos)
        throw std::runtime_error("Invalid compiler working directory");
    const auto base = resolve(directory);
    if (!std::filesystem::is_directory(base))
        throw std::runtime_error("Compiler working directory is missing");
    auto unmapped = [](const char *reason) { return nlohmann::json{{"mapped", false}, {"reason", reason}}; };
    const auto parsed = parse(text);
    if (!parsed)
        return unmapped("No supported source location");
    auto source = trim(parsed->path);
    if (source.size() >= 2 && ((source.front() == '"' && source.back() == '"') ||
                               (source.front() == '\'' && source.back() == '\'')))
        source = source.substr(1, source.size() - 2);
    if (source.empty() || source.find("://") != source.npos ||
        source.find_first_not_of("0123456789") == source.npos)
        return unmapped("No explicit file path");
    try {
        const auto input = utf8Path(std::string(source));
        if (input.has_root_name() && !input.is_absolute())
            return unmapped("Drive-relative path is ambiguous");
        const auto path = resolve(pathUtf8(input.is_absolute() ? input : base / input));
        if (!std::filesystem::is_regular_file(path))
            return unmapped("Source file is missing");
        return {{"mapped", true},           {"path", pathUtf8(path.lexically_relative(root))},
                {"line", parsed->line},     {"column", parsed->column},
                {"format", parsed->format}, {"reason", ""}};
    } catch (const std::exception &) {
        return unmapped("Source path is outside the workspace or unavailable");
    }
}
} // namespace cycode
