#include "cycode/Plugins.h"
#include "cycode/Process.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace cycode {
using Json = nlohmann::json;
void Plugins::addCommand(void *context, const char *id, const char *title, CyCodeCommand fn) {
    auto &self = *static_cast<Plugins *>(context);
    if (!id || !title || !fn)
        return;
    for (const auto &existing : self.commands_)
        if (existing.id == id)
            return;
    self.commands_.push_back({id, title, fn});
}
void Plugins::addVcs(void *context, const char *id, const char *title, const char *caps, CyCodeCommand fn) {
    auto &self = *static_cast<Plugins *>(context);
    if (!id || !title || !caps || !fn)
        return;
    try {
        auto parsed = Json::parse(caps);
        if (!parsed.is_array())
            return;
        for (const auto &p : self.providers_)
            if (p.id == id)
                return;
        self.providers_.push_back({id, title, parsed, fn});
    } catch (...) {
    }
}
Plugins::~Plugins() {
    for (auto it = libraries_.rbegin(); it != libraries_.rend(); ++it) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(*it));
#else
        dlclose(*it);
#endif
    }
}
void Plugins::load(const std::filesystem::path &directory) {
    std::lock_guard guard(mutex_);
    if (!std::filesystem::exists(directory))
        return;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() != ".json")
            continue;
        Json info;
        auto commandCount = commands_.size(), providerCount = providers_.size();
        void *library = nullptr;
        try {
            std::ifstream file(entry.path());
            info = Json::parse(file);
            if (info.at("abi") != CYCODE_PLUGIN_ABI)
                throw std::runtime_error("Unsupported plugin ABI");
            auto name = info.at("library").get<std::string>();
            if (utf8Path(name).filename() != utf8Path(name))
                throw std::runtime_error("Plugin library must be a local filename");
#ifdef _WIN32
            auto path = directory / utf8Path(name + ".dll");
            library = LoadLibraryExW(path.c_str(), nullptr,
                                     LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            auto init = library ? reinterpret_cast<CyCodePluginInit>(
                                      GetProcAddress(static_cast<HMODULE>(library), "cycode_plugin_init"))
                                : nullptr;
#elif defined(__APPLE__)
            library = dlopen((directory / (name + ".so")).c_str(), RTLD_NOW | RTLD_LOCAL);
            auto init =
                library ? reinterpret_cast<CyCodePluginInit>(dlsym(library, "cycode_plugin_init")) : nullptr;
#else
            library = dlopen((directory / (name + ".so")).c_str(), RTLD_NOW | RTLD_LOCAL);
            auto init =
                library ? reinterpret_cast<CyCodePluginInit>(dlsym(library, "cycode_plugin_init")) : nullptr;
#endif
            if (!init)
                throw std::runtime_error("Missing library or cycode_plugin_init export");
            CyCodeHostV1 host{CYCODE_PLUGIN_ABI, sizeof(CyCodeHostV1), this, addCommand, addVcs};
            if (init(&host) != 0)
                throw std::runtime_error("Plugin initialization failed");
            libraries_.push_back(library);
            info["state"] = "loaded";
        } catch (const std::exception &e) {
            commands_.resize(commandCount);
            providers_.resize(providerCount);
            info["state"] = "error";
            info["error"] = e.what();
            if (library) {
#ifdef _WIN32
                FreeLibrary(static_cast<HMODULE>(library));
#else
                dlclose(library);
#endif
            }
        }
        info["manifest"] = pathUtf8(entry.path());
        plugins_.push_back(info);
    }
}
Json Plugins::describe() const {
    std::lock_guard guard(mutex_);
    Json commands = Json::array(), providers = Json::array();
    for (const auto &c : commands_)
        commands.push_back({{"id", c.id}, {"title", c.title}});
    for (const auto &p : providers_)
        providers.push_back({{"id", p.id}, {"title", p.title}, {"capabilities", p.capabilities}});
    return {{"abi", CYCODE_PLUGIN_ABI},
            {"plugins", plugins_},
            {"commands", commands},
            {"vcsProviders", providers}};
}
Json Plugins::invoke(const std::string &id, const Json &input) {
    std::lock_guard guard(mutex_);
    for (const auto &c : commands_)
        if (c.id == id) {
            auto result = c.function(input.dump().c_str());
            if (!result)
                throw std::runtime_error("Plugin returned null");
            return Json::parse(result);
        }
    throw std::runtime_error("Unknown plugin command");
}
Json Plugins::vcs(const std::string &provider, const std::string &action, const Json &input) {
    std::lock_guard guard(mutex_);
    for (const auto &p : providers_)
        if (p.id == provider) {
            if (std::find(p.capabilities.begin(), p.capabilities.end(), action) == p.capabilities.end())
                throw std::runtime_error("Provider does not support " + action);
            return Json::parse(p.function(Json({{"action", action}, {"parameters", input}}).dump().c_str()));
        }
    throw std::runtime_error("Unknown version control provider");
}
} // namespace cycode
