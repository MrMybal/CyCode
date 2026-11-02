#include "cycode/Application.h"
#include "cycode/Completion.h"
#include "cycode/EditorCompletion.h"
#include "cycode/EditorTheme.h"
#include "cycode/EditorLanguage.h"
#include "cycode/FileIcons.h"
#include "cycode/ImageView.h"
#include <algorithm>
#include <cyui/Core.h>
#include <fstream>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <random>
#include <sstream>
namespace cycode {
template <class T> static bool ready(std::future<T> &f) {
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
static std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
static const TextEditor::Language *languageFor(const std::string &path) {
    return editorLanguage(path);
}
static bool cppFile(const std::string &path) {
    return LanguageService::supportsCpp(path);
}
Application::Application(LaunchOptions options, SDL_Window *window)
    : root_(options.installation), window_(window), smoke_(options.smoke) {
    testHighlights_=options.testHighlights;
    try {
        std::ifstream in(root_ / "app/locales/ui.json");
        translations_ = Json::parse(in);
    } catch (...) {
        translations_ = Json::object();
    }
    for (const auto *locale : {"en", "fr"}) {
        cyui::TranslationCatalog catalog;
        for (const auto &[key, value] : translations_.items())
            if (value.contains(locale)) catalog.emplace(key, value.at(locale).get<std::string>());
        cyui::registerTranslations(locale, catalog);
    }
    loadSettings();
    if (!options.keymap.empty()) { keymap_.load(options.keymap, keymapOverrides_.value(options.keymap, Json::object())); keymapProfile_ = options.keymap; }
    options_ = options.showOptions;
    focusCmake_ = options.showCmake;
    focusProjectTasks_ = options.showProjectTasks;
    focusUnreal_ = options.showUnreal;
    testSession_ = options.testSession;
    if (!options.terminalProfile.empty()) terminalProfile_ = options.terminalProfile;
    testWindows_ = options.testWindows;
    std::ostringstream identity;
    std::random_device random;
    identity << std::hex << std::chrono::steady_clock::now().time_since_epoch().count() << '-' << random()
             << random();
    instance_ = identity.str();
    if (testSession_)
        testNavigation_ = std::move(options.testNavigation);
    if (testSession_) testCompletion_ = std::move(options.testCompletion);
    if (!options.theme.empty())
        theme_ = options.theme;
    if (!options.language.empty())
        locale_ = options.language;
    cyui::Config config;
    config.themeKey = theme_.c_str();
    config.dpiScale = window_ ? SDL_GetWindowDisplayScale(window_) : 1.f;
    config.squareWindows = true;
    cyui::apply(config);
    cyui::setLocale(locale_.c_str());
    applyEditorTheme(theme_);
    cyui::loadFonts();
    ImGui::GetStyle().FontScaleMain = textScale_;
    auto workspace = options.workspace;
    if (workspace.empty())
        workspace = utf8Path(settings_.value("workspace", pathUtf8(root_)));
    if (options.workspace.empty() && !std::filesystem::exists(workspace))
        workspace = root_;
    setWorkspace(workspace);
    if (!options.openFile.empty())
        open(options.openFile, options.openLine, options.openColumn, true);
    else if (std::filesystem::exists(workspace / "app/examples/hello/main.cpp"))
        open("app/examples/hello/main.cpp");
}
Application::~Application() {
    if (unrealAnalysisCancellation_) *unrealAnalysisCancellation_ = true;
    if (unrealAnalysisFuture_.valid()) unrealAnalysisFuture_.wait();
    if (searchCancellation_)
        *searchCancellation_ = true;
    engine_->configureLanguage(clangd_, false, false);
    for (const auto &tab : tabs_) for (auto *future : {&tab->completion,&tab->completionAccept,&tab->highlighting}) if (future->valid()) future->wait();
    for (auto &future : retiredCompletions_) if (future.valid()) future.wait();
    for (auto *future : {&filesFuture_, &searchFuture_, &gitFuture_, &taskFuture_, &navigationFuture_,
                         &buildFuture_, &detectionFuture_, &symbolFuture_, &informationFuture_, &codeActionsFuture_, &refactorFuture_, &terminalFuture_, &debugFuture_, &vcsFuture_})
        if (future->valid())
            future->wait();
    bridge_.reset();
    if (coreService_.valid())
        coreService_.wait();
    if (!smoke_ && !testSession_) {
        ImGui::SaveIniSettingsToDisk(layoutIni_.c_str());
        try {
            saveSettings();
        } catch (const std::exception &) { /* A read-only settings directory must not prevent exit. */
        }
    }
    tabs_.clear();
    if (imageTextures_) imageTextures_->collect();
    imageLoader_.reset();
    ImGui::GetIO().IniFilename = nullptr;
}
const char *Application::tr(const char *key) const {
    auto found = translations_.find(key);
    if (found != translations_.end() && found->contains(locale_))
        return (*found)[locale_].get_ref<const std::string &>().c_str();
    return key;
}
std::string Application::label(const char *text, const char *id) const {
    return std::string(tr(text)) + "###" + id;
}
void Application::loadSettings() {
    try {
        std::ifstream in(root_ / "data/settings/ui-preferences.json");
        settings_ = Json::parse(in);
    } catch (...) {
        settings_ = Json::object();
    }
    theme_ = settings_.value("theme", theme_);
    if (settings_.value("schemaVersion", 1) < 2 && theme_ == "navy")
        theme_ = "graphite";
    locale_ = settings_.value("language", locale_);
    keymapProfile_ = settings_.value("keymap", keymapProfile_);
    keymapOverrides_ = settings_.value("keymapOverrides", Json::object());
    try { keymap_.load(keymapProfile_, keymapOverrides_.value(keymapProfile_, Json::object())); }
    catch (const std::exception &e) { lastError_ = e.what(); keymapProfile_ = "rider"; keymapOverrides_ = Json::object(); keymap_.load(keymapProfile_); }
    textScale_ = settings_.value("textScale", 1.15f);
    if (settings_.value("schemaVersion", 1) < 3 && settings_.contains("textScale")) textScale_ *= 1.15f;
    textScale_ = std::clamp(textScale_, 0.8f, 2.0f);
    clangd_ = settings_.value("clangd", clangd_);
#ifdef _WIN32
    if (clangd_ == "clangd" && std::filesystem::exists("C:/Program Files/LLVM/bin/clangd.exe"))
        clangd_ = "C:/Program Files/LLVM/bin/clangd.exe";
#endif
}
void Application::saveSettings() {
    std::filesystem::create_directories(root_ / "data/settings");
    settings_ = {{"schemaVersion", 3},      {"theme", theme_},   {"language", locale_},
                 {"textScale", textScale_}, {"clangd", clangd_}, {"workspace", pathUtf8(entry_)},
                 {"keymap", keymapProfile_}, {"keymapOverrides", keymapOverrides_}};
    auto destination = root_ / "data/settings/ui-preferences.json";
    auto temporary = destination;
    temporary += "." + instance_ + ".tmp";
    {
        std::ofstream out(temporary);
        out << settings_.dump(2);
        if (!out)
            throw std::runtime_error("Cannot save preferences");
    }
    if (!SDL_RenamePath(pathUtf8(temporary).c_str(), pathUtf8(destination).c_str())) {
        std::filesystem::remove(temporary);
        throw std::runtime_error(SDL_GetError());
    }
}
bool Application::busy() const {
    for (const auto &tab : tabs_) if ((tab->image && tab->image->loading()) || tab->highlighting.valid() || tab->completionAccept.valid()) return true;
    return filesFuture_.valid() || searchFuture_.valid() || gitFuture_.valid() || taskFuture_.valid() ||
           navigationFuture_.valid() || buildFuture_.valid() || detectionFuture_.valid() || unrealAnalysisFuture_.valid() || symbolFuture_.valid() || informationFuture_.valid() || codeActionsFuture_.valid() || refactorFuture_.valid() || terminalFuture_.valid() || debugFuture_.valid() || vcsFuture_.valid();
}
Json Application::testState() const {
    Json result = {{"active", active_},
                   {"navigation", navigation_},
                   {"error", lastError_},
                   {"languageReady", languageState_.value("ready", false)}};
    result["build"] = buildState_;
    result["msbuild"] = msbuildState_; result["msbuildActive"] = msbuildActive_;
    result["projectTasks"] = projectState_; result["projectActive"] = projectActive_;
    result["buildDiagnostics"] = buildDiagnostics_;
    result["toolchains"] = toolchains_;
    result["solution"] = solution_;
    result["unreal"] = unreal_;
    result["unrealAnalysis"] = unrealAnalysisState_;
    result["workspace"] = pathUtf8(engine_->workspace());
    result["entry"] = pathUtf8(entry_);
    result["theme"] = theme_;
    result["textScale"] = textScale_;
    result["fileCount"] = files_.size();
    result["ui"] = {{"quickOpen", quickOpenVisible_}, {"lastShortcut", lastShortcut_}};
    result["ui"]["paletteMode"] = paletteMode_;
    result["ui"]["keymap"] = keymapProfile_;
    result["ui"]["options"] = options_;
    result["ui"]["editorFocused"] = editorFocused_;
    if (auto focused = ImGui::GetCurrentContext()->NavWindow) result["ui"]["focusedWindow"] = focused->Name;
    result["ui"]["commands"] = executedCommands_;
    result["ui"]["searchQuery"] = searchQuery_;
    result["ui"]["paletteQuery"] = paletteFilter_;
    result["ui"]["search"] = search_;
    result["symbols"] = symbolResults_;
    result["information"] = {{"result",informationState_},{"error",informationError_},{"method",informationMethod_},{"query",informationQuery_}};
    result["refactoring"] = refactorPlan_;
    result["codeActions"]=codeActionsState_;
    result["terminals"] = engine_->terminals().list();
    result["debug"] = engine_->debugger().status();
    result["vcs"] = {{"status",git_},{"details",vcsInfo_},{"history",vcsHistory_},{"conflict",vcsConflict_},{"section",vcsSection_},{"preview",vcsPreviewTitle_}};
    if (!terminalId_.empty()) {
        try { result["terminal"] = engine_->dispatch("terminal.read", {{"id", terminalId_}, {"limit", 2000}}); } catch (const std::exception &) {}
    }
    result["instance"] = instance_;
    result["windowTest"] = windowTest_;
    result["nativeWindows"] = ImGui::GetPlatformIO().Viewports.Size;
    result["hiddenWindow"] = bool(SDL_GetWindowFlags(window_) & SDL_WINDOW_HIDDEN);
    for (const auto &tab : tabs_)
        if (tab->path == active_) {
            if (tab->image) {
                result["image"]={{"path",tab->path},{"loading",tab->image->loading()},{"error",tab->image->error()}};
                if (auto texture=tab->image->texture()) result["image"]["size"]={texture->data.width,texture->data.height};
            }
            result["text"] = tab->editor.GetText();
            result["dirty"] = tab->dirty;
            result["readOnly"] = tab->readOnly;
            result["displayPath"] = tab->displayPath;
            result["syntax"]={{"language",Syntax::language(tab->path)},{"spans",tab->syntax.spans()},{"truncated",tab->syntax.truncated()}};
            result["highlight"]={{"revision",tab->highlightRevision},{"generation",tab->highlightGeneration},{"count",tab->highlightCount},{"truncated",tab->highlightTruncated},{"error",tab->highlightError}};
            result["completionTest"] = {{"phase", testCompletionPhase_}, {"generation", tab->completionGeneration}, {"runningGeneration", tab->runningCompletionGeneration}, {"wordStart", {tab->runningCompletionStart.line, tab->runningCompletionStart.index}}, {"end", {tab->completionPosition.line, tab->completionPosition.index}}};
            result["completionTest"]["reply"] = tab->completionTrace;
            result["completionSuggestions"] = tab->lastSuggestions;
            const auto position = tab->editor.GetMainCursorPosition();
            result["cursor"] = {{"line", position.line},
                                {"codepoint", position.index},
                                {"character", LanguageClient::utf16Length(
                                                  tab->editor.GetSectionText({position.line, 0}, position))}};
        }
    return result;
}
void Application::setWorkspace(const std::filesystem::path &path) {
    // Validate the requested solution before tearing down the current session.
    auto nextEngine = std::make_shared<Engine>(path);
    auto nextSolution = nextEngine->dispatch("workspace.describe").at("solution");
    auto nextUnreal = nextEngine->dispatch("unreal.inspect");
    const bool inspectMsbuild = !nextUnreal.value("available",false) && (!std::filesystem::is_directory(path) || (!std::filesystem::exists(nextEngine->workspace()/"CMakeLists.txt") && !std::filesystem::exists(nextEngine->workspace()/"app/CMakeLists.txt")));
    const auto nextMsbuild = inspectMsbuild ? nextEngine->dispatch("msbuild.inspect") : Json::object();
    if (engine_) {
        for (const auto &tab : tabs_)
            if (tab->dirty)
                throw std::runtime_error(tr("Save or close modified documents before switching workspace."));
        for (const auto &tab : tabs_) if (tab->completionAccept.valid()) throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        for (auto *future : {&filesFuture_, &searchFuture_, &gitFuture_, &taskFuture_, &navigationFuture_,
                             &buildFuture_, &detectionFuture_, &symbolFuture_, &informationFuture_, &codeActionsFuture_, &refactorFuture_, &terminalFuture_, &debugFuture_, &vcsFuture_})
            if (future->valid())
                throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        engine_->configureLanguage(clangd_, false, false);
    }
    bridge_.reset();
    if (coreService_.valid())
        coreService_.get();
    for (const auto &tab : tabs_) {
        if (tab->highlighting.valid()) retiredCompletions_.push_back(std::move(tab->highlighting));
        if (tab->completion.valid()) retiredCompletions_.push_back(std::move(tab->completion));
    }
    tabs_.clear();
    languageDecisions_.clear();
    historyBack_.clear(); historyForward_.clear(); recentFiles_.clear(); closedFiles_.clear(); keymap_.cancel(); closeAll_ = false;
    symbolResults_ = Json::object(); symbolWantedKey_.clear(); symbolSentKey_.clear(); refactorPlan_ = nullptr; refactorPreview_ = nullptr;
    panels_.try_emplace("refactoring", false);
    panels_.try_emplace("information", false);
    panels_.try_emplace("codeActions",false);codeActionsState_=Json::object();codeActionsKey_.clear();codeActionsError_.clear();codeActionSelected_=-1;
    informationMethod_.clear(); informationWanted_.clear(); informationSent_.clear(); informationError_.clear();
    informationState_ = Json::object(); informationQuery_ = nullptr;
    panels_.try_emplace("terminal", false);
    terminalId_.clear(); terminalFocused_ = false; focusTerminal_ = false;
    debugProfileLoaded_ = false; debugState_ = Json::object(); debugChildren_ = Json::object(); debugJumpKey_.clear(); debugCurrentPath_.clear(); pendingDebugBuild_ = false;
    debugBreakpointDrafts_ = Json::object(); debugCurrentLine_ = 0;
    vcsInfo_ = vcsHistory_ = vcsConflict_ = Json::object(); vcsSection_ = "Changes"; vcsPreviewTitle_.clear();
    terminalSelectionStart_ = terminalSelectionEnd_ = {-1, 0};
    active_.clear();
    files_.clear();
    tree_ = {};
    lastError_.clear();
    search_ = Json::object();
    navigation_ = Json::object();
    buildState_ = Json::object();
    buildDiagnostics_ = Json::array();
    problemBuild_ = problemLanguage_ = nullptr; problems_ = {};
    toolchains_ = Json::object();
    buildEdited_ = false;
    buildToolchain_ = "automatic";
    buildGenerator_ = "automatic";
    buildTarget_.clear();
    engine_ = std::move(nextEngine);
    solution_ = std::move(nextSolution);
    solutionConfiguration_ = solution_.is_object() && !solution_.at("configurations").empty()
                                 ? solution_.at("configurations")[0].get<std::string>()
                                 : "";
    applyUnreal(nextUnreal);
    msbuildState_ = Json::object(); msbuildActive_ = false;
    msbuildActive_ = nextMsbuild.value("available",false); if (msbuildActive_) applyBuild(nextMsbuild);
    projectActive_ = !unreal_.value("available",false) && !msbuildActive_ && !ProjectTasks::detect(engine_->workspace()).empty();
    projectState_ = Json::object(); projectProfile_ = Json::object();
    unrealLanguage_ = *editorLanguage("source.cpp",true);
    entry_ = !std::filesystem::is_directory(path)
                 ? std::filesystem::canonical(path)
                 : (solution_.is_object() ? utf8Path(solution_.at("path")) : engine_->workspace());
    if (!smoke_ && !testSession_) {
        if (!layoutIni_.empty())
            ImGui::SaveIniSettingsToDisk(layoutIni_.c_str());
        // A stable workspace key keeps layouts for different projects independent.
        auto key = pathUtf8(entry_);
#ifdef _WIN32
        key = lower(key);
#endif
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : key) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        auto directory = root_ / "data/settings/workspaces";
        std::filesystem::create_directories(directory);
        layoutIni_ = pathUtf8(directory / (std::to_string(hash) + ".ini"));
        ImGui::GetIO().IniFilename = layoutIni_.c_str();
        ImGui::ClearIniSettings();
        if (std::filesystem::exists(utf8Path(layoutIni_)))
            ImGui::LoadIniSettingsFromDisk(layoutIni_.c_str());
        layoutReady_ = false;
    }
    unrealAnalysisState_.clear();
    engine_->configureLanguage(clangd_, !smoke_ && !unreal_.value("available", false), true);
    engine_->plugins().load(root_ / "runtime/plugins");
    pluginInfo_ = engine_->dispatch("plugins.list");
    auto instance =
        instance_ + '-' + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    bridge_ = std::make_unique<Bridge>(*engine_, root_ / ("data/bridge/native-" + instance + ".json"));
    coreDescriptor_ = root_ / ("data/bridge/core-" + instance + ".json");
#ifdef _WIN32
    auto python = root_ / "runtime/python/Scripts/python.exe";
#else
    auto python = root_ / "runtime/python/bin/python";
#endif
    if (!smoke_ && std::filesystem::exists(python)) {
        std::vector<std::string> command{pathUtf8(python),
                                         pathUtf8(root_ / "app/tool.py"),
                                         "--desktop-service",
                                         "--native-descriptor",
                                         pathUtf8(bridge_->descriptor()),
                                         "--service-descriptor",
                                         pathUtf8(coreDescriptor_),
                                         "--data-dir",
                                         pathUtf8(root_ / ("data/jobs/instance-" + instance))};
        auto directory = root_;
        coreService_ = std::async(std::launch::async, [command, directory] {
            return runProcess(command, directory, std::chrono::hours(24), 1024 * 1024);
        });
    }
    buildSource_ = std::filesystem::exists(engine_->workspace() / "CMakeLists.txt")       ? "."
                   : std::filesystem::exists(engine_->workspace() / "app/CMakeLists.txt") ? "app"
                                                                                          : ".";
    output_ = std::string(tr("Workspace opened: ")) + pathUtf8(path) + "\n";
    refreshFiles();
    refreshGit();
    refreshBuild(!smoke_, true);
    if (!smoke_ && unreal_.value("available", false)) prepareUnreal();
    if (window_) SDL_SetWindowTitle(window_,
                       ("CyCode  |  " + (solution_.is_object() ? solution_.at("name").get<std::string>()
                                                               : pathUtf8(engine_->workspace().filename())))
                           .c_str());
}
void Application::openProject(const std::filesystem::path &path) {
    pendingWorkspace_ = pathUtf8(path);
    if (unrealAnalysisCancellation_) *unrealAnalysisCancellation_ = true;
}
void Application::prepareUnreal() {
    if (unrealAnalysisFuture_.valid() || !unreal_.value("available", false)) return;
    unrealAnalysisState_ = "preparing";
    auto engine = engine_; auto params = buildParameters();
    unrealAnalysisCancellation_ = std::make_shared<std::atomic<bool>>(false);
    auto cancellation = unrealAnalysisCancellation_;
    unrealAnalysisFuture_ = std::async(std::launch::async, [engine, params, cancellation] { return engine->prepareUnrealAnalysis(params, cancellation.get()); });
}
void Application::refreshFiles() {
    if (filesFuture_.valid())
        return;
    auto engine = engine_;
    filesFuture_ = std::async(std::launch::async, [engine] { return engine->dispatch("workspace.files"); });
}
Json Application::buildParameters() const {
    if (projectActive_) return projectProfile_;
    if (unreal_.value("available", false))
        return {{"project", unrealProject_},
                {"engineRoot", unrealEngine_},
                {"target", unrealTarget_},
                {"platform", unrealPlatform_},
                {"configuration", unrealConfiguration_}};
    if (msbuildActive_) return {{"entry",msbuildState_.at("profile").at("entry")},{"configuration",msbuildConfiguration_},{"platform",msbuildPlatform_},{"project",msbuildProject_},{"tool",msbuildTool_}};
    return {{"source", buildSource_},          {"buildDirectory", buildDirectory_},
            {"configuration", configuration_}, {"toolchain", buildToolchain_},
            {"generator", buildGenerator_},    {"target", buildTarget_}};
}
void Application::applyBuild(const Json &project) {
    if (project.contains("kind")) {
        projectState_ = project;
        if (!buildEdited_ || projectProfile_.empty()) projectProfile_ = project.at("profile");
        return;
    }
    if (project.contains("profile") && project.at("profile").contains("entry")) {
        msbuildState_ = project;
        if (msbuildState_.at("tools").empty() && toolchains_.contains("tools")) msbuildState_["tools"] = toolchains_.at("tools");
        if (!buildEdited_) { const auto &profile = project.at("profile"); msbuildConfiguration_ = profile.at("configuration"); msbuildPlatform_ = profile.at("platform"); msbuildProject_ = profile.at("project"); msbuildTool_ = profile.at("tool"); solutionConfiguration_ = msbuildConfiguration_+"|"+msbuildPlatform_; }
        return;
    }
    buildState_ = project;
    if (buildEdited_ || !project.contains("profile"))
        return;
    const auto &profile = project.at("profile");
    buildSource_ = profile.at("source");
    buildDirectory_ = profile.at("buildDirectory");
    configuration_ = profile.at("configuration");
    buildToolchain_ = profile.at("toolchain");
    buildGenerator_ = profile.at("generator");
    buildTarget_ = profile.at("target");
}
void Application::refreshBuild(bool detectTools, bool initial) {
    if (unreal_.value("available", false))
        return;
    auto engine = engine_;
    if (!buildFuture_.valid()) {
        auto parameters = !initial && buildEdited_ ? buildParameters() : Json::object();
        const auto method = projectActive_ ? "project.inspect" : msbuildActive_ ? "msbuild.inspect" : "build.inspect";
        buildFuture_ = std::async(std::launch::async, [engine, parameters, method] {
            return engine->dispatch(method, parameters);
        });
        lastBuildRefresh_ = ImGui::GetTime();
    }
    if (detectTools && !projectActive_ && !detectionFuture_.valid()) {
        const auto method = msbuildActive_ ? "msbuild.tools" : "toolchains.detect";
        detectionFuture_ = std::async(std::launch::async, [engine,method] {
            return engine->dispatch(method, {{"refresh", true}});
        });
    }
}
void Application::refreshGit() {
    if (gitFuture_.valid())
        return;
    auto engine = engine_;
    auto provider = provider_;
    gitFuture_ = std::async(std::launch::async, [engine, provider] {
        Json result = {{"status",engine->dispatch("vcs.status", {{"provider", provider}})}};
        if (provider == "git") { result["details"] = engine->dispatch("vcs.inspect"); if (result.at("details").value("repository",false)) result["history"] = engine->dispatch("vcs.history"); }
        return result;
    });
    lastGitRefresh_ = ImGui::GetTime();
}
bool Application::vcsSupports(const std::string &action) const {
    if (provider_ == "git")
        return true;
    for (const auto &provider : pluginInfo_.value("vcsProviders", Json::array())) {
        if (provider.value("id", std::string{}) != provider_)
            continue;
        const auto &capabilities = provider["capabilities"];
        return std::find(capabilities.begin(), capabilities.end(), action) != capabilities.end();
    }
    return false;
}
void Application::error(const std::string &message) {
    lastError_ = message;
    output_ += "\n" + message + "\n";
    cyui::notify(message.c_str(), cyui::Tone::error, 6);
}
void Application::task(const std::string &method, Json params) {
    auto actualMethod = method;
    if (unreal_.value("available", false)) {
        if (method == "build.configure")
            actualMethod = "unreal.generate";
        if (method == "build.run")
            actualMethod = "unreal.build";
    }
    if (msbuildActive_) { if (method == "build.configure") actualMethod = "msbuild.restore"; if (method == "build.run") actualMethod = "msbuild.build"; }
    if (projectActive_) { if (method == "build.configure") actualMethod = "project.configure"; if (method == "build.run") actualMethod = "project.build"; }
    if (actualMethod.starts_with("unreal."))
        params = buildParameters();
    if (method == "build.configure" || method == "build.run")
        params = buildParameters();
    if (actualMethod.starts_with("msbuild.")) { params = buildParameters(); if (msbuildRestore_ && (actualMethod == "msbuild.build" || actualMethod == "msbuild.rebuild")) params["restore"] = true; }
    if (actualMethod.starts_with("project.")) params = projectProfile_;
    if (actualMethod == "msbuild.build" || actualMethod == "msbuild.rebuild" || actualMethod.starts_with("project.")) {
        saveAll();
        if (std::any_of(tabs_.begin(),tabs_.end(),[](const auto &tab) { return tab->dirty; })) return;
    }
    if (method.starts_with("vcs."))
        params["provider"] = provider_;
    if (taskFuture_.valid()) {
        error(tr("A task is already running."));
        return;
    }
    auto engine = engine_;
    taskMethod_ = actualMethod;
    showPanel("Build","build");
    output_ += "\n> " + actualMethod + "\n";
    taskFuture_ = std::async(std::launch::async, [engine, actualMethod, params] {
        return engine->dispatch(actualMethod, params);
    });
}
void Application::configureEditor(const std::shared_ptr<Tab> &tab) {
    configureCodeEditor(tab->codeEditor);
    tab->codeEditor.setLanguageForPath(tab->displayPath);
    // Rich project-aware lexical/semantic overlays keep their existing native hook.
    tab->codeEditor.setSyntaxEnabled(!LanguageService::supports(tab->displayPath));
    tab->snippet.attach(tab->editor);
    if (LanguageService::supports(tab->displayPath)) tab->editor.SetLanguage(unreal_.value("available", false) && cppFile(tab->path) ? &unrealLanguage_ : languageFor(tab->displayPath));
    tab->syntax.update(tab->editor,tab->path,tab->editor.GetText());
    tab->editor.SetTabSize(4);
    tab->editor.SetInsertSpacesOnTabs(true);
    tab->editor.SetShowMiniMapEnabled(false);
    tab->editor.SetLineFoldingEnabled(true);
    tab->editor.SetReadOnlyEnabled(tab->readOnly);
    tab->editor.SetLineDecorator(2, [this, weak = std::weak_ptr<Tab>(tab)](TextEditor::Decorator &line) {
        auto tab = weak.lock(); if (!tab) return;
        const auto at = ImGui::GetCursorScreenPos(); auto draw = ImGui::GetWindowDrawList();
        bool breakpoint = false, enabled = true, verified = false;
        if (debugState_.contains("breakpoints")) {
            const auto &points = debugState_.at("breakpoints");
            if (points.contains(tab->path)) for (const auto &point : points.at(tab->path)) if (point.value("line",0) == int(line.line+1)) { breakpoint = true; enabled = point.value("enabled",true); }
            if (debugState_.contains("verifiedBreakpoints") && debugState_.at("verifiedBreakpoints").contains(tab->path))
                for (const auto &point : debugState_.at("verifiedBreakpoints").at(tab->path)) if (point.value("line",0) == int(line.line+1)) verified = point.value("verified",false);
        }
        if (breakpoint) {
            const ImVec2 center{at.x+line.width*.36f,at.y+line.height*.5f}; const auto color = enabled ? cyui::palette().error : cyui::palette().textFaint;
            if (verified || debugState_.value("state","idle") == "idle") draw->AddCircleFilled(center,cyui::dp(4.3f),color);
            else draw->AddCircle(center,cyui::dp(4.3f),color,12,cyui::dp(1.4f));
        }
        if (debugState_.value("state","") == "stopped" && debugCurrentPath_ == tab->path && debugCurrentLine_ == line.line+1)
            draw->AddTriangleFilled({at.x+line.width*.6f,at.y+cyui::dp(2)},{at.x+line.width,at.y+line.height*.5f},{at.x+line.width*.6f,at.y+line.height-cyui::dp(2)},cyui::palette().warning);
        ImGui::BeginDisabled(tab->readOnly || debugFuture_.valid());
        if (ImGui::InvisibleButton("##breakpoint",{line.width,line.height})) toggleBreakpoint(tab->path,line.line+1);
        if (ImGui::IsItemHovered()) cyui::tooltip((std::string(tr("Toggle breakpoint"))+" ("+keymap_.hint("debug.breakpoint")+")").c_str());
        ImGui::EndDisabled();
    });
    std::weak_ptr<Tab> weak = tab;
    TextEditor::AutoCompleteConfig completion;
    completion.suggestionWidth = 48;
    completion.noSuggestionsLabel = tr("No matching suggestions");
    completion.callback = [this, weak](TextEditor::AutoCompleteState &state) {
        auto tab = weak.lock();
        state.suggestions.clear(); state.suggestionsPromise = false;
        if (!tab || tab->readOnly || tab->snippet.active() || !LanguageService::supports(tab->path))
            return;
        tab->completionSession.clear();tab->completionItems.clear();
        if (cppFile(tab->path) && unreal_.value("available", false) && state.searchTerm.size() >= 2) {
            for (const auto &name : unrealLanguage_.identifiers) if (name.starts_with(state.searchTerm)) state.suggestions.push_back(name);
            for (const auto &name : unrealLanguage_.declarations) if (name.starts_with(state.searchTerm)) state.suggestions.push_back(name);
            std::sort(state.suggestions.begin(), state.suggestions.end());
        }
        if (smoke_) return;
        state.suggestionsPromise = true;
        auto position = state.searchTermEnd;
        auto prefix = tab->editor.GetSectionText({position.line, 0}, position);
        ++tab->completionGeneration;
        tab->queuedCompletionStart = state.searchTermStart;
        tab->queuedCompletionEnd = state.searchTermEnd;
        tab->queuedCompletion = {
            {"path", tab->path},
            {"expectedRevision", tab->revision},
            {"position", {{"line", position.line}, {"character", LanguageClient::utf16Length(prefix)}}}};
    };
    completion.accept=[this,weak](size_t selected) {
        auto tab=weak.lock();
        if (!tab || tab->completionSession.empty()) return false; // Local Unreal fallback.
        if (selected>=tab->completionItems.size() || tab->completionAccept.valid()) return true;
        if (tab->editor.GetNumberOfCursors()!=1) { error(tr("Completion with imports requires a single cursor."));return true; }
        if (tab->readOnly || tab->revision!=tab->completionRevision || tab->completionGeneration!=tab->runningCompletionGeneration || tab->editor.GetMainCursorPosition()!=tab->completionPosition) {
            error(tr("Completion context changed. Request suggestions again."));return true;
        }
        tab->acceptedRevision=tab->revision;tab->acceptedGeneration=tab->completionGeneration;tab->acceptedPosition=tab->editor.GetMainCursorPosition();
        auto engine=engine_;const Json parameters={{"sessionId",tab->completionSession},{"item",tab->completionItems[selected]},{"expectedRevision",tab->revision}};
        tab->queuedCompletion=nullptr;
        tab->completionAccept=std::async(std::launch::async,[engine,parameters] { return engine->dispatch("language.completionPrepare",parameters); });
        return true;
    };
    tab->editor.SetAutoCompleteConfig(&completion);
}
void Application::open(const std::string &path, size_t line, size_t column, bool utf16, bool activate) {
    if (openImage(path,activate)) return;
    if (path.starts_with("debug://")) { openDebugSource(path,line,column); return; }
    try {
        auto doc = path.starts_with("sdk://") ? engine_->dispatch("sources.read", {{"source", path}})
                                             : engine_->dispatch("documents.open", {{"path", path}});
        std::string canonical = doc["path"];
        std::shared_ptr<Tab> tab;
        for (auto &candidate : tabs_)
            if (candidate->path == canonical)
                tab = candidate;
        if (!tab) {
            tab = std::make_shared<Tab>();
            tab->path = canonical;
            tab->displayPath = doc.value("displayPath", canonical);
            tab->readOnly = doc.value("readOnly", false);
            tab->codeEditor.setText(doc["text"].get<std::string>());
            tab->revision = doc["revision"];
            tab->dirty = doc["dirty"];
            configureEditor(tab);
            tabs_.push_back(tab);
            const auto backend=languageBackend(tab->path);
            if (!smoke_ && !backend.empty() && backend!="cpp" && backend!="csharp" && !languageDecisions_.contains(backend)) {
                languageDecisions_.insert(backend);
                const auto server=engine_->languageStatus().value("servers",Json::object()).value(backend,Json::object());
                if (server.value("available",false) && !server.value("enabled",false))
                    engine_->dispatch("language.configure",{{"language",backend}});
            }
        } else if (tab->revision != doc["revision"].get<std::uint64_t>()) {
            tab->snippet.cancel();
            const auto cursor = tab->editor.GetMainCursorPosition();
            tab->editor.SelectAll();
            tab->editor.ReplaceTextPreservingTabs(doc["text"].get<std::string>());
            tab->syntax.update(tab->editor,tab->path,doc["text"].get_ref<const std::string &>());
            tab->editor.SetCursor(cursor);
            tab->revision = doc["revision"];
            tab->dirty = doc["dirty"];
            tab->queuedCompletion = nullptr;
            ++tab->completionGeneration;
            tab->editor.SetAutoCompleteSuggestions({});
            navigation_ = Json::object();
        }
        if (activate) {
            if (!historyMoving_ && (active_ != canonical || line)) rememberNavigation();
            std::erase(recentFiles_, canonical); recentFiles_.insert(recentFiles_.begin(), canonical);
            if (recentFiles_.size() > 100) recentFiles_.resize(100);
            active_ = canonical;
            focusTab_ = canonical;
            revealPath_ = canonical;
        }
        if (line) {
            const auto text = tab->editor.GetLineText(line - 1);
            size_t units = 0, index = 0;
            const size_t offset = column ? column - 1 : 0;
            for (size_t byte = 0; byte < text.size();) {
                auto c = static_cast<unsigned char>(text[byte]);
                if ((utf16 ? units : byte) >= offset)
                    break;
                byte += c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
                units += c >= 0xf0 ? 2 : 1;
                ++index;
            }
            tab->editor.SetCursor({line - 1, index});
            tab->editor.ScrollToLine(line - 1);
        }
        if (activate) tab->editor.SetFocus();
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::changed(const std::shared_ptr<Tab> &tab) {
    if (tab->readOnly) return;
    tab->editor.ClearSemanticTokens(); tab->highlightRevision=0; tab->highlightCount=0; tab->highlightRetryAt=ImGui::GetTime()+.3;
    try {
        auto result = engine_->dispatch(
            "documents.replace",
            {{"path", tab->path}, {"text", tab->editor.GetText()}, {"expectedRevision", tab->revision}});
        tab->revision = result["revision"];
        tab->dirty = result["dirty"];
        tab->syntax.update(tab->editor,tab->path,result["text"].get_ref<const std::string &>());
        ++tab->completionGeneration;
        navigation_ = Json::object();
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::save(const std::shared_ptr<Tab> &tab) {
    if (tab->readOnly) return;
    try {
        auto result =
            engine_->dispatch("documents.save", {{"path", tab->path}, {"expectedRevision", tab->revision}});
        tab->dirty = result["dirty"];
        cyui::notify(tr("Saved"), cyui::Tone::success);
        refreshGit();
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::saveAll() {
    for (const auto &tab : tabs_)
        if (tab->dirty)
            save(tab);
}
void Application::closeTab(const std::string &path, bool discard) {
    auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == path; });
    if (found == tabs_.end())
        return;
    if ((*found)->dirty && !discard) {
        pendingClose_ = path;
        return;
    }
    if ((*found)->image) { }
    else if ((*found)->readOnly && path.starts_with("sdk://")) engine_->dispatch("sources.close", {{"source", path}});
    else if (path.starts_with("debug://")) { }
    else engine_->dispatch("documents.close", {{"path", path}, {"discard", discard}});
    navigation_ = Json::object();
    if ((*found)->completion.valid())
        retiredCompletions_.push_back(std::move((*found)->completion));
    if ((*found)->completionAccept.valid()) retiredCompletions_.push_back(std::move((*found)->completionAccept));
    if ((*found)->highlighting.valid()) retiredCompletions_.push_back(std::move((*found)->highlighting));
    tabs_.erase(found);
    closedFiles_.push_back(path);
    if (closedFiles_.size() > 100) closedFiles_.erase(closedFiles_.begin());
    if (active_ == path)
        active_ = tabs_.empty() ? "" : tabs_.front()->path;
}
void Application::update() {
    updateImages();
    if (ready(unrealAnalysisFuture_)) {
        try {
            auto result = unrealAnalysisFuture_.get();
            if (unrealAnalysisCancellation_ && *unrealAnalysisCancellation_) unrealAnalysisState_ = "cancelled";
            else {
                output_ += result.value("output", std::string{});
                applyUnreal(result.at("project"));
                unrealAnalysisState_ = result.value("exitCode", -1) == 0 ? "ready" : "failed";
                if (unrealAnalysisState_ == "failed") error(tr("Unreal analysis failed. See Build output."));
                engine_->configureLanguage(clangd_, !smoke_, true);
            }
        } catch (const std::exception &e) { unrealAnalysisState_ = "failed"; error(e.what()); engine_->configureLanguage(clangd_, !smoke_, true); }
    }
    std::erase_if(retiredCompletions_, [](auto &future) {
        if (!ready(future))
            return false;
        try {
            future.get();
        } catch (const std::exception &) {
        }
        return true;
    });
    languageState_ = engine_->languageStatus();
    if (ready(buildFuture_)) {
        try {
            applyBuild(buildFuture_.get());
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(detectionFuture_)) {
        try {
            toolchains_ = detectionFuture_.get();
            if (msbuildActive_ && toolchains_.contains("tools")) { msbuildState_["tools"] = toolchains_.at("tools"); refreshBuild(); }
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    // Synchronize external edits before consuming any asynchronous language result.
    auto documents = engine_->dispatch("documents.list")["documents"];
    for (const auto &info : documents) {
        auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) {
            return tab->path == info["path"].get<std::string>();
        });
        if (found == tabs_.end())
            continue;
        auto &tab = *found;
        if (info["revision"] != tab->revision) {
            auto doc = engine_->dispatch("documents.read", {{"path", tab->path}});
            tab->snippet.cancel();
            auto cursor = tab->editor.GetMainCursorPosition();
            tab->editor.SelectAll();
            tab->editor.ReplaceTextPreservingTabs(doc["text"].get<std::string>());
            tab->syntax.update(tab->editor,tab->path,doc["text"].get_ref<const std::string &>());
            tab->editor.SetCursor(cursor);
            tab->revision = doc["revision"];
            tab->dirty = doc["dirty"];
            tab->queuedCompletion = nullptr;
            ++tab->completionGeneration;
            tab->editor.SetAutoCompleteSuggestions({});
            navigation_ = Json::object();
        }
    }
    updateSemantic();
    updateInformation();
    updateCodeActions();
    updateHighlights();
    updateTerminal();
    updateDebug();
    updateVcs();
    for (const auto &tab : tabs_) {
        if (ready(tab->completionAccept)) {
            try {
                const auto prepared=tab->completionAccept.get();
                if (tab->revision!=tab->acceptedRevision || tab->completionGeneration!=tab->acceptedGeneration || tab->editor.GetMainCursorPosition()!=tab->acceptedPosition || tab->editor.GetNumberOfCursors()!=1)
                    throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                const auto &plan=prepared.at("plan");
                if (!plan.at("files").empty()) {
                    const auto preview=engine_->dispatch("changes.preview",{{"id",plan.at("id")},{"path",tab->path}});
                    if (tab->editor.GetText()!=preview.at("before").get_ref<const std::string &>()) throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                    engine_->dispatch("changes.apply",{{"id",plan.at("id")}});
                    const auto doc=engine_->dispatch("documents.read",{{"path",tab->path}});
                    if (doc.at("text")!=preview.at("after")) throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                    const auto &cursor=prepared.at("cursor");
                    applyEditorCompletion(tab->editor,preview.at("before"),doc.at("text"),cursor.at("line"),cursor.at("character"));
                    tab->revision=doc.at("revision");tab->dirty=doc.at("dirty");
                    tab->syntax.update(tab->editor,tab->path,doc.at("text"));
                    tab->editor.ClearSemanticTokens();tab->highlightRevision=0;tab->highlightRetryAt=ImGui::GetTime()+.3;
                    ++tab->completionGeneration;navigation_=Json::object();
                }
                tab->snippet.begin(prepared.at("snippet"));
            } catch (const std::exception &e) { error(e.what()); }
            tab->completionSession.clear();tab->completionItems.clear();tab->editor.SetAutoCompleteSuggestions({});
        }
        if (ready(tab->completion)) {
            try {
                auto reply = tab->completion.get();
                if (tab->revision == tab->completionRevision &&
                    tab->completionGeneration == tab->runningCompletionGeneration &&
                    tab->editor.GetMainCursorPosition() == tab->completionPosition) {
                    const auto &response = reply["result"];
                    Json items = response.is_array()    ? response
                                 : response.is_object() ? response.value("items", Json::array())
                                                        : Json::array();
                    for (size_t i=0;i<items.size();++i) items[i]["cycodeItemIndex"]=i;
                    if (testSession_) tab->completionTrace = {{"count", items.size()}, {"first", items.empty() ? Json() : items[0]}};
                    const auto typedPrefix = tab->editor.GetSectionText(tab->runningCompletionStart,tab->completionPosition);
                    const bool rankPrefix=!LanguageService::supportsCpp(tab->path);
                    std::stable_sort(items.begin(), items.end(), [&](const Json &a, const Json &b) {
                        if (rankPrefix) {
                            const auto left=fuzzyScore(typedPrefix,a.value("filterText",a.value("label",std::string{})));
                            const auto right=fuzzyScore(typedPrefix,b.value("filterText",b.value("label",std::string{})));
                            if (left!=right) return left>right;
                        }
                        return a.value("sortText", a.value("label", std::string{})) < b.value("sortText", b.value("label", std::string{}));
                    });
                    std::vector<std::string> suggestions;
                    std::vector<size_t> indices;std::set<std::string> unique;
                    for (const auto &item : items) {
                        if (!item.is_object() || !completionCommandAllowed(item,languageBackend(tab->path)))
                            continue;
                        // Selection/ranking and editor-only hints do not alter
                        // the complete text edit. No completion command is run.
                        // Roslyn returns the member set; filtering is the client's job.
                        if (!LanguageService::supportsCpp(tab->path) &&
                            fuzzyScore(typedPrefix,item.value("filterText",item.value("label",std::string{}))) < 0) continue;
                        auto text = item.value("insertText", item.value("label", std::string{}));
                        if (item.contains("textEdit")) {
                            const auto &edit = item.at("textEdit");
                            text = item["textEdit"].value("newText", text);
                        }
                        if (item.value("insertTextFormat",1)==2) {
                            const auto plain=plainSnippet(text); if (!plain) continue; text=*plain;
                        }
                        if (text.find('\n')!=std::string::npos) text=item.value("label",std::string{});
                        const auto details=item.value("labelDetails",Json::object());
                        if (details.is_object() && details.contains("description") && details.at("description").is_string() && !details.at("description").get_ref<const std::string&>().empty()) text+="  — "+details.at("description").get<std::string>();
                        for (auto &c : text) if (static_cast<unsigned char>(c)<32 || c==127) c=' ';
                        if (text.size()>1024 || text.empty() || !unique.insert(text).second) continue;
                        suggestions.push_back(text);indices.push_back(item.at("cycodeItemIndex"));
                        if (suggestions.size() >= 100)
                            break;
                    }
                    tab->completionSession=reply.at("sessionId");tab->completionItems=std::move(indices);
                    tab->lastSuggestions = suggestions;
                    tab->editor.SetAutoCompleteSuggestions(suggestions);
                }
            } catch (const std::exception &e) {
                if (testSession_) tab->completionTrace = {{"error", e.what()}};
                tab->completionSession.clear();tab->completionItems.clear();
                tab->editor.SetAutoCompleteSuggestions({});
            }
        }
        if (!tab->completion.valid() && !tab->completionAccept.valid() && tab->queuedCompletion.is_object()) {
            if (tab->editor.GetMainCursorPosition() != tab->queuedCompletionEnd) {
                tab->queuedCompletion = nullptr; tab->editor.SetAutoCompleteSuggestions({}); continue;
            }
            // clangd's initialized state precedes the first parsed preamble. Before
            // that parse, completion returns unrelated index symbols (even with
            // isIncomplete=false). Keep the widget's promise until the document's
            // first diagnostics publication, including an empty diagnostics list.
            const auto uri = LanguageClient::uri(engine_->workspace() / utf8Path(tab->path));
            const auto backend=languageBackend(tab->path);
            const auto server = languageState_.value("servers",Json::object()).value(backend,Json::object());
            if (!server.value("ready", false) || (backend=="cpp" && (unrealAnalysisState_ == "preparing" || !server.value("diagnostics", Json::object()).contains(uri)))) continue;
            auto parameters = std::move(tab->queuedCompletion);
            tab->queuedCompletion = nullptr;
            tab->completionRevision = tab->revision;
            // The editor callback runs during Render(), before changed() commits the keystroke.
            parameters["expectedRevision"] = tab->revision;
            tab->completionPosition = tab->editor.GetMainCursorPosition();
            tab->runningCompletionStart = tab->queuedCompletionStart;
            tab->runningCompletionGeneration = tab->completionGeneration;
            auto engine = engine_;
            tab->completion = std::async(std::launch::async, [engine, parameters] {
                return engine->dispatch("language.complete", parameters);
            });
        }
    }
    if (ready(navigationFuture_)) {
        try {
            auto result = navigationFuture_.get();
            const auto source = std::find_if(tabs_.begin(), tabs_.end(),
                                             [&](const auto &tab) { return tab->path == navigationPath_; });
            if (source == tabs_.end() || (*source)->revision != result["revision"].get<std::uint64_t>())
                throw std::runtime_error("RevisionConflict: navigation result is obsolete");
            navigation_ = std::move(result);
            const auto &locations = navigation_["locations"];
            bool cursorUnchanged = false;
            for (const auto &tab : tabs_)
                if (tab->path == navigationPath_ && active_ == navigationPath_)
                    cursorUnchanged = tab->editor.GetMainCursorPosition() == navigationPosition_;
            if (!navigationReferences_ && locations.size() == 1 && cursorUnchanged) {
                const auto &location = locations.front();
                const auto &start = location["range"]["start"];
                open(location["path"], start["line"].get<size_t>() + 1, start["character"].get<size_t>() + 1,
                     true);
            } else
                showPanel("References", "references");
            if (locations.empty())
                cyui::notify(tr("No locations found"), cyui::Tone::neutral);
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(filesFuture_)) {
        try {
            auto result = filesFuture_.get();
            files_ = result["files"].get<std::vector<std::string>>();
            tree_ = {};
            for (const auto &path : files_) {
                Tree *node = &tree_;
                auto fs = utf8Path(path);
                for (const auto &part : fs.parent_path())
                    node = &node->folders[pathUtf8(part)];
                node->files.push_back(path);
            }
            if (result.value("truncated", false))
                output_ += std::string(tr("File list limited to 20,000 entries.")) + "\n";
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(gitFuture_)) {
        try {
            auto result = gitFuture_.get(); git_ = result.at("status");
            vcsInfo_ = result.value("details",Json::object()); vcsHistory_ = result.value("history",Json::object());
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(taskFuture_)) {
        try {
            auto result = taskFuture_.get();
            if (taskMethod_.starts_with("msbuild.") || taskMethod_.starts_with("project.")) buildDiagnostics_ = result.value("diagnostics",Json::array());
            if (taskMethod_ == "unreal.generate" && result.value("exitCode", -1) == 0) {
                solution_ = engine_->dispatch("workspace.describe").at("solution");
                refreshFiles();
            }
            if (taskMethod_ == "unreal.inspect")
                applyUnreal(result);
            else if (taskMethod_.starts_with("unreal.") && result.contains("project"))
                applyUnreal(result.at("project"));
            else if (result.contains("project") && result["project"].is_object()) {
                buildEdited_ = false;
                applyBuild(result["project"]);
            }
            output_ += result.contains("output") ? result["output"].get<std::string>() : result.dump(2);
            output_ += "\n";
            if (result.contains("terminal")) {
                terminalId_ = result.at("terminal").at("id"); showPanel("Terminal","terminal"); focusTerminal_ = true; terminalScrollBottom_ = true;
            }
            if (taskMethod_=="project.debug" && result.contains("program")) {
                debugProgram_ = result.at("program"); debugRuntime_ = "native"; debugCwd_ = projectProfile_.value("directory",".");
                debugArgs_.clear(); for (const auto &arg : projectProfile_.value("args",Json::array())) debugArgs_ += arg.get<std::string>()+"\n";
            }
            if (pendingDebugBuild_) { pendingDebugBuild_ = false; if (result.value("exitCode",-1) == 0 && !result.value("cancelled",false)) startDebug(false); }
            if (result.value("cancelled",false)) cyui::notify(tr("Task cancelled"),cyui::Tone::warning);
            else if (result.value("exitCode", 0) != 0)
                { error(tr("Task failed. See output.")); if (!buildDiagnostics_.empty()) showPanel("Problems","problems"); }
            else
                cyui::notify(tr("Task completed"), cyui::Tone::success);
            refreshGit();
        } catch (const std::exception &e) {
            pendingDebugBuild_ = false;
            error(e.what());
        }
    }
    if (ready(searchFuture_)) {
        try {
            auto result = searchFuture_.get();
            if (runningSearchGeneration_ == searchGeneration_)
                search_ = std::move(result);
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (searchQueued_ && !searchFuture_.valid() && ImGui::GetTime() - searchChanged_ > .2) {
        searchQueued_ = false;
        runningSearchGeneration_ = searchGeneration_;
        auto engine = engine_;
        searchCancellation_ = std::make_shared<std::atomic<bool>>(false);
        auto cancellation = searchCancellation_;
        auto parameters = Json{
            {"query", searchQuery_}, {"caseSensitive", caseSensitive_}, {"regex", regex_}, {"limit", 500}};
        searchFuture_ = std::async(std::launch::async, [engine, parameters, cancellation] {
            return engine->searchText(parameters, cancellation.get());
        });
    }
    {
        std::lock_guard lock(dialog_->mutex);
        if (dialog_->ready) {
            dialog_->ready = false;
            if (!dialog_->path.empty())
                try {
                    if (dialog_->image) openImageFile(utf8Path(dialog_->path));
                    else if (dialog_->newWindow)
                        newWindow(utf8Path(dialog_->path));
                    else
                        openProject(utf8Path(dialog_->path));
                } catch (const std::exception &e) {
                    error(e.what());
                }
            if (!dialog_->error.empty())
                error(dialog_->error);
        }
    }
    if (!pendingWorkspace_.empty() && !busy()) {
        auto path = std::move(pendingWorkspace_); pendingWorkspace_.clear();
        try { setWorkspace(utf8Path(path)); } catch (const std::exception &e) { error(e.what()); }
    }
    if (pendingWorkspace_.empty() && ImGui::GetTime() - lastGitRefresh_ > 5 && !smoke_)
        refreshGit();
    if (pendingWorkspace_.empty() && !smoke_ && !taskFuture_.valid() && ImGui::GetTime() - lastBuildRefresh_ > 5)
        refreshBuild();
    if (testSession_ && testCompletion_.is_object() && unrealAnalysisState_ != "preparing" && testCompletionPhase_ == 0) {
        const auto &position = testCompletion_.at("position");
        open(testCompletion_.at("path"), position.at("line").get<size_t>() + 1, position.at("character").get<size_t>() + 1, true);
        testCompletionPhase_ = 1;
    } else if (testCompletionPhase_ == 1 && editorFocused_ && languageState_.value("ready", false)) {
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true); ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftCtrl, true); ImGui::GetIO().AddKeyEvent(ImGuiKey_Space, true); testCompletionPhase_ = 2;
    } else if (testCompletionPhase_ == 2) {
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false); ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftCtrl, false); ImGui::GetIO().AddKeyEvent(ImGuiKey_Space, false); testCompletionPhase_ = 3;
    } else if (testCompletionPhase_ == 3 && testCompletion_.value("acceptFirst", false)) {
        for (const auto &tab : tabs_) if (tab->path == active_ && !tab->lastSuggestions.empty() && !tab->completion.valid()) { ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true); testCompletionPhase_ = 4; }
    } else if (testCompletionPhase_ == 4) {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false); testCompletionPhase_ = 5;
    }
    if (testSession_ && testNavigation_.is_object() && languageState_.value("ready", false)) {
        const auto position = testNavigation_.at("position");
        open(testNavigation_.at("path"), position.at("line").get<size_t>() + 1,
             position.at("character").get<size_t>() + 1, true);
        navigate(testNavigation_.value("references", false));
        testNavigation_ = nullptr;
    }
}
void Application::chooseFolder() {
    {
        std::lock_guard lock(dialog_->mutex);
        dialog_->path.clear();
        dialog_->error.clear();
        dialog_->newWindow = false;
        dialog_->image = false;
    }
    auto *state = new std::shared_ptr<Dialog>(dialog_);
    SDL_ShowOpenFolderDialog(
        [](void *user, const char *const *paths, int) {
            auto state = *static_cast<std::shared_ptr<Dialog> *>(user);
            delete static_cast<std::shared_ptr<Dialog> *>(user);
            std::lock_guard lock(state->mutex);
            state->ready = true;
            if (paths && paths[0])
                state->path = paths[0];
            else if (!paths)
                state->error = SDL_GetError();
        },
        state, window_, pathUtf8(engine_->workspace()).c_str(), false);
}
void Application::chooseSolution(bool separate) {
    {
        std::lock_guard lock(dialog_->mutex);
        dialog_->path.clear();
        dialog_->error.clear();
        dialog_->newWindow = separate;
        dialog_->image = false;
    }
    static const SDL_DialogFileFilter filters[] = {{"Solutions / Unreal / Cargo / npm / Java", "sln;uproject;vcxproj;csproj;fsproj;vbproj;toml;json;java"},
                                                   {"Unreal project (.uproject)", "uproject"},
                                                   {"Visual Studio solution (.sln)", "sln"}};
    auto *state = new std::shared_ptr<Dialog>(dialog_);
    SDL_ShowOpenFileDialog(
        [](void *user, const char *const *paths, int) {
            auto state = *static_cast<std::shared_ptr<Dialog> *>(user);
            delete static_cast<std::shared_ptr<Dialog> *>(user);
            std::lock_guard lock(state->mutex);
            state->ready = true;
            if (paths && paths[0])
                state->path = paths[0];
            else if (!paths)
                state->error = SDL_GetError();
        },
        state, window_, filters, 3, pathUtf8(engine_->workspace()).c_str(), false);
}
void Application::newWindow(const std::filesystem::path &entry) {
#ifdef _WIN32
    auto executable = utf8Path(SDL_GetBasePath()) / "CyCode.exe";
#else
    auto executable = utf8Path(SDL_GetBasePath()) / "CyCode";
#endif
    std::vector<std::string> command{pathUtf8(executable), "--workspace", pathUtf8(entry), "--theme", theme_,
                                     "--language",         locale_, "--keymap", keymapProfile_};
    if (std::filesystem::is_regular_file(entry) && lower(pathUtf8(entry.extension())) != ".sln" &&
        lower(pathUtf8(entry.extension())) != ".uproject") {
        const auto ext = lower(pathUtf8(entry.extension()));
        if (ext != ".vcxproj" && ext != ".csproj" && ext != ".fsproj" && ext != ".vbproj") {
        command[2] = pathUtf8(entry.parent_path());
        command.push_back("--open");
        command.push_back(pathUtf8(entry.filename()));
        }
    }
    std::vector<const char *> argv;
    for (const auto &arg : command)
        argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    auto props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    auto process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process)
        error(SDL_GetError());
    else
        SDL_DestroyProcess(process);
}
bool Application::beginPanel(const char *text, const char *id) {
    auto [found, inserted] = panels_.try_emplace(id, true);
    if (!found->second)
        return false;
    if (focusPanel_ == id) {
        ImGui::SetNextWindowFocus();
        focusPanel_.clear();
    }
    if (ImGui::Begin(label(text, id).c_str(), &found->second))
        return true;
    ImGui::End();
    return false;
}
void Application::showPanel(const char *text, const char *id) {
    if (std::string(id) == "search") focusSearch_ = true;
    panels_[id] = true;
    focusPanel_ = id;
    ImGui::SetWindowFocus(label(text, id).c_str());
}
void Application::menu() {
    if (!ImGui::BeginMainMenuBar()) return;
    for (const auto *category : {"File", "Edit", "Search", "Navigate", "Refactor", "Build", "Run", "View", "Project", "Version control"}) {
        if (!ImGui::BeginMenu(tr(category))) continue;
        std::vector<cyui::FilterMenuItem> items;
        for (const auto &command : commands()) if (command.category == category)
            items.push_back({uiId("command/" + command.id), command.title, keymap_.hint(command.id), command.id,
                             commandEnabled(command.id), false, {}});
        const auto request = cyui::filterMenu(category, menuFilters_[category], items);
        if (request) for (const auto &command : commands()) if (uiId("command/" + command.id) == request) { executeCommand(command.id); break; }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}
void Application::topBar() {
    cyui::beginTopBar("##top", 44);
    {
        cyui::ScopedFont bold(cyui::boldFont(), 1.1f);
        cyui::textColoured(cyui::palette().accent, "CYCODE");
    }
    ImGui::SameLine();
    cyui::badge(CYCODE_VERSION, cyui::Tone::neutral);
    ImGui::SameLine();
    cyui::toolbarSeparator();
    ImGui::SameLine();
    if (cyui::iconTextButton(cyui::icon::folder, pathUtf8(engine_->workspace().filename()).c_str()))
        chooseFolder();
    ImGui::SameLine();
    cyui::textFaint("/");
    ImGui::SameLine();
    cyui::textDim("%s", (unreal_.value("available", false)
                             ? unrealTarget_ + " | " + unrealConfiguration_ + " | " + unrealPlatform_
                             : configuration_)
                            .c_str());
    ImGui::SameLine();
    cyui::toolbarSeparator();
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::save, (std::string(tr("Save")) + " (" + keymap_.hint("file.save") + ")").c_str()))
        executeCommand("file.save");
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::repair, (std::string(tr("Build project")) + " (" + keymap_.hint("build.run") + ")").c_str(), taskFuture_.valid()))
        executeCommand("build.run");
    ImGui::SameLine();
    cyui::alignRight(cyui::dp(335));
    if (cyui::ghostButton((std::string(tr("Go to file...")) + "   " + keymap_.hint("search.files")).c_str(), cyui::dp(260, 28)))
        executeCommand("search.files");
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::settings, tr("Options")))
        options_ = true;
    cyui::endTopBar();
}
void Application::statusBar() {
    cyui::beginStatusBar("##status");
    cyui::statusText(cyui::Tone::accent, "%s", busy() ? tr("Working...") : tr("Ready"));
    if (unrealAnalysisState_ == "preparing") { ImGui::SameLine(); cyui::textDim("%s", tr("Preparing Unreal code analysis...")); }
    if (auto pending = keymap_.pending(ImGui::GetTime()); !pending.empty()) { ImGui::SameLine(); cyui::textDim("%s", pending.c_str()); }
    ImGui::SameLine();
    cyui::textFaint(" | ");
    ImGui::SameLine();
    cyui::textDim("%s", git_.value("repository", false) ? git_.value("branch", std::string{}).c_str()
                                                        : tr("No repository"));
    for (const auto &tab : tabs_)
        if (tab->path == active_) {
            ImGui::SameLine();
            cyui::alignRight(cyui::dp(420));
            if (tab->image) { cyui::textDim("%s",tr("Image preview")); continue; }
            auto pos = tab->editor.GetMainCursorPosition();
            cyui::textDim("%s  |  Ln %zu, Col %zu  |  UTF-8", tab->editor.GetLanguageName().c_str(),
                          pos.line + 1, pos.index + 1);
        }
    cyui::endStatusBar();
}
void Application::rail() {
    cyui::beginSidebar("##rail", 48);
    auto button = [&](const char *icon, const char *text, const char *panel) {
        if (cyui::iconButton(icon, tr(text), false, 0, cyui::dp(32, 34)))
            showPanel(text, panel);
        cyui::space(6);
    };
    button(cyui::icon::folder, "Project", "project");
    button(cyui::icon::search, "Search", "search");
    button(cyui::icon::sync, "Version control", "vcs");
    button(cyui::icon::repair, "Build", "build");
    button(cyui::icon::library, "Extensions", "extensions");
    button(cyui::icon::connect, "MCP", "mcp");
    cyui::endSidebar();
}
void Application::layout(ImGuiID dock) {
    for (auto &[id, visible] : panels_)
        visible = id != "refactoring" && id != "terminal" && id != "debugger" && id != "codeActions";
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    auto center = dock;
    auto left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .23f, nullptr, &center);
    auto bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .29f, nullptr, &center);
    ImGui::DockBuilderDockWindow(label("Project", "project").c_str(), left);
    ImGui::DockBuilderDockWindow(label("Search", "search").c_str(), left);
    ImGui::DockBuilderDockWindow("##editors", center);
    for (const auto &[text, id] :
         std::vector<std::pair<const char *, const char *>>{{"Build", "build"},
                                                            {"Unreal Engine", "unreal"},
                                                            {"CMake", "cmake"},
                                                            {"MSBuild", "msbuild"},
                                                            {"Project tasks", "projectTasks"},
                                                            {"Version control", "vcs"},
                                                            {"Problems", "problems"},
                                                            {"References", "references"},
                                                            {"Code information", "information"},
                                                            {"Code actions", "codeActions"},
                                                            {"Refactoring", "refactoring"},
                                                            {"Terminal", "terminal"},
                                                            {"Debugger", "debugger"},
                                                            {"Extensions", "extensions"},
                                                            {"MCP", "mcp"}})
        ImGui::DockBuilderDockWindow(label(text, id).c_str(), bottom);
    ImGui::DockBuilderFinish(dock);
    layoutReady_ = true;
}
void Application::tree(Tree &node, const std::string &prefix) {
    for (auto &[name, child] : node.folders) {
        auto key = prefix + name;
        ImGui::PushID(key.c_str());
        if (revealPath_.starts_with(key + '/'))
            ImGui::SetNextItemOpen(true);
        bool open = fileTreeNode(name, key,
                                 ImGuiTreeNodeFlags_SpanAvailWidth |
                                     (prefix.empty() && (!unreal_.value("available", false) ||
                                                         name == "Source" || name == "Plugins")
                                          ? ImGuiTreeNodeFlags_DefaultOpen
                                          : 0),
                                 theme_, true);
        if (open) {
            tree(child, key + "/");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(node.files.size()));
    while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto &path = node.files[static_cast<size_t>(index)];
            bool selected = active_ == path;
            auto name = pathUtf8(utf8Path(path).filename());
            ImGui::PushID(path.c_str());
            if (fileSelectable(name, path, selected, theme_))
                open(path);
            cyui::tooltip(path.c_str());
            ImGui::PopID();
        }
}
void Application::solutionTree(const std::string &parent, int depth) {
    if (depth > 64)
        return;
    for (const auto &project : solution_.at("projects")) {
        if (project.at("parent") != parent)
            continue;
        const auto id = project.at("id").get<std::string>();
        const bool folder = project.at("folder"), exists = project.at("exists"),
                   external = project.at("external");
        auto name = project.at("name").get<std::string>();
        if (!exists)
            name += " (" + std::string(tr("Missing")) + ")";
        else if (external)
            name += " (" + std::string(tr("External")) + ")";
        ImGui::PushID(id.c_str());
        if (!exists)
            ImGui::PushStyleColor(ImGuiCol_Text, cyui::vec(cyui::palette().warning));
        bool expanded =
            fileTreeNode(name, project.at("path"),
                         ImGuiTreeNodeFlags_SpanAvailWidth | (folder ? 0 : ImGuiTreeNodeFlags_Leaf) |
                             (depth < 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0),
                         theme_, folder);
        if (!exists)
            ImGui::PopStyleColor();
        cyui::tooltip(project.at("path").get_ref<const std::string &>().c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !folder &&
            exists && !external)
            open(project.at("relativePath"));
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem(tr("Open project file"), nullptr, false, exists && !folder && !external))
                open(project.at("relativePath"));
            if (ImGui::MenuItem(tr("Open in new window"), nullptr, false, exists && !folder))
                newWindow(utf8Path(project.at("path")));
            ImGui::EndPopup();
        }
        if (expanded) {
            for (const auto &item : project.at("items")) {
                auto path = item.at("path").get<std::string>();
                if (fileSelectable(pathUtf8(utf8Path(path).filename()), path, false, theme_) &&
                    item.at("exists").get<bool>() && !item.at("external").get<bool>())
                    open(item.at("relativePath"));
            }
            solutionTree(id, depth + 1);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}
void Application::projectPanel() {
    if (!beginPanel("Project", "project"))
        return;
    cyui::overline(pathUtf8(engine_->workspace().filename()).c_str());
    ImGui::SameLine();
    cyui::alignRight(cyui::dp(24));
    if (cyui::iconButton(cyui::icon::refresh, tr("Refresh")))
        refreshFiles();
    ImGui::SetNextItemWidth(-1);
    viewFilter("##filter", filesFilter_);
    cyui::divider();
    if (solution_.is_object()) {
        cyui::textDim("%s", (solution_.at("name").get<std::string>() + ".sln").c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##solution-config", solutionConfiguration_.c_str())) {
            for (const auto &config : solution_.at("configurations")) {
                auto text = config.get<std::string>();
                if (ImGui::Selectable(text.c_str(), text == solutionConfiguration_)) {
                    solutionConfiguration_ = text;
                    if (msbuildActive_) { auto split = text.find('|'); msbuildConfiguration_ = text.substr(0,split); msbuildPlatform_ = text.substr(split+1); buildEdited_ = true; refreshBuild(); }
                    if (unreal_.value("available", false)) {
                        auto bar = text.find('|'), space = text.find(' ');
                        unrealConfiguration_ = text.substr(0, std::min(space, bar));
                        if (bar != std::string::npos)
                            unrealPlatform_ = text.substr(bar + 1);
                        const bool editor = text.find("Editor") != std::string::npos;
                        for (const auto &t : unreal_.at("targets")) {
                            auto name = t.at("name").get<std::string>();
                            if (name.ends_with("Editor") == editor) {
                                unrealTarget_ = name;
                                break;
                            }
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (solution_.at("missingProjects").get<size_t>()) {
            cyui::textColoured(cyui::palette().warning, "%s: %zu", tr("Missing project files"),
                               solution_.at("missingProjects").get<size_t>());
            cyui::tooltip(tr("Generated Unreal project files may be absent. Available source files remain "
                             "accessible below."));
        }
    }
    ImGui::BeginChild("##files");
    if (solution_.is_object() && !filesFilter_.active()) {
        if (ImGui::TreeNodeEx(tr("Solution"), ImGuiTreeNodeFlags_DefaultOpen)) {
            solutionTree("");
            ImGui::TreePop();
        }
        cyui::divider();
        cyui::overline(tr("Workspace files"));
    }
    if (files_.empty())
        cyui::emptyState(tr(filesFuture_.valid() ? "Loading files..." : "No files"), tr("Open a workspace or refresh the file list."),
                         cyui::icon::folder);
    else if (!filesFilter_.active()) {
        tree(tree_, "");
        revealPath_.clear();
    } else {
        for (const auto &path : files_)
            if (filesFilter_.matches(path, path))
                if (fileSelectable(path, path, active_ == path, theme_))
                    open(path);
    }
    ImGui::EndChild();
    ImGui::End();
}
TextEditor::Palette Application::editorPalette(const std::string &theme) {
    return editorColours(theme);
}
void Application::editorPanel() {
    editorFocused_ = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, cyui::dp(0, 0));
    ImGuiWindowClass hostClass;
    hostClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&hostClass);
    bool visible = ImGui::Begin("##editors", nullptr, ImGuiWindowFlags_NoTitleBar);
    editorDock_ = ImGui::GetID("##document-dock");
    ImGui::DockSpace(editorDock_, ImVec2(0, 0),
                     ImGuiDockNodeFlags_PassthruCentralNode |
                         (visible && !tabs_.empty() ? 0 : ImGuiDockNodeFlags_KeepAliveOnly));
    if (visible && tabs_.empty()) {
        cyui::space(80);
        cyui::heading("CyCode", 1);
        cyui::textDim("%s", tr("A workspace for your code."));
        cyui::space(12);
        if (cyui::primaryButton(tr("Open solution...")))
            chooseSolution();
        ImGui::SameLine();
        if (cyui::button(tr("Open workspace...")))
            chooseFolder();
    }
    ImGui::End();
    std::string close;
    for (auto &tab : tabs_) {
        bool opened = true;
        auto title = documentTitle(tab->path);
        ImGui::SetNextWindowDockID(editorDock_, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(cyui::dp(850, 650), ImGuiCond_FirstUseEver);
        if (focusTab_ == tab->path) {
            ImGui::SetNextWindowFocus();
            focusTab_.clear();
        }
        const bool rendered =
            ImGui::Begin(title.c_str(), &opened, ImGuiWindowFlags_NoFocusOnAppearing | (tab->dirty ? ImGuiWindowFlags_UnsavedDocument : 0));
        documentIcon(tab->path, theme_);
        tab->visible=rendered;
        if (rendered) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                active_ = tab->path;
            cyui::space(4);
            cyui::textFaint("  %s", tab->displayPath.c_str());
            if (tab->image) imagePanel(tab);
            else {
            if (tab->snippet.active()) {
                ImGui::SameLine();cyui::badge((std::string(tr("Snippet parameter"))+" "+std::to_string(tab->snippet.index())).c_str(),cyui::Tone::accent);
                cyui::tooltip(tr("Tab: next | Shift+Tab: previous | Esc: finish"));
                const auto choices=tab->snippet.choices();
                if (!choices.empty()) {
                    ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(160));
                    if (ImGui::BeginCombo(("##snippet-choice"+tab->path).c_str(),tr("Snippet choices"))) {
                        for (size_t i=0;i<choices.size();++i) { ImGui::PushID(static_cast<int>(i));if (ImGui::Selectable(choices[i].c_str())) {
                            try { tab->snippet.choose(i);changed(tab);tab->editor.SetFocus(); } catch (const std::exception &e) { error(e.what()); }
                        }ImGui::PopID(); }
                        ImGui::EndCombo();
                    }
                }
            }
            if (tab->readOnly) { ImGui::SameLine(); cyui::badge(tr(tab->path.starts_with("debug://") ? "Read-only debugger source" : "Read-only SDK source"), cyui::Tone::neutral); }
            cyui::divider(4);
            const auto undoBefore = tab->editor.GetUndoIndex();
            const bool renderedChange = tab->codeEditor.draw(("##text" + tab->path).c_str());
            // Popup edits occur after the widget's Render change flag is computed.
            // Include their transactions so completion/replace reaches Engine now.
            if (renderedChange || tab->editor.GetUndoIndex() != undoBefore)
                changed(tab);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !(ImGui::GetActiveID() && ImGui::GetCurrentContext()->InputTextState.ID == ImGui::GetActiveID())) editorFocused_ = true;
            }
        }
        if (ImGui::BeginPopupContextWindow("##document-actions", ImGuiPopupFlags_MouseButtonRight |
                                                                     ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem(tr("Move to new window"))) {
                ImGui::SetWindowDock(ImGui::FindWindowByName(title.c_str()), 0, ImGuiCond_Always);
            }
            if (ImGui::MenuItem(tr("Return to editor"))) {
                ImGui::DockBuilderDockWindow(title.c_str(), editorDock_);
            }
            ImGui::EndPopup();
        }
        ImGui::End();
        if (!opened)
            close = tab->path;
    }
    if (!close.empty())
        closeTab(close);
    ImGui::PopStyleVar();
}
void Application::searchPanel() {
    if (!beginPanel("Search", "search"))
        return;
    if (focusSearch_) { ImGui::SetKeyboardFocusHere(); focusSearch_ = false; }
    bool changed = ImGui::InputTextWithHint("##query", tr("Find in workspace..."), &searchQuery_);
    changed |= ImGui::Checkbox(tr("Case sensitive"), &caseSensitive_);
    ImGui::SameLine();
    changed |= ImGui::Checkbox(tr("Regex"), &regex_);
    if (changed) {
        if (searchCancellation_)
            *searchCancellation_ = true;
        searchChanged_ = ImGui::GetTime();
        ++searchGeneration_;
        searchQueued_ = true;
    }
    if (searchFuture_.valid())
        cyui::spinner("##searching");
    cyui::divider();
    if (!search_.contains("matches"))
        cyui::emptyState(tr("Search your project"),
                         tr("Results include unsaved documents for literal searches."), cyui::icon::search);
    else {
        cyui::textDim("%zu %s", search_["matches"].size(), tr("matches"));
        if (search_.value("truncated", false))
            cyui::badge(tr("Results limited"), cyui::Tone::warning);
        if (search_.value("unsavedRegexExcluded", false))
            cyui::callout(cyui::Tone::warning, tr("Regex excludes modified buffers."));
        ImGui::BeginChild("##results");
        for (const auto &hit : search_["matches"]) {
            auto path = hit["path"].get<std::string>();
            auto line = hit["line"].get<size_t>();
            auto title = path + ":" + std::to_string(line) + "  " + hit["text"].get<std::string>();
            ImGui::PushID(title.c_str());
            if (ImGui::Selectable(title.c_str()))
                open(path, line, hit["column"].get<size_t>());
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
void Application::outputPanel() {
    if (!beginPanel("Build", "build"))
        return;
    if (projectActive_) {
        cyui::textDim("%s",projectState_.value("name",projectState_.value("kind","")).c_str()); ImGui::SameLine();
        { cyui::ScopedDisabled disabled(taskFuture_.valid()); if (cyui::button(tr("Build project"))) task("build.run"); }
        ImGui::SameLine(); if (cyui::button(tr("Project tasks"))) showPanel("Project tasks","projectTasks");
        if (taskFuture_.valid() && taskMethod_.starts_with("project.")) { ImGui::SameLine(); if (cyui::button(tr("Cancel task"))) engine_->dispatch("project.cancel"); }
        ImGui::SameLine(); if (cyui::ghostButton(tr("Copy output"))) ImGui::SetClipboardText(output_.c_str());
        outputLog_.synchronize(output_); cyui::logView("##output", outputLog_.state, outputLog_.buffer);
        ImGui::End(); return;
    }
    if (msbuildActive_) {
        cyui::textDim("%s | %s",msbuildConfiguration_.c_str(),msbuildPlatform_.c_str()); ImGui::SameLine();
        { cyui::ScopedDisabled disabled(taskFuture_.valid()); if (cyui::button(tr("Build project"))) task("build.run"); ImGui::SameLine(); if (cyui::button(tr("MSBuild settings"))) showPanel("MSBuild","msbuild"); }
        ImGui::SameLine(); if (cyui::ghostButton(tr("Copy output"))) ImGui::SetClipboardText(output_.c_str());
        outputLog_.synchronize(output_); cyui::logView("##output", outputLog_.state, outputLog_.buffer);
        ImGui::End(); return;
    }
    ImGui::SetNextItemWidth(cyui::dp(100));
    auto &configuration = unreal_.value("available", false) ? unrealConfiguration_ : configuration_;
    const std::vector<std::string> configurations =
        unreal_.value("available", false)
            ? std::vector<std::string>{"Debug", "DebugGame", "Development", "Shipping", "Test"}
            : std::vector<std::string>{"Debug", "Release", "RelWithDebInfo"};
    if (ImGui::BeginCombo("##config", configuration.c_str())) {
        for (const auto &value : configurations)
            if (ImGui::Selectable(value.c_str(), configuration == value)) {
                configuration = value;
                buildEdited_ = true;
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (cyui::button(tr(unreal_.value("available", false) ? "Generate project files" : "Configure")))
        task("build.configure", {{"source", buildSource_}, {"buildDirectory", buildDirectory_}});
    ImGui::SameLine();
    if (cyui::button(tr("Build project")))
        task("build.run", {{"source", buildSource_},
                           {"buildDirectory", buildDirectory_},
                           {"configuration", configuration_}});
    ImGui::SameLine();
    if (taskFuture_.valid())
        cyui::spinner("##task");
    ImGui::SameLine();
    if (cyui::ghostButton(tr("Copy output")))
        ImGui::SetClipboardText(output_.c_str());
    ImGui::SameLine();
    if (cyui::ghostButton(tr(unreal_.value("available", false) ? "Unreal Engine" : "CMake settings")))
        showPanel(unreal_.value("available", false) ? "Unreal Engine" : "CMake",
                  unreal_.value("available", false) ? "unreal" : "cmake");
    outputLog_.synchronize(output_); cyui::logView("##output", outputLog_.state, outputLog_.buffer);
    ImGui::End();
}
void Application::applyUnreal(const Json &value) {
    unreal_ = value;
    if (!value.value("available", false))
        return;
    const auto &profile = value.at("profile");
    unrealProject_ = profile.at("project");
    unrealEngine_ = profile.at("engineRoot");
    unrealTarget_ = profile.at("target");
    unrealPlatform_ = profile.at("platform");
    unrealConfiguration_ = profile.at("configuration");
    if (solution_.is_object()) {
        auto selected =
            unrealConfiguration_ + (unrealTarget_.ends_with("Editor") ? " Editor|" : "|") + unrealPlatform_;
        for (const auto &config : solution_.at("configurations"))
            if (config == selected)
                solutionConfiguration_ = selected;
    }
}
void Application::unrealPanel() {
    if (!beginPanel("Unreal Engine", "unreal"))
        return;
    if (!unreal_.value("available", false)) {
        cyui::emptyState(tr("No Unreal project"), tr("Open a .uproject or its .sln solution."),
                         cyui::icon::folder);
        if (cyui::button(tr("Open solution...")))
            chooseSolution();
        ImGui::End();
        return;
    }
    if (unrealAnalysisState_ == "preparing") {
        cyui::spinner("##unreal-analysis"); ImGui::SameLine(); cyui::textDim("%s", tr("Preparing Unreal code analysis..."));
        ImGui::TextWrapped("%s", tr("Includes, reflection macros and generated headers are being prepared in the background."));
    } else if (unrealAnalysisState_ == "ready") cyui::textColoured(cyui::palette().success, "%s", tr("Unreal code analysis ready"));
    else if (unrealAnalysisState_ == "failed") {
        cyui::textColoured(cyui::palette().warning, "%s", tr("Unreal analysis failed. See Build output."));
        if (cyui::button(tr("Retry Unreal analysis"))) prepareUnreal();
    }
    cyui::sectionTitle(unreal_.value("name", "Unreal Engine").c_str());
    ImGui::SameLine();
    cyui::badge(unreal_.value("engineAssociation", "").c_str(), cyui::Tone::neutral);
    if (cyui::beginProperties("##unreal-settings", 150)) {
        cyui::property(tr("Project"));
        cyui::textDim("%s", unrealProject_.c_str());
        cyui::property(tr("Engine directory"));
        ImGui::InputText("##ue-engine", &unrealEngine_);
        cyui::property(tr("Target"));
        if (ImGui::BeginCombo("##ue-target", unrealTarget_.c_str())) {
            for (const auto &t : unreal_.at("targets")) {
                auto name = t.at("name").get<std::string>();
                if (ImGui::Selectable(name.c_str(), unrealTarget_ == name))
                    unrealTarget_ = name;
            }
            ImGui::EndCombo();
        }
        cyui::property(tr("Platform"));
        if (ImGui::BeginCombo("##ue-platform", unrealPlatform_.c_str())) {
            for (auto value : {"Win64", "Linux", "LinuxArm64", "Mac", "Android", "IOS"})
                if (ImGui::Selectable(value, unrealPlatform_ == value))
                    unrealPlatform_ = value;
            ImGui::EndCombo();
        }
        cyui::property(tr("Configuration"));
        if (ImGui::BeginCombo("##ue-configuration", unrealConfiguration_.c_str())) {
            for (auto value : {"Debug", "DebugGame", "Development", "Shipping", "Test"})
                if (ImGui::Selectable(value, unrealConfiguration_ == value))
                    unrealConfiguration_ = value;
            ImGui::EndCombo();
        }
        cyui::endProperties();
    }
    if (cyui::button(tr("Refresh Unreal project")))
        task("unreal.inspect");
    ImGui::SameLine();
    if (cyui::button(tr("Generate project files")))
        task("unreal.generate");
    ImGui::SameLine();
    if (cyui::button(tr("Generate code index")))
        task("unreal.database");
    ImGui::SameLine();
    if (cyui::primaryButton(tr("Build project")))
        task("unreal.build");
    cyui::divider();
    if (!unreal_.value("ready", false))
        cyui::textColoured(cyui::palette().warning, "%s", tr("Select the matching Unreal engine directory."));
    if (!unreal_.at("compilationDatabase").value("available", false))
        ImGui::TextWrapped("%s", tr("Generate the code index for Unreal include paths, macros and symbol "
                                    "navigation. The engine SDK and generated headers are required."));
    cyui::textDim("%s: %zu  |  %s: %zu  |  %s: %zu", tr("Targets"), unreal_.at("targets").size(),
                  tr("Modules"), unreal_.at("modules").size(), tr("Plugins"), unreal_.at("plugins").size());
    ImGui::BeginChild("##unreal-rules");
    for (const auto *section : {"targets", "modules", "plugins"})
        for (const auto &item : unreal_.at(section)) {
            const auto path = item.at("path").get<std::string>();
            if (fileSelectable(path, path, active_ == path, theme_))
                open(path);
        }
    ImGui::EndChild();
    ImGui::End();
}
void Application::cmakePanel() {
    if (!beginPanel("CMake", "cmake"))
        return;
    if (projectActive_) { if (cyui::button(tr("Project tasks"))) showPanel("Project tasks","projectTasks"); ImGui::End(); return; }
    if (msbuildActive_) { cyui::textDim("%s",tr("This project uses MSBuild.")); if (cyui::button(tr("MSBuild settings"))) showPanel("MSBuild","msbuild"); ImGui::End(); return; }
    if (unreal_.value("available", false)) {
        cyui::textDim("%s", tr("This project uses UnrealBuildTool."));
        if (cyui::button(tr("Unreal Engine")))
            showPanel("Unreal Engine", "unreal");
        ImGui::End();
        return;
    }
    if (cyui::button(tr("Configure")))
        task("build.configure");
    ImGui::SameLine();
    if (cyui::button(tr("Build project")))
        task("build.run");
    ImGui::SameLine();
    if (cyui::ghostButton(tr("Detect toolchains")))
        refreshBuild(true);
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::refresh, tr("Refresh CMake project")))
        refreshBuild();
    ImGui::SameLine();
    if (buildFuture_.valid() || detectionFuture_.valid())
        cyui::spinner("##cmake-pending");
    cyui::divider(4);
    if (ImGui::BeginTable("##cmake-layout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("##settings", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableSetupColumn("##model", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableNextColumn();
        {
            cyui::ScopedDisabled disabled(taskFuture_.valid());
            if (cyui::beginProperties("##cmake-settings", cyui::dp(120))) {
                cyui::property(tr("CMake source directory"));
                buildEdited_ |= ImGui::InputText("##cmake-source", &buildSource_);
                cyui::property(tr("Build directory"));
                buildEdited_ |= ImGui::InputText("##cmake-build", &buildDirectory_);
                cyui::property(tr("Toolchain"));
                std::string selected = tr("Automatic");
                for (const auto &entry : toolchains_.value("toolchains", Json::array()))
                    if (entry.at("id") == buildToolchain_)
                        selected = entry.at("name");
                if (buildToolchain_ != "automatic" && selected == tr("Automatic"))
                    selected = buildToolchain_;
                if (ImGui::BeginCombo("##toolchain", selected.c_str())) {
                    if (ImGui::Selectable(tr("Automatic"), buildToolchain_ == "automatic")) {
                        buildToolchain_ = "automatic";
                        buildEdited_ = true;
                    }
                    for (const auto &entry : toolchains_.value("toolchains", Json::array())) {
                        if (ImGui::Selectable(entry.at("name").get_ref<const std::string &>().c_str(),
                                              entry.at("id") == buildToolchain_)) {
                            buildToolchain_ = entry.at("id");
                            buildEdited_ = true;
                        }
                        cyui::tooltip(entry.at("cxxCompiler").get_ref<const std::string &>().c_str());
                    }
                    ImGui::EndCombo();
                }
                cyui::property(tr("Generator"));
                if (ImGui::BeginCombo("##generator", buildGenerator_ == "automatic"
                                                         ? tr("Automatic")
                                                         : buildGenerator_.c_str())) {
                    if (ImGui::Selectable(tr("Automatic"), buildGenerator_ == "automatic")) {
                        buildGenerator_ = "automatic";
                        buildEdited_ = true;
                    }
                    for (const auto &entry : toolchains_.value("generators", Json::array())) {
                        const auto name = entry.at("name").get<std::string>();
                        if (ImGui::Selectable(name.c_str(), name == buildGenerator_)) {
                            buildGenerator_ = name;
                            buildEdited_ = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                cyui::property(tr("Configuration"));
                if (ImGui::BeginCombo("##configuration", configuration_.c_str())) {
                    for (const auto *config : {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"})
                        if (ImGui::Selectable(config, configuration_ == config)) {
                            configuration_ = config;
                            buildEdited_ = true;
                        }
                    ImGui::EndCombo();
                }
                cyui::property(tr("Target"));
                if (ImGui::BeginCombo("##target",
                                      buildTarget_.empty() ? tr("All targets") : buildTarget_.c_str())) {
                    if (ImGui::Selectable(tr("All targets"), buildTarget_.empty())) {
                        buildTarget_.clear();
                        buildEdited_ = true;
                    }
                    for (const auto &entry : buildState_.value("targets", Json::array())) {
                        const auto name = entry.at("name").get<std::string>();
                        if (ImGui::Selectable(name.c_str(), name == buildTarget_)) {
                            buildTarget_ = name;
                            buildEdited_ = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                cyui::endProperties();
            }
        }
        ImGui::TableNextColumn();
        const auto database = buildState_.value("compilationDatabase", Json::object());
        cyui::badge(database.value("available", false) ? tr("Compilation database ready")
                                                       : tr("Configure for code analysis"),
                    database.value("available", false) ? cyui::Tone::success : cyui::Tone::neutral);
        if (database.value("available", false)) {
            cyui::textDim("%zu %s / %s", database.value("entries", size_t(0)), tr("translation units"),
                          database.value("origin", "").c_str());
            ImGui::TextWrapped("%s", database.value("path", "").c_str());
        }
        auto generator = buildState_.value("generator", "");
        if (!generator.empty())
            cyui::textDim("%s", generator.c_str());
        for (const auto &compiler : buildState_.value("toolchains", Json::array())) {
            const auto data = compiler.value("compiler", Json::object());
            cyui::textDim("%s: %s %s", compiler.value("language", "").c_str(), data.value("id", "").c_str(),
                          data.value("version", "").c_str());
        }
        for (const auto &warning : toolchains_.value("warnings", Json::array()))
            ImGui::TextWrapped("%s", warning.get_ref<const std::string &>().c_str());
        if (buildEdited_)
            cyui::textDim("%s", tr("Settings apply on configure."));
        ImGui::EndTable();
    }
    ImGui::End();
}
void Application::problemsPanel() {
    if (!beginPanel("Problems", "problems"))
        return;
    const auto servers = languageState_.value("servers",Json::object());
    for (const auto &[key,server] : servers.items()) {
        ImGui::PushID(key.c_str());
        const auto label = server.value("name","") + " : " + tr(server.value("ready",false) ? "Connected" : server.value("running",false) ? "Starting..." : server.value("enabled",false) ? "Idle" : "Disabled");
        cyui::badge(label.c_str(),server.value("ready",false) ? cyui::Tone::success : cyui::Tone::neutral);
        ImGui::SameLine();
        { cyui::ScopedDisabled disabled(smoke_ || !server.value("available",false));
          if (cyui::button(tr(server.value("enabled",false) ? "Restart language server" : "Enable"))) {
              languageDecisions_.insert(key);
              try { engine_->dispatch("language.configure",{{"language",key}}); } catch (const std::exception &e) { error(e.what()); }
          }
        }
        if (server.value("enabled",false)) {
            ImGui::SameLine();
            if (cyui::button(tr("Disable"))) { languageDecisions_.insert(key); engine_->dispatch("language.configure",{{"language",key},{"enabled",false}}); }
        } else if (!server.value("available",false)) { ImGui::SameLine(); ImGui::TextDisabled("%s",tr("Language server is not installed")); }
        ImGui::PopID();
    }
    problemRows();
    auto message = languageState_.value("error", std::string{});
    if (!message.empty())
        ImGui::TextWrapped("%s", message.c_str());
    ImGui::End();
}
void Application::navigate(bool references) {
    if (navigationFuture_.valid())
        return;
    for (const auto &tab : tabs_) {
        if (tab->path != active_ || !LanguageService::supports(tab->path))
            continue;
        navigationPosition_ = tab->editor.GetMainCursorPosition();
        navigationPath_ = tab->path;
        navigationReferences_ = references;
        const auto prefix = tab->editor.GetSectionText({navigationPosition_.line, 0}, navigationPosition_);
        Json parameters = {
            {"path", tab->path},
            {"includeExternal", true},
            {"expectedRevision", tab->revision},
            {"position",
             {{"line", navigationPosition_.line}, {"character", LanguageClient::utf16Length(prefix)}}}};
        auto engine = engine_;
        navigationFuture_ = std::async(std::launch::async, [engine, parameters, references] {
            return engine->dispatch(references ? "language.references" : "language.definition", parameters);
        });
        showPanel("References", "references");
        return;
    }
    cyui::notify(tr("Open a C/C++ document to navigate symbols."), cyui::Tone::neutral);
}
void Application::referencesPanel() {
    if (!beginPanel("References", "references"))
        return;
    cyui::textDim("%s", navigationReferences_ ? tr("Find references") : tr("Go to definition"));
    if (navigationFuture_.valid()) {
        ImGui::SameLine();
        cyui::spinner("##navigation");
    }
    if (!navigation_.contains("locations")) {
        cyui::emptyState(tr("Symbol navigation"), tr("F12: definition. Shift+F12: references."),
                         cyui::icon::search);
    } else {
        const auto &locations = navigation_["locations"];
        cyui::textDim("%zu %s", locations.size(), tr("matches"));
        if (navigation_.value("outsideWorkspace", 0) > 0)
            ImGui::TextWrapped("%s", tr("Some locations are outside this workspace."));
        if (navigation_.value("truncated", false))
            cyui::textDim("%s", tr("Results limited"));
        cyui::divider();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(locations.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto &location = locations[static_cast<size_t>(i)];
                const auto &start = location["range"]["start"];
                const auto line = start["line"].get<size_t>() + 1,
                           column = start["character"].get<size_t>() + 1;
                const auto text = location.value("displayPath", location["path"].get<std::string>()) + ":" + std::to_string(line) + ":" +
                                  std::to_string(column);
                ImGui::PushID(i);
                if (ImGui::Selectable(text.c_str()))
                    open(location["path"], line, column, true);
                ImGui::PopID();
            }
    }
    ImGui::End();
}
void Application::extensionsPanel() {
    if (!beginPanel("Extensions", "extensions"))
        return;
    cyui::textDim("%s 1", tr("Plugin ABI"));
    for (const auto &plugin : pluginInfo_.value("plugins", Json::array())) {
        cyui::sectionTitle(plugin.value("name", plugin.value("id", std::string("Plugin"))).c_str());
        cyui::badge(plugin.value("state", std::string{}).c_str(),
                    plugin.value("state", std::string{}) == "loaded" ? cyui::Tone::success
                                                                     : cyui::Tone::error);
        ImGui::SameLine();
        cyui::textDim("%s", plugin.value("version", std::string{}).c_str());
        if (plugin.contains("error"))
            ImGui::TextWrapped("%s", plugin["error"].get<std::string>().c_str());
    }
    for (const auto &command : pluginInfo_.value("commands", Json::array())) {
        if (cyui::button(command["title"].get<std::string>().c_str())) {
            std::string text;
            for (auto &tab : tabs_)
                if (tab->path == active_)
                    text = tab->editor.GetText();
            task("plugins.invoke", {{"command", command["id"]}, {"input", {{"text", text}}}});
        }
    }
    ImGui::End();
}
Json Application::mcpConfiguration() const {
#ifdef _WIN32
    auto python = root_ / "runtime/python/Scripts/python.exe";
#else
    auto python = root_ / "runtime/python/bin/python";
#endif
    return {{"mcpServers",
             {{"cycode",
               {{"command", pathUtf8(python)},
                {"args",
                 Json::array({pathUtf8(root_ / "app/tool.py"), "--bridge-config", pathUtf8(coreDescriptor_),
                              "--access", mcpWrite_ ? "edit" : "read", "mcp"})}}}}}};
}
void Application::mcpPanel() {
    if (!beginPanel("MCP", "mcp"))
        return;
    cyui::badge(tr("Native bridge ready"), cyui::Tone::success);
    ImGui::SameLine();
    cyui::textDim("CyToolsCore 0.10.1");
    ImGui::SameLine();
    cyui::badge(
        tr(std::filesystem::exists(coreDescriptor_) ? "Shared Core ready" : "Core starting or unavailable"),
        std::filesystem::exists(coreDescriptor_) ? cyui::Tone::success : cyui::Tone::neutral);
    ImGui::TextWrapped("%s", tr("Connect an assistant to inspect this workspace and its live documents."));
    ImGui::Checkbox(tr("Enable editing, builds and plugin commands"), &mcpWrite_);
    if (cyui::button(tr("Copy MCP configuration")))
        ImGui::SetClipboardText(mcpConfiguration().dump(2).c_str());
    ImGui::SameLine();
    if (cyui::button(tr("Save MCP configuration"))) {
        auto path = root_ / "data/mcp-config.json";
        std::ofstream out(path);
        out << mcpConfiguration().dump(2);
        cyui::notify(pathUtf8(path).c_str(), cyui::Tone::success);
    }
    cyui::divider();
    cyui::textDim("%s", tr("Available services"));
    ImGui::TextWrapped("%s", tr("Workspace, live documents, language services, search, Git, CMake/MSBuild/Unreal, terminal, debugger and plugins. "
                                "MCP sessions use CyToolsCore jobs and permissions."));
    ImGui::End();
}
void Application::optionsPanel() {
    if (!options_)
        return;
    ImGui::SetNextWindowSize(cyui::dp(620, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(label("Options", "options").c_str(), &options_)) {
        cancelPropertyDrafts(optionsInspector_); ImGui::End(); return;
    }
    optionsProperties();
    keymapOptions();
    if (cyui::button(tr("Save preferences")))
        saveSettings();
    ImGui::SameLine();
    if (cyui::button(tr("Save configuration"))) {
        auto path = root_ / "data/settings/configuration.json";
        saveSettings();
        std::ofstream out(path);
        out << settings_.dump(2);
    }
    ImGui::SameLine();
    if (cyui::button(tr("Load configuration"))) {
        try {
            std::ifstream in(root_ / "data/settings/configuration.json");
            auto value = Json::parse(in);
            cancelPropertyDrafts(optionsInspector_); ++optionsRevision_;
            theme_ = value.at("theme");
            locale_ = value.at("language");
            textScale_ = std::clamp(value.at("textScale").get<float>() * (value.value("schemaVersion", 1) < 3 ? 1.15f : 1.0f), .8f, 2.0f);
            auto profile = value.value("keymap", std::string("rider"));
            auto overrides = value.value("keymapOverrides", Json::object());
            keymap_.load(profile, overrides.value(profile, Json::object())); keymapProfile_ = profile; keymapOverrides_ = overrides;
            clangd_ = value.at("clangd");
            applyEditorTheme(theme_);
            ImGui::GetStyle().FontScaleMain = textScale_;
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    cyui::divider();
    cyui::sectionTitle(tr("Licenses"));
    ImGui::TextWrapped("CyUI (MIT): https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE");
    ImGui::TextWrapped("%s", tr("CyCode: GPL-3.0-only. Copyright (c) 2026 Cyberalien. "
                                "This program comes without any warranty. See LICENSE and COPYRIGHT."));
    ImGui::TextWrapped("%s", tr("CyUI, Dear ImGui, TextEditor, CyToolsCore and nlohmann/json: MIT. SDL3: zlib."));
    ImGui::TextWrapped("%s", tr("libvterm and CodeLLDB: MIT. LLVM: Apache 2.0 with LLVM exceptions. Embedded Python: PSF."));
    ImGui::TextWrapped("%s", "NetCoreDbg: MIT (Samsung). app/licenses/NetCoreDbg.txt");
    ImGui::TextWrapped("%s", tr("csharp-ls: MIT. Private .NET SDK: included Microsoft terms and component notices."));
    if (cyui::button(tr("Open third-party notices")))
        open("app/THIRD_PARTY_NOTICES.md");
    ImGui::End();
}
void Application::palette() {
    if (paletteOpen_ && paletteMode_ == "actions") {
        actionPaletteOpen_ = true; paletteOpen_ = false;
    }
    std::vector<cyui::Command> actions;
    const auto loadActions = [&] {
        if (!actions.empty()) return;
        for (const auto &command : commands()) actions.push_back({uiId("command/" + command.id),
            command.title, keymap_.hint(command.id), command.category + " " + command.id, commandEnabled(command.id)});
    };
    if (actionPaletteOpen_) loadActions();
    const auto requested = cyui::commandPalette("Find action...", actionPaletteOpen_, actionPaletteState_, actions, cyui::dp(850, 570));
    if (requested) for (const auto &command : commands()) if (uiId("command/" + command.id) == requested) { executeCommand(command.id); break; }
    quickOpenVisible_ = actionPaletteOpen_;
    if (actionPaletteOpen_) { paletteFilter_ = actionPaletteState_.query; return; }
    if (paletteOpen_) {
        ImGui::OpenPopup("##quickopen"); paletteOpen_ = false; paletteFilter_.clear(); paletteSelection_ = 0;
        symbolWantedKey_.clear(); symbolSentKey_.clear(); symbolResults_ = Json::object(); symbolError_.clear(); symbolAttempts_ = 0;
    }
    ImGui::SetNextWindowSize(cyui::dp(850, 570), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(.5f, .5f));
    if (!ImGui::BeginPopup("##quickopen")) return;
    quickOpenVisible_ = true;
    const char *heading = paletteMode_ == "actions" ? "Find action..." : paletteMode_ == "everywhere" ? "Search Everywhere" : paletteMode_ == "recent" ? "Recent files" : paletteMode_ == "types" ? "Go to type..." : paletteMode_ == "symbols" ? "Go to symbol..." : paletteMode_ == "members" ? "File structure" : "Go to file...";
    cyui::heading(tr(heading), 2);
    for (const auto &[id, text] : std::initializer_list<std::pair<const char *, const char *>>{{"everywhere", "All"}, {"files", "Files"}, {"types", "Types"}, {"symbols", "Symbols"}, {"members", "Members"}, {"actions", "Actions"}, {"recent", "Recent files"}}) {
        bool selected = paletteMode_ == id;
        ImGui::BeginDisabled(std::string(id) == "members" && !LanguageService::supports(active_));
        if (cyui::toggleButton(tr(text), &selected)) { paletteMode_ = id; paletteSelection_ = 0; }
        ImGui::EndDisabled(); ImGui::SameLine();
    }
    ImGui::NewLine();
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##quickquery", tr("Type a name; > actions, @ members, # symbols..."), &paletteFilter_)) paletteSelection_ = 0;
    auto query = paletteFilter_;
    auto mode = paletteMode_;
    if (query.starts_with(">") || query.starts_with("@") || query.starts_with("#")) {
        mode = query[0] == '>' ? "actions" : query[0] == '@' ? "members" : "symbols";
        query.erase(0,1); while (query.starts_with(" ")) query.erase(0,1);
    }
    const bool wantSymbols = !smoke_ && (mode == "types" || mode == "symbols" || mode == "members" || (mode == "everywhere" && !query.empty()));
    if (wantSymbols) {
        auto backend=languageBackend(active_); if (backend.empty()) backend="cpp";
        Json parameters = {{"query", query}, {"language",backend}, {"typesOnly", mode == "types"}, {"includeExternal", true}, {"limit", 500}};
        std::string method = "language.symbols";
        if (mode == "members") {
            method = "language.documentSymbols";
            parameters = {{"path", active_}, {"includeExternal", true}, {"limit", 1000}};
            for (const auto &tab : tabs_) if (tab->path == active_) parameters["expectedRevision"] = tab->revision;
        }
        auto key = method + parameters.dump();
        if (key != symbolWantedKey_) { symbolWantedKey_ = key; symbolWantedParams_ = parameters; symbolWantedMethod_ = method; symbolResults_ = Json::object(); symbolError_.clear(); symbolChanged_ = ImGui::GetTime(); symbolAttempts_ = 0; }
        if (!symbolFuture_.valid() && symbolSentKey_ != symbolWantedKey_ && ImGui::GetTime()-symbolChanged_ > .12 && unrealAnalysisState_ != "preparing" && parameters.contains(mode == "members" ? "expectedRevision" : "query")) {
            auto engine = engine_; symbolSentKey_ = symbolWantedKey_; ++symbolAttempts_;
            symbolFuture_ = std::async(std::launch::async, [engine, parameters, method] { return engine->dispatch(method, parameters); });
        }
    }
    struct Item { std::string id, title; int kind, score; Json location; };
    std::vector<Item> items;
    if (mode == "actions" || mode == "everywhere") {
        loadActions();
        const auto ranked = cyui::rankCommands(actions, query);
        for (size_t commandIndex = 0; commandIndex < commands().size(); ++commandIndex) {
            const auto &command = commands()[commandIndex];
            const auto name = std::string(tr(command.title.c_str()));
            auto title = std::string(tr(command.category.c_str())) + ": " + name;
            int score = std::max({fuzzyScore(query, name),fuzzyScore(query, title),fuzzyScore(query, command.id)});
            if (!query.empty() && (lower(query) == lower(name) || lower(query) == lower(command.id))) score += 10000;
            const auto common = std::find(ranked.begin(), ranked.end(), commandIndex);
            if (common != ranked.end()) score = std::max(score, 1000 - int(common - ranked.begin()));
            if (score >= 0) items.push_back({command.id, title, 1, score, nullptr});
        }
    }
    if (mode == "files" || mode == "recent" || mode == "everywhere") {
        const auto &paths = mode == "recent" ? recentFiles_ : files_;
        for (const auto &path : paths) {
            auto score = fuzzyScore(query, path);
            if (score < 0) continue;
            auto recent = std::find(recentFiles_.begin(), recentFiles_.end(), path);
            if (recent != recentFiles_.end()) score += static_cast<int>(100 - (recent - recentFiles_.begin()));
            items.push_back({path, path, 0, score, nullptr});
        }
    }
    if (wantSymbols && symbolResults_.contains("symbols")) {
        for (const auto &symbol : symbolResults_.at("symbols")) {
            const auto name = symbol.at("name").get<std::string>(), container = symbol.value("container", "");
            auto score = std::max(fuzzyScore(query, name), fuzzyScore(query, container+"::"+name));
            if (score < 0) continue;
            auto display = symbol.value("displayPath", symbol.at("path").get<std::string>());
            auto title = name + (container.empty() ? "" : "  ("+container+")") + "   " + pathUtf8(utf8Path(display).filename()) + ":" + std::to_string(symbol.at("range").at("start").at("line").get<size_t>()+1);
            items.push_back({symbol.at("path"), title, 2, score+30, symbol});
        }
    }
    auto compare = [](const auto &a, const auto &b) { return a.score == b.score ? a.title < b.title : a.score > b.score; };
    if (items.size() > 100) { std::partial_sort(items.begin(), items.begin()+100, items.end(), compare); items.resize(100); }
    else std::sort(items.begin(), items.end(), compare);
    paletteSelection_ = std::clamp(paletteSelection_, 0, std::max(0, static_cast<int>(items.size())-1));
    bool moved = false;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !items.empty()) { paletteSelection_ = (paletteSelection_+1) % items.size(); moved = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !items.empty()) { paletteSelection_ = (paletteSelection_+items.size()-1) % items.size(); moved = true; }
    const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter);
    if (wantSymbols && (symbolFuture_.valid() || symbolSentKey_ != symbolWantedKey_)) { cyui::spinner("##symbols"); ImGui::SameLine(); cyui::textDim("%s", tr("Searching symbols...")); }
    if (wantSymbols && !symbolError_.empty()) cyui::textColoured(cyui::palette().warning, "%s", symbolError_.c_str());
    if (wantSymbols && symbolResults_.value("truncated", false)) cyui::textDim("%s", tr("Results limited. Refine the symbol name."));
    std::string chosen; int kind = 0; Json destination;
    ImGui::BeginChild("##quickresults");
    for (size_t i = 0; i < items.size(); ++i) {
        const auto &item = items[i]; bool clicked = false;
        ImGui::PushID(static_cast<int>(i));
        const bool enabled = item.kind != 1 || commandEnabled(item.id);
        ImGui::BeginDisabled(!enabled);
        if (item.kind == 1) {
            clicked = ImGui::Selectable(item.title.c_str(), paletteSelection_ == static_cast<int>(i));
            const auto shortcut = keymap_.hint(item.id);
            if (!shortcut.empty()) { ImGui::SameLine(); cyui::textFaint("  %s", shortcut.c_str()); }
        } else if (item.kind == 2) clicked = ImGui::Selectable(item.title.c_str(), paletteSelection_ == static_cast<int>(i));
        else clicked = fileSelectable(item.id, item.title, paletteSelection_ == static_cast<int>(i), theme_);
        if (moved && paletteSelection_ == static_cast<int>(i)) ImGui::SetScrollHereY();
        ImGui::EndDisabled();
        if (enabled && (clicked || (enter && paletteSelection_ == static_cast<int>(i)))) { chosen = item.id; kind = item.kind; destination = item.location; }
        ImGui::PopID();
    }
    if (items.empty()) cyui::textDim("%s", tr("No matching results"));
    ImGui::EndChild();
    if (!chosen.empty()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    if (!chosen.empty()) {
        if (kind == 1) executeCommand(chosen);
        else if (kind == 2) { const auto p = destination.at("range").at("start"); open(chosen, p.at("line").get<size_t>()+1, p.at("character").get<size_t>()+1, true); }
        else open(chosen);
    }
}
void Application::windowTest() {
    ++windowTestFrame_;
    auto build = ImGui::FindWindowByName(label("Build", "build").c_str());
    auto document = tabs_.empty() ? nullptr : ImGui::FindWindowByName(documentTitle(tabs_[0]->path).c_str());
    if (windowTestFrame_ == 20 && build) {
        windowTest_.push_back({{"stage", "initial"}, {"buildDock", build->DockId}});
        ImGui::SetWindowDock(build, 0, ImGuiCond_Always);
        auto pos = ImGui::GetMainViewport()->Pos;
        ImGui::SetWindowPos(build, ImVec2(pos.x + cyui::dp(100), pos.y + cyui::dp(100)));
        ImGui::SetWindowSize(build, cyui::dp(650, 350));
    }
    if (windowTestFrame_ == 25 && document) {
        ImGui::SetWindowDock(document, 0, ImGuiCond_Always);
        auto pos = ImGui::GetMainViewport()->Pos;
        ImGui::SetWindowPos(document, ImVec2(pos.x + cyui::dp(250), pos.y + cyui::dp(150)));
        ImGui::SetWindowSize(document, cyui::dp(850, 550));
    }
    if (windowTestFrame_ == 55 && build && document) {
        auto viewport = build->Viewport;
        auto native = SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(viewport->PlatformHandle)));
        auto editorNative = SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(document->Viewport->PlatformHandle)));
        int count = 0;
        auto windows = SDL_GetWindows(&count);
        SDL_free(windows);
        windowTest_.push_back(
            {{"stage", "detached"},
             {"nativeWindows", count},
             {"panelNative", native && native != window_},
             {"editorNative", editorNative && editorNative != window_ && editorNative != native}});
        if (native) {
            SDL_SetWindowPosition(native, int(ImGui::GetMainViewport()->Pos.x) + 150,
                                  int(ImGui::GetMainViewport()->Pos.y) + 120);
            SDL_SetWindowSize(native, 700, 380);
        }
    }
    if (windowTestFrame_ == 75 && document)
        ImGui::SetWindowDock(document, editorDock_, ImGuiCond_Always);
    if (windowTestFrame_ == 90 && build) {
        auto native = SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(build->Viewport->PlatformHandle)));
        if (native && native != window_) {
            SDL_Event event{};
            event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            event.window.windowID = SDL_GetWindowID(native);
            SDL_PushEvent(&event);
        }
    }
    if (windowTestFrame_ == 115) {
        windowTest_.push_back({{"stage", "closed"},
                               {"panelHidden", !panels_["build"]},
                               {"editorDocked", document && document->DockId == editorDock_}});
        showPanel("Build", "build");
        if (build && !windowTest_.empty())
            ImGui::SetWindowDock(build, windowTest_[0].value("buildDock", ImGuiID(0)), ImGuiCond_Always);
    }
    if (windowTestFrame_ == 140) {
        int count = 0;
        auto windows = SDL_GetWindows(&count);
        SDL_free(windows);
        windowTest_.push_back({{"stage", "restored"},
                               {"nativeWindows", count},
                               {"panelVisible", panels_["build"]},
                               {"panelDocked", build && build->DockId != 0}});
    }
}
void Application::frame() {
    outputLog_.synchronize(output_);
    cyui::setLocale(locale_.c_str());
    if (!options_) cancelPropertyDrafts(optionsInspector_);
    update();
    shortcuts();
    if (testWindows_)
        windowTest();
    menu();
    topBar();
    statusBar();
    rail();
    auto dock = cyui::dockSpace("##workspace");
    bool rebuiltLayout = false;
    if (!layoutReady_ || resetLayout_) {
        layoutReady_ = true;
        if (resetLayout_ || smoke_ || !ImGui::DockBuilderGetNode(dock) ||
            ImGui::DockBuilderGetNode(dock)->IsLeafNode()) {
            layout(dock);
            rebuiltLayout = true;
        }
        resetLayout_ = false;
    }
    projectPanel();
    searchPanel();
    editorPanel();
    outputPanel();
    unrealPanel();
    cmakePanel();
    msbuildPanel();
    projectTasksPanel();
    gitPanel();
    problemsPanel();
    referencesPanel();
    informationPanel();
    codeActionsPanel();
    refactorPanel();
    terminalPanel();
    debugPanel();
    extensionsPanel();
    mcpPanel();
    if (rebuiltLayout) {
        panels_["build"] = true;
        focusPanel_.clear();
        if (!active_.empty()) focusTab_ = active_;
        else ImGui::SetWindowFocus("##editors");
    }
    if (focusCmake_) {
        showPanel("CMake", "cmake");
        focusCmake_ = false;
    }
    if (focusProjectTasks_) { showPanel("Project tasks","projectTasks"); focusProjectTasks_ = false; }
    if (focusUnreal_) {
        showPanel("Unreal Engine", "unreal");
        focusUnreal_ = false;
    }
    optionsPanel();
    palette();
    goToLine();
    renamePopup();
    if (closeAll_ && pendingClose_.empty()) { if (tabs_.empty()) closeAll_ = false; else closeTab(tabs_.back()->path); }
    if (newFile_) {
        ImGui::OpenPopup(tr("New file..."));
        newFile_ = false;
    }
    if (ImGui::BeginPopupModal(tr("New file..."), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(cyui::dp(420));
        ImGui::InputTextWithHint("##newpath", tr("Workspace-relative path"), &newFilePath_);
        if (cyui::primaryButton(tr("Create"))) {
            try {
                engine_->dispatch("documents.create", {{"path", newFilePath_}});
                open(newFilePath_);
                refreshFiles();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception &e) {
                error(e.what());
            }
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!pendingClose_.empty()) {
        ImGui::OpenPopup(tr("Unsaved changes"));
        if (ImGui::BeginPopupModal(tr("Unsaved changes"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(pendingClose_.c_str());
            if (cyui::primaryButton(tr("Save"))) {
                for (auto &tab : tabs_)
                    if (tab->path == pendingClose_)
                        save(tab);
                bool dirty = false;
                for (auto &tab : tabs_)
                    if (tab->path == pendingClose_)
                        dirty = tab->dirty;
                if (!dirty) {
                    auto close = pendingClose_;
                    pendingClose_.clear();
                    closeTab(close);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (cyui::dangerButton(tr("Discard"))) {
                auto close = pendingClose_;
                pendingClose_.clear();
                closeTab(close, true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (cyui::button(tr("Cancel"))) {
                pendingClose_.clear();
                closeAll_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if (quitRequested_) {
        bool dirty = std::any_of(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->dirty; });
        if (!dirty)
            done_ = true;
        else {
            ImGui::OpenPopup(tr("Close workspace?"));
            if (ImGui::BeginPopupModal(tr("Close workspace?"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted(tr("Some documents have unsaved changes."));
                if (cyui::primaryButton(tr("Save all"))) {
                    saveAll();
                    done_ =
                        std::none_of(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->dirty; });
                }
                ImGui::SameLine();
                if (cyui::dangerButton(tr("Discard")))
                    done_ = true;
                ImGui::SameLine();
                if (cyui::button(tr("Cancel"))) {
                    quitRequested_ = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }
    cyui::renderNotifications();
}
} // namespace cycode
