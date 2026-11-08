#include "cycode/SnippetCatalogue.h"
#include "cycode/Languages.h"
#include "cycode/TextFile.h"
#include "cycode/PluginManifest.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace cycode {
namespace {
using Json = nlohmann::json;
std::string stringValue(const Json &value, size_t limit, bool singleLine) {
    if (!value.is_string()) throw std::runtime_error("Snippet catalogue: expected a string");
    const auto &text = value.get_ref<const std::string &>();
    if (text.size() > limit || text.find('\0') != text.npos ||
        (singleLine && std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; })))
        throw std::runtime_error("Snippet catalogue: invalid or oversized text");
    return text;
}
std::string bodyValue(const Json &value) {
    if (value.is_string()) return stringValue(value, SnippetCatalogueLimits::body, false);
    if (!value.is_array() || value.size() > SnippetCatalogueLimits::lines)
        throw std::runtime_error("Snippet catalogue: body must be text or at most 256 lines");
    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
        const auto line = stringValue(value[i], SnippetCatalogueLimits::body, false);
        if (line.size() + (i ? 1 : 0) > SnippetCatalogueLimits::body - result.size())
            throw std::runtime_error("Snippet catalogue: body exceeds 8 KiB");
        if (i) result += '\n';
        result += line;
    }
    return result;
}
}
std::string readSnippetCatalogue(const std::filesystem::path &file) {
    std::error_code error;
    const auto state = std::filesystem::status(file, error);
    if (error == std::errc::no_such_file_or_directory || (!error && !std::filesystem::exists(state))) return "{}";
    if (error || !std::filesystem::is_regular_file(state)) throw std::runtime_error("Snippet catalogue is not a readable regular file");
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read snippet catalogue");
    std::string source(SnippetCatalogueLimits::bytes + 1, '\0');
    input.read(source.data(), static_cast<std::streamsize>(source.size()));
    const auto size = input.gcount();
    if (input.bad() || (!input.eof() && input.fail()) || size > SnippetCatalogueLimits::bytes)
        throw std::runtime_error("Snippet catalogue read failed or exceeds 256 KiB");
    source.resize(static_cast<size_t>(size));
    return source;
}
std::string snippetLanguage(const std::string &path) {
    auto language = languageId(path);
    if (!language.empty()) return language;
    auto extension = pathUtf8(utf8Path(path).extension());
    if (!extension.empty()) extension.erase(0, 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return char(c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c);
    });
    if (extension.empty() || extension == "txt") return "plaintext";
    if (extension == "md" || extension == "markdown") return "markdown";
    return extension;
}
Json snippetCatalogueItems(std::string_view source, const std::string &language) {
    if (source.size() > SnippetCatalogueLimits::bytes) throw std::runtime_error("Snippet catalogue exceeds 256 KiB");
    size_t nodes=0;
    const auto data = Json::parse(source,[&](int depth,Json::parse_event_t,Json &) {
        if (depth>SnippetCatalogueLimits::depth || ++nodes>SnippetCatalogueLimits::nodes)
            throw std::runtime_error("Snippet catalogue JSON exceeds its depth/node limit");
        return true;
    });
    if (!data.is_object() || data.size() > SnippetCatalogueLimits::definitions)
        throw std::runtime_error("Snippet catalogue must contain at most 256 named definitions");
    Json items = Json::array(); bool truncated = false;
    for (const auto &[name, entry] : data.items()) {
        if (name.empty()) throw std::runtime_error("Snippet catalogue: empty definition name");
        (void)stringValue(name, 160, true);
        if (!entry.is_object() || !entry.contains("body") || !entry.contains("prefix"))
            throw std::runtime_error("Snippet catalogue: every definition needs prefix and body");
        for (const auto &[key, value] : entry.items())
            if (key != "prefix" && key != "body" && key != "scope" && key != "description" && key != "isFileTemplate")
                throw std::runtime_error("Snippet catalogue: unknown definition property");
        if (entry.contains("isFileTemplate") && (!entry.at("isFileTemplate").is_boolean() || entry.at("isFileTemplate").get<bool>()))
            throw std::runtime_error("Snippet catalogue: file templates are not insertion snippets");
        const auto body = bodyValue(entry.at("body"));
        const auto scope = entry.contains("scope") ? stringValue(entry.at("scope"), 1024, true) : "";
        const auto description = entry.contains("description") ? stringValue(entry.at("description"), 1024, true) : "";
        const auto prefixes = entry.at("prefix").is_array() ? entry.at("prefix") : Json::array({entry.at("prefix")});
        if (prefixes.empty() || prefixes.size() > SnippetCatalogueLimits::prefixes)
            throw std::runtime_error("Snippet catalogue: expected one to sixteen prefixes");
        for (const auto &value : prefixes) {
            const auto prefix = stringValue(value, SnippetCatalogueLimits::prefix, true);
            if (prefix.empty()) throw std::runtime_error("Snippet catalogue: empty prefix");
            if (!snippetScopeMatches(scope, language)) continue;
            if (items.size() == SnippetCatalogueLimits::items) { truncated = true; continue; }
            items.push_back({{"label", prefix + " - " + name}, {"filterText", prefix},
                {"sortText", prefix + " " + name}, {"insertText", body}, {"insertTextFormat", 2},
                {"insertTextMode", 2}, {"detail", description}, {"kind", 15}});
        }
    }
    return {{"items", std::move(items)}, {"isIncomplete", truncated}};
}
void validateSnippetContribution(const Json &info) {
    if (!info.is_object() || !info.contains("id") || !info.at("id").is_string() ||
        !pluginIdentifier(info.at("id").get_ref<const std::string&>()) ||
        !info.contains("definitions") || !info.at("definitions").is_object() || info.at("definitions").empty())
        throw std::runtime_error("Snippet contribution needs an ID and nonempty definitions");
    for (const auto &[key,value] : info.items())
        if (key!="id" && key!="title" && key!="definitions" && key!="order")
            throw std::runtime_error("Unsupported snippet contribution property");
    if (info.contains("title")) (void)stringValue(info.at("title"),160,true);
    if (info.contains("order") && (!info.at("order").is_number_integer() ||
        info.at("order") < -10000 || info.at("order") > 10000))
        throw std::runtime_error("Snippet catalogue order must be an integer from -10000 to 10000");
    (void)snippetCatalogueItems(info.at("definitions").dump(),""); // Validate excluded scopes too.
}
Json mergedSnippetItems(std::string_view workspace, const Json &catalogues, const std::string &language,std::string_view user) {
    if (!catalogues.is_array() || !snippetRegistryFits(catalogues.size(),0))
        throw std::runtime_error("Invalid plugin snippet snapshot");
    Json items=Json::array();bool incomplete=false;size_t bytes=0;
    const auto append=[&](const Json &result,const Json &origin) {
        incomplete=incomplete || result.at("isIncomplete").get<bool>();
        for (auto item : result.at("items")) {
            if (items.size()==SnippetCatalogueLimits::items) { incomplete=true;continue; }
            item["cycodeSnippetSource"]=origin;
            if (origin.at("kind")=="plugin") {
                item["label"]=item.at("label").get<std::string>()+" ["+origin.at("plugin").get<std::string>()+"]";
                item["detail"]=item.value("detail",std::string{})+" "+origin.at("title").get<std::string>()+
                    " "+origin.at("catalogue").get<std::string>();
            }
            if(origin.at("kind")=="user")item["label"]=item.at("label").get<std::string>()+" [user]";
            items.push_back(std::move(item));
        }
    };
    append(snippetCatalogueItems(workspace,language),{{"kind","workspace"},{"catalogue",snippetCataloguePath},{"plugin",""},{"title",""}});
    append(snippetCatalogueItems(user,language),{{"kind","user"},{"catalogue","user"},{"plugin",""},{"title",""}});
    for (const auto &catalogue : catalogues) {
        auto descriptor=catalogue;descriptor.erase("plugin");
        validateSnippetContribution(descriptor);
        if (!catalogue.contains("plugin") || !catalogue.at("plugin").is_string() ||
            !pluginIdentifier(catalogue.at("plugin").get_ref<const std::string&>()))
            throw std::runtime_error("Snippet snapshot has no plugin owner");
        if(!catalogue.at("id").get_ref<const std::string&>().starts_with(catalogue.at("plugin").get<std::string>()+"."))
            throw std::runtime_error("Snippet snapshot ID is outside its plugin namespace");
        const auto encoded=catalogue.dump();bytes+=encoded.size();
        if (!snippetRegistryFits(catalogues.size(),bytes)) throw std::runtime_error("Plugin snippet snapshot exceeds 1 MiB");
        append(snippetCatalogueItems(catalogue.at("definitions").dump(),language),
            {{"kind","plugin"},{"catalogue",catalogue.at("id")},{"plugin",catalogue.at("plugin")},
             {"title",catalogue.value("title",catalogue.at("id").get<std::string>())}});
    }
    return {{"items",std::move(items)},{"isIncomplete",incomplete}};
}
} // namespace cycode
