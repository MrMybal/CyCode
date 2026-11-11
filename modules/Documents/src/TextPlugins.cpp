#include "cycode/text/TextPlugins.h"
#include "cycode/TextFile.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace cycode::text {
namespace {
void release(void *library) {
    if (!library) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}
bool identifier(const std::string &value) {
    return !value.empty() && value.size()<=128 && std::all_of(value.begin(),value.end(),[](unsigned char c) { return std::isalnum(c) || c=='.' || c=='-' || c=='_'; });
}
void validateOptions(const Json &definitions,const Json &values) {
    if (!values.is_object() || values.size()>64) throw std::runtime_error("Invalid plugin options");
    for (const auto &[key,value] : values.items()) {
        auto definition=std::find_if(definitions.begin(),definitions.end(),[&](const auto &d) { return d.at("id")==key; });
        if (definition==definitions.end()) throw std::runtime_error("Unknown plugin option: "+key);
        const auto type=definition->at("type").template get<std::string>();
        if ((type=="boolean" && !value.is_boolean()) || (type=="integer" && !value.is_number_integer()) || (type=="string" && (!value.is_string() || value.template get_ref<const std::string&>().size()>4096)) || (type!="boolean" && type!="integer" && type!="string")) throw std::runtime_error("Invalid plugin option type: "+key);
        if (type=="integer" && (value.template get<int64_t>()<-2147483648LL || value.template get<int64_t>()>2147483647LL)) throw std::runtime_error("Plugin integer option exceeds 32-bit range");
    }
}
}
std::string transformText(const std::string &text,const std::string &operation) {
    auto result=text;
    if (operation=="uppercase" || operation=="lowercase") {
        // Keep non-ASCII UTF-8 bytes intact; full Unicode case conversion is separate.
        for (auto &c : result) if (static_cast<unsigned char>(c)<128) c=char(operation=="uppercase" ? std::toupper(static_cast<unsigned char>(c)) : std::tolower(static_cast<unsigned char>(c)));
    } else if (operation=="trimTrailing") {
        result.clear();size_t start=0;
        while (start<text.size()) { const auto end=text.find('\n',start);auto line=text.substr(start,end==std::string::npos ? end : end-start);while (!line.empty() && (line.back()==' ' || line.back()=='\t')) line.pop_back();result+=line;if (end==std::string::npos) break;result+='\n';start=end+1; }
    } else if (operation=="sortLines" || operation=="sortLinesDescending" || operation=="uniqueLines") {
        std::vector<std::string> lines;std::istringstream stream(text);std::string line;while (std::getline(stream,line)) lines.push_back(line);
        if (operation=="uniqueLines") { std::set<std::string> seen;std::erase_if(lines,[&](const auto &s) { return !seen.insert(s).second; }); }
        else { std::stable_sort(lines.begin(),lines.end());if (operation=="sortLinesDescending") std::reverse(lines.begin(),lines.end()); }
        result.clear();for (size_t i=0;i<lines.size();++i) { result+=lines[i];if (i+1<lines.size() || text.ends_with('\n')) result+='\n'; }
    } else throw std::runtime_error("Unknown text transformation");
    validateEditorText(result);return result;
}
Plugins::~Plugins()=default;
void Plugins::load(const std::filesystem::path &directory) {
    if (!plugins_.empty() || !errors_.empty()) throw std::runtime_error("Plugin reload requires restarting Reader");
    if (!std::filesystem::exists(directory)) return;
    size_t count=0;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension()!=".json") continue;
        if (++count>64) { errors_.push_back({{"manifest",""},{"error","Plugin count exceeds 64"}});break; }
        Plugin p;
        try {
            if (std::filesystem::file_size(entry.path())>256*1024) throw std::runtime_error("Plugin manifest exceeds 256 KiB");
            p.manifest=Json::parse(readTextFileBytes(entry.path()));const auto id=p.manifest.at("id").get<std::string>();
            if (!identifier(id) || plugins_.contains(id) || p.manifest.at("abi")!=1) throw std::runtime_error("Invalid/duplicate plugin ID or ABI");
            for (const auto *key : {"name","version"}) if (p.manifest.contains(key) && (!p.manifest.at(key).is_string() || p.manifest.at(key).get_ref<const std::string&>().size()>256)) throw std::runtime_error("Invalid plugin metadata");
            const auto commands=p.manifest.value("commands",Json::array()),languages=p.manifest.value("languages",Json::array()),options=p.manifest.value("options",Json::array());
            if (!commands.is_array() || commands.size()>64 || !languages.is_array() || languages.size()>64 || !options.is_array() || options.size()>64) throw std::runtime_error("Invalid plugin contribution limits");
            std::set<std::string> names;
            for (const auto &command : commands) if (!identifier(command.at("id")) || !names.insert(command.at("id")).second || command.at("title").get<std::string>().size()>256) throw std::runtime_error("Invalid plugin command");
            const auto commandNames=names;
            for (const auto &command : commands) if (command.contains("kind") && command.at("kind")!="syntax" && command.at("kind")!="service") throw std::runtime_error("Unknown plugin command kind");
            if (p.manifest.contains("fileComparisons")) {
                const auto &provider=p.manifest.at("fileComparisons");
                const auto declaredService=[&](const std::string &name) { return std::any_of(commands.begin(),commands.end(),[&](const auto &c) { return c.at("id")==name && c.value("kind","")=="service"; }); };
                if (!p.manifest.contains("library") || !provider.is_object() || provider.at("menu").get<std::string>().empty() || provider.at("menu").get_ref<const std::string&>().size()>64 || !declaredService(provider.at("detect"))) throw std::runtime_error("Invalid file comparison provider");
                const auto &actions=provider.at("actions");if (!actions.is_array() || actions.empty() || actions.size()>16) throw std::runtime_error("Invalid file comparison actions");
                if (provider.contains("history")) {
                    const auto &history=provider.at("history");if (!history.is_object()) throw std::runtime_error("Invalid file history provider");
                    for (const auto *key : {"file","selection","compare"}) if (!history.contains(key) || !history.at(key).is_string() || !declaredService(history.at(key))) throw std::runtime_error("Invalid file history command");
                }
                std::set<std::string> actionIds;
                for (const auto &action : actions) if (!declaredService(action.at("command")) || !actionIds.insert(action.at("command")).second || action.at("title").get<std::string>().empty() || action.at("title").get_ref<const std::string&>().size()>128) throw std::runtime_error("Invalid file comparison action");
            }
            names.clear();for (const auto &language : languages) {
                if (!identifier(language.at("id")) || !names.insert(language.at("id")).second || language.at("name").get<std::string>().empty() || language.at("name").get<std::string>().size()>128) throw std::runtime_error("Invalid plugin language");
                const auto extensions=language.at("extensions");if (!extensions.is_array() || extensions.empty() || extensions.size()>32) throw std::runtime_error("Invalid language extensions");
                for (const auto &ext : extensions) { auto s=ext.get<std::string>();if (s.size()<2 || s.size()>32 || s[0]!='.' || s.find_first_of("/\\")!=std::string::npos) throw std::runtime_error("Invalid language extension"); }
                for (const auto *key : {"base","lineComment","commentStart","commentEnd"}) if (language.contains(key) && (!language.at(key).is_string() || language.at(key).get_ref<const std::string&>().size()>64)) throw std::runtime_error("Invalid language delimiter");
                if (language.contains("providerCommand") && (!language.at("providerCommand").is_string() || !commandNames.contains(language.at("providerCommand").get<std::string>()) || !p.manifest.contains("library"))) throw std::runtime_error("Syntax provider must name a declared native command");
                if (language.contains("lexer") && language.at("lexer")!="rules" && language.at("lexer")!="inherit") throw std::runtime_error("Unknown syntax lexer mode");
                if (language.contains("wordLists")) { if (!language.at("wordLists").is_array() || language.at("wordLists").size()>9) throw std::runtime_error("Invalid lexer word lists");for (const auto &words : language.at("wordLists")) if (!words.is_string() || words.get_ref<const std::string&>().size()>32768) throw std::runtime_error("Lexer word list exceeds limit"); }
                if (language.contains("caseSensitive") && !language.at("caseSensitive").is_boolean()) throw std::runtime_error("Invalid language case-sensitivity option");
                for (const auto &field : {"keywords","types","identifiers"}) if (language.contains(field)) { if (!language.at(field).is_array() || language.at(field).size()>2048) throw std::runtime_error("Invalid language word limit");for (const auto &word : language.at(field)) if (!word.is_string() || word.get_ref<const std::string&>().size()>128) throw std::runtime_error("Invalid language word"); }
            }
            p.options=Json::object();for (const auto &option : options) { const auto key=option.at("id").get<std::string>();if (!identifier(key) || p.options.contains(key)) throw std::runtime_error("Invalid plugin option ID");p.options[key]=option.at("default"); }validateOptions(options,p.options);
            if (p.manifest.contains("library")) {
                const auto name=p.manifest.at("library").get<std::string>();
                if (!identifier(name) || utf8Path(name).filename()!=utf8Path(name)) throw std::runtime_error("Plugin library must be a local stem");
#ifdef _WIN32
                const auto file=std::filesystem::weakly_canonical(directory/utf8Path(name+".dll"));
#else
                const auto file=std::filesystem::weakly_canonical(directory/utf8Path(name+".so"));
#endif
                if (file.parent_path()!=std::filesystem::weakly_canonical(directory)) throw std::runtime_error("Plugin library escapes plugin directory");
#ifdef _WIN32
                p.library=std::shared_ptr<void>(LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS),release);
                auto init=p.library ? reinterpret_cast<CyCodeReaderPluginEntry>(GetProcAddress(static_cast<HMODULE>(p.library.get()),"cycodereader_plugin_v1")) : nullptr;
#else
                p.library=std::shared_ptr<void>(dlopen(file.c_str(),RTLD_NOW|RTLD_LOCAL),release);auto init=p.library ? reinterpret_cast<CyCodeReaderPluginEntry>(dlsym(p.library.get(),"cycodereader_plugin_v1")) : nullptr;
#endif
                p.api=init ? init() : nullptr;
                if (!p.api || p.api->abi!=CYCODE_READER_PLUGIN_ABI || p.api->size<sizeof(CyCodeReaderPluginV1) || !p.api->execute) throw std::runtime_error("Invalid plugin library/API");
            }
            plugins_.emplace(id,std::move(p));
        } catch (const std::exception &e) { errors_.push_back({{"manifest",pathUtf8(entry.path().filename())},{"error",e.what()}}); }
    }
}
Json Plugins::describe() const {
    Json items=Json::array();for (const auto &[id,p] : plugins_) items.push_back({{"id",id},{"name",p.manifest.value("name",id)},{"version",p.manifest.value("version","")},{"commands",p.manifest.value("commands",Json::array())},{"languages",p.manifest.value("languages",Json::array())},{"optionDefinitions",p.manifest.value("options",Json::array())},{"options",p.options},{"native",p.api!=nullptr},{"fileComparisons",p.manifest.value("fileComparisons",Json::object())}});
    return {{"plugins",items},{"errors",errors_}};
}
Json Plugins::configure(const std::string &id,const Json &options) { auto &p=plugins_.at(id);validateOptions(p.manifest.value("options",Json::array()),options);p.options.update(options);return describe(); }
Json Plugins::invoke(const std::string &id,const std::string &command,const Json &input) {
    return prepare(id,command,input)();
}
std::function<Json()> Plugins::prepare(const std::string &id,const std::string &command,const Json &input) const {
    const auto &p=plugins_.at(id);const auto commands=p.manifest.value("commands",Json::array());auto found=std::find_if(commands.begin(),commands.end(),[&](const auto &c) { return c.at("id")==command; });if (found==commands.end()) throw std::runtime_error("Unknown plugin command");
    if (found->contains("operation")) return [input,operation=found->at("operation").get<std::string>()] { return Json{{"text",transformText(input.at("text"),operation)}}; };
    if (!p.api) throw std::runtime_error("This command requires a native plugin library");
    auto payload=input;payload["options"]=p.options;const auto serialized=payload.dump();
    return [library=p.library,api=p.api,execution=p.execution,serialized,command] {
    std::lock_guard lock(*execution);
    const char *reply=api->execute(command.c_str(),serialized.c_str());if (!reply) throw std::runtime_error("Plugin returned no result");
    size_t length=0;while (length<=8*1024*1024 && reply[length]) ++length;if (length>8*1024*1024) throw std::runtime_error("Plugin response exceeds 8 MiB");
    auto result=Json::parse(reply,reply+length);if (!result.is_object()) throw std::runtime_error("Plugin response must be an object");if (result.contains("text")) validateEditorText(result.at("text"));return result;
    };
}
}
