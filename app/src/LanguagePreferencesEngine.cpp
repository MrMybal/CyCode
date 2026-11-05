#include "cycode/Engine.h"
#include "cycode/RustAnalysis.h"
#include "cycode/Tools.h"

namespace cycode {
void Engine::applyLanguageConfiguration(const std::string &backend, const Json &configuration, bool prewarm) {
    const bool enabled = configuration.at("enabled");
    if (configuration.contains("executable") && (backend == "cpp" || backend == "csharp")) {
        language_.configureBuiltin(backend, configuration.at("executable"), enabled, prewarm);
        return;
    }
    auto arguments = configuration.at("arguments").get<std::vector<std::string>>();
    if (configuration.contains("executable") && configuration.at("executable") != "")
        arguments.push_back(configuration.at("executable"));
    language_.configureBackend(backend, arguments, configuration.at("settings"), enabled, prewarm,
                               configuration.value("cargoProject",Json()));
}
void Engine::configureLanguage(const std::string &executable, bool enabled, bool prewarm) {
    refreshPluginServices();
    std::unique_lock lock(languageConfigurationMutex_);
    if (!enabled) {
        // Shutdown/initial Unreal preparation is a pause, never a saved user choice.
        language_.configure(executable, false, prewarm);
        return;
    }
    const auto preferences = languagePreferences_.snapshot(false);
    for (const auto *backend : {"cpp", "csharp"}) {
        if (languageChoices_.contains(backend))
            applyLanguageConfiguration(backend, languageChoices_.at(backend), prewarm);
        else if (preferences.at("profiles").contains(backend))
            applyLanguageConfiguration(backend, preferences.at("profiles").at(backend), prewarm);
        else
            language_.configureBuiltin(backend, backend == std::string("cpp") ? executable : "", true,
                                       prewarm);
    }
}
Json Engine::configureLanguageBackend(const Json &params) {
    std::unique_lock lock(languageConfigurationMutex_);
    const auto backend = params.value("language", "csharp");
    if (!language_.hasBackend(backend))
        throw std::runtime_error("Unknown language backend");
    const auto preferences = languagePreferences_.snapshot(false);
    auto configuration = languageChoices_.contains(backend) ? languageChoices_.at(backend)
                         : preferences.at("profiles").contains(backend)
                             ? preferences.at("profiles").at(backend)
                             : language_.configuration(backend);
    if (params.contains("arguments") || params.contains("settings") || params.contains("executable")) {
        const auto cargoProject=configuration.value("cargoProject",Json());
        configuration = {{"arguments", params.value("arguments", Json::array())},
                         {"settings", params.value("settings", Json::object())}};
        if(!cargoProject.is_null())configuration["cargoProject"]=cargoProject;
        if (params.contains("executable"))
            configuration["executable"] = params.at("executable");
    }
    configuration["enabled"] = params.value("enabled", true);
    if(params.contains("cargoProject")) {
        if(backend!="rust")throw std::runtime_error("Cargo analysis profiles require language=rust");
        if(params.at("cargoProject").is_null())configuration.erase("cargoProject");
        else configuration["cargoProject"]=normalizeRustAnalysisProfile(params.at("cargoProject"));
    }
    LanguagePreferences::validateConfiguration(configuration);
    if(configuration.contains("cargoProject")&&configuration.at("enabled").get<bool>()) {
        if(backend!="rust")throw std::runtime_error("Cargo analysis profiles require language=rust");
        // Validate derived settings and workspace paths before a preference write
        // or any reconfiguration of a running server.
        const auto defaults=defaultServerLaunch(root_,backend);
        const auto &settings=configuration.at("settings");
        const auto base=configuration.at("arguments").empty()&&!configuration.contains("executable")&&settings.empty()?defaults.settings:settings;
        (void)rustAnalysisSettings(base,configuration.at("cargoProject"),root_);
        auto requested=configuration.at("arguments").get<std::vector<std::string>>();
        if(configuration.contains("executable")&&configuration.at("executable")!="")requested.push_back(configuration.at("executable"));
        (void)rustAnalysisArguments(configuration.at("cargoProject"),requested,defaults.argv,toolPath("rustup"));
    }
    const bool save = params.value("save", false);
    if (save) {
        if (!params.contains("expectedPreferencesRevision"))
            throw std::runtime_error("Saving requires expectedPreferencesRevision from language.preferences");
        // Check the persisted revision before changing a running server. A stale writer
        // must neither overwrite preferences nor replace the other instance's launch.
        languagePreferences_.update(
            backend, configuration,
            LanguagePreferences::revisionValue(params.at("expectedPreferencesRevision")));
    } else if (params.contains("expectedPreferencesRevision"))
        throw std::runtime_error("expectedPreferencesRevision requires save=true");
    try {
        applyLanguageConfiguration(backend, configuration, true);
    } catch (const std::exception &e) {
        if (save)
            throw std::runtime_error(std::string("Language preferences saved; server preparation failed: ") +
                                     e.what());
        throw;
    }
    languageChoices_[backend] = std::move(configuration);
    lock.unlock(); // Status refresh may acquire the plugin activation lock.
    return languageStatus();
}
Json Engine::languageStatus() {
    refreshPluginServices();
    auto result = language_.status();
    result["preferences"] = languagePreferences_.status();
    return result;
}
} // namespace cycode
