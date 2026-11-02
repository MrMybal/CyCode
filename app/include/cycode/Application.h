#pragma once
#include "cycode/Bridge.h"
#include "cycode/Commands.h"
#include "cycode/LanguageClient.h"
#include "cycode/Syntax.h"
#include "cycode/EditorSnippet.h"
#include "cycode/ModuleModels.h"
#include <SDL3/SDL.h>
#include <TextEditor.h>
#include <future>
#include <memory>
#include <set>
namespace cycode {
class ImageView;
class ImageLoader;
class ImageTextures;
struct LaunchOptions {
    std::filesystem::path installation, workspace;
    std::string theme, language, openFile, keymap, terminalProfile;
    size_t openLine = 0, openColumn = 0;
    bool smoke = false;
    bool showOptions = false;
    bool showCmake = false;
    bool showProjectTasks = false;
    bool showUnreal = false;
    bool testSession = false;
    bool testWindows = false;
    bool testHidden = false, testHighlights = false;
    Json testNavigation;
    Json testCompletion;
};
class Application {
  public:
    Application(LaunchOptions, SDL_Window *);
    ~Application();
    void frame();
    void requestQuit() {
        quitRequested_ = true;
    }
    bool shouldQuit() const {
        return done_;
    }
    bool busy() const;
    bool readyToCapture() const {
        return !filesFuture_.valid();
    }
    bool readyToFinishTests() const {
        if (testHighlights_) for (const auto &tab : tabs_) if (tab->path==active_ &&
            (tab->highlighting.valid() || tab->highlightRevision!=tab->revision || !tab->highlightCount)) return false;
        if (testNavigation_.is_object() || navigationFuture_.valid()) return false;
        if (!testCompletion_.is_object()) return true;
        if (testCompletionPhase_ < 3) return false;
        if (testCompletion_.value("acceptFirst", false) && testCompletionPhase_ < 5) return false;
        for (const auto &tab : tabs_) if (tab->path == active_) return tab->completionGeneration > 0 && !tab->completion.valid() && !tab->completionAccept.valid() && !tab->queuedCompletion.is_object();
        return false;
    }
    Json testState() const;
    bool testCommandReady(const std::string &id) const { return commandEnabled(id); }
    void open(const std::string &, size_t line = 0, size_t column = 0, bool utf16 = false, bool activate = true);
    void openProject(const std::filesystem::path &);
    // Explicit user-selected image: grants read access to this canonical file
    // only, without extending workspace document permissions.
    void openImageFile(const std::filesystem::path &);
    const std::string &theme() const {
        return theme_;
    }

  private:
    friend struct ApplicationModuleTests;
    friend struct ApplicationImageTests;
    struct Tab;
    bool openImage(const std::string &, bool activate);
    void reloadImage(const std::shared_ptr<Tab> &);
    void imagePanel(const std::shared_ptr<Tab> &);
    void updateImages();
    void chooseImage();
    std::shared_ptr<ImageLoader> imageLoader_;
    std::shared_ptr<ImageTextures> imageTextures_;
    void optionsProperties();
    void problemRows();
    void viewFilter(const char *, cyui::TextFilter &);
    std::map<ImGuiID, cyui::TextFilter *> viewFilters_;
    std::map<std::string, cyui::TextFilter> menuFilters_;
    bool testHighlights_=false;
    struct Tab {
        std::string path, displayPath;
        std::shared_ptr<ImageView> image;
        std::filesystem::path imageSource;
        cyui::CodeEditor codeEditor;
        TextEditor &editor = codeEditor.native();
        EditorSnippet snippet;
        Syntax syntax;
        std::uint64_t revision = 0;
        bool dirty = false;
        bool readOnly = false;
        size_t completionGeneration = 0;
        size_t runningCompletionGeneration = 0;
        std::uint64_t completionRevision = 0;
        std::future<Json> completion;
        std::future<Json> completionAccept;
        std::string completionSession;
        std::vector<size_t> completionItems;
        uint64_t acceptedRevision=0;
        size_t acceptedGeneration=0;
        TextEditor::DocPos acceptedPosition;
        Json queuedCompletion;
        TextEditor::DocPos completionPosition;
        TextEditor::DocPos queuedCompletionStart, queuedCompletionEnd, runningCompletionStart;
        std::vector<std::string> lastSuggestions;
        Json completionTrace;
        std::future<Json> highlighting;
        uint64_t highlightRevision=0, highlightGeneration=0, highlightObservedGeneration=0;
        double highlightRetryAt=0;
        size_t highlightCount=0;
        std::string highlightError;
        bool highlightTruncated=false;
        bool visible=false;
    };
    struct Tree {
        std::map<std::string, Tree> folders;
        std::vector<std::string> files;
    };
    struct Dialog {
        std::mutex mutex;
        std::string path, error;
        bool ready = false, newWindow = false, image = false;
    };
    const char *tr(const char *) const;
    std::string label(const char *, const char *) const;
    void loadSettings();
    void saveSettings();
    void setWorkspace(const std::filesystem::path &);
    void menu();
    void topBar();
    void statusBar();
    void rail();
    void layout(ImGuiID);
    void projectPanel();
    void solutionTree(const std::string &parent, int depth = 0);
    bool beginPanel(const char *text, const char *id);
    void showPanel(const char *text, const char *id);
    void windowTest();
    void tree(Tree &, const std::string &);
    void editorPanel();
    void searchPanel();
    void gitPanel();
    void updateVcs();
    void vcsTask(const std::string &, Json = Json::object());
    void outputPanel();
    void cmakePanel();
    void msbuildPanel();
    void projectTasksPanel();
    void unrealPanel();
    void applyUnreal(const Json &);
    void prepareUnreal();
    void refreshBuild(bool detectTools = false, bool initial = false);
    Json buildParameters() const;
    void applyBuild(const Json &project);
    void problemsPanel();
    void referencesPanel();
    void navigate(bool references);
    void extensionsPanel();
    void optionsPanel();
    void mcpPanel();
    void palette();
    void shortcuts();
    bool commandEnabled(const std::string &) const;
    void executeCommand(const std::string &);
    void keymapOptions();
    void navigationHistory(bool forward);
    void rememberNavigation();
    void goToLine();
    void updateSemantic();
    void requestInformation(bool signature);
    void updateInformation();
    Json informationParameters() const;
    void informationPanel();
    Json codeActionParameters() const;
    void requestCodeActions();
    void updateCodeActions();
    void codeActionsPanel();
    void refactorPanel();
    void terminalPanel();
    void updateTerminal();
    void newTerminal();
    void debugPanel();
    void updateDebug();
    void debugTask(const std::string &, Json = Json::object());
    void toggleBreakpoint(const std::string &, size_t line);
    void openDebugSource(const std::string &, size_t line, size_t column);
    void debugFrame(const Json &);
    void startDebug(bool build = true);
    Json debugParameters() const;
    void renamePopup();
    void beginRename();
    void beginFormat(bool selection);
    void changeTask(const std::string &, Json);
    void selectChangeFile(const std::string &);
    static TextEditor::Palette editorPalette(const std::string &);
    void update();
    void save(const std::shared_ptr<Tab> &);
    void saveAll();
    void task(const std::string &, Json params = Json::object());
    void refreshFiles();
    void refreshGit();
    bool vcsSupports(const std::string &) const;
    void configureEditor(const std::shared_ptr<Tab> &);
    void updateHighlights();
    void changed(const std::shared_ptr<Tab> &);
    void closeTab(const std::string &, bool discard = false);
    void error(const std::string &);
    void chooseFolder();
    void chooseSolution(bool newWindow = false);
    void newWindow(const std::filesystem::path &);
    Json mcpConfiguration() const;
    std::filesystem::path root_;
    std::filesystem::path entry_;
    std::string instance_, layoutIni_;
    Json solution_;
    std::string solutionConfiguration_;
    ImGuiID editorDock_ = 0;
    std::map<std::string, bool> panels_;
    std::string focusPanel_;
    bool testWindows_ = false;
    int windowTestFrame_ = 0;
    Json windowTest_ = Json::array();
    SDL_Window *window_;
    std::shared_ptr<Engine> engine_;
    std::future<Json> unrealAnalysisFuture_;
    std::shared_ptr<std::atomic<bool>> unrealAnalysisCancellation_;
    std::string unrealAnalysisState_, pendingWorkspace_;
    TextEditor::Language unrealLanguage_;
    std::unique_ptr<Bridge> bridge_;
    std::future<ProcessResult> coreService_;
    std::filesystem::path coreDescriptor_;
    Json languageState_ = Json::object(), navigation_ = Json::object();
    std::set<std::string> languageDecisions_;
    std::future<Json> navigationFuture_;
    std::vector<std::future<Json>> retiredCompletions_;
    std::string navigationPath_;
    TextEditor::DocPos navigationPosition_;
    bool navigationReferences_ = false;
    Json testNavigation_;
    Json testCompletion_;
    int testCompletionPhase_ = 0;
    std::vector<std::shared_ptr<Tab>> tabs_;
    std::string active_, focusTab_, pendingClose_, revealPath_;
    std::string newFilePath_ = "new.cpp";
    bool newFile_ = false;
    Json translations_, settings_, git_ = Json::object(), search_ = Json::object(),
                                   pluginInfo_ = Json::object();
    Json buildState_ = Json::object(), toolchains_ = Json::object();
    Json msbuildState_ = Json::object();
    Json projectState_ = Json::object(), projectProfile_ = Json::object();
    bool projectActive_ = false, focusProjectTasks_ = false;
    Json buildDiagnostics_ = Json::array();
    bool msbuildActive_ = false, msbuildRestore_ = false;
    std::string msbuildConfiguration_, msbuildPlatform_, msbuildProject_, msbuildTool_ = "automatic";
    Json unreal_ = Json::object();
    std::string unrealProject_, unrealEngine_, unrealTarget_, unrealPlatform_, unrealConfiguration_;
    bool focusUnreal_ = false;
    std::vector<std::string> files_;
    Tree tree_;
    std::future<Json> filesFuture_, searchFuture_, gitFuture_, taskFuture_;
    std::future<Json> vcsFuture_;
    Json vcsInfo_ = Json::object(), vcsHistory_ = Json::object(), vcsConflict_ = Json::object();
    std::string vcsOperation_, vcsSection_ = "Changes", vcsBranch_, vcsNewBranch_, vcsRevision_ = "HEAD", vcsRemote_ = "origin", vcsRemoteUrl_, vcsStashMessage_, vcsPreviewTitle_;
    int vcsStashIndex_ = 0;
    bool vcsIncludeUntracked_ = false, vcsSetUpstream_ = false;
    cyui::CodeEditor vcsPreviewCode_, vcsBaseCode_, vcsOursCode_, vcsTheirsCode_;
    TextEditor &vcsPreview_ = vcsPreviewCode_.native(), &vcsBase_ = vcsBaseCode_.native(),
               &vcsOurs_ = vcsOursCode_.native(), &vcsTheirs_ = vcsTheirsCode_.native();
    cyui::DiffDocument vcsDiff_;
    cyui::DiffState vcsDiffState_;
    bool vcsIsDiff_ = false;
    std::future<Json> buildFuture_, detectionFuture_;
    std::shared_ptr<std::atomic<bool>> searchCancellation_;
    std::string output_, lastError_, searchQuery_, pendingQuery_, commitMessage_,
        buildSource_ = ".", buildDirectory_ = "build", configuration_ = "Debug", clangd_ = "clangd";
    std::string buildToolchain_ = "automatic", buildGenerator_ = "automatic", buildTarget_;
    bool buildEdited_ = false;
    OutputLog outputLog_;
    ProblemModel problems_;
    Json problemBuild_, problemLanguage_;
    cyui::TextFilter problemsFilter_, optionsFilter_, filesFilter_, shortcutsFilter_;
    cyui::InspectorState optionsInspector_;
    uint64_t optionsRevision_ = 1;
    bool actionPaletteOpen_ = false;
    cyui::CommandState actionPaletteState_;
    bool focusCmake_ = false;
    double lastBuildRefresh_ = 0;
    std::string theme_ = "graphite", locale_ = "en", paletteFilter_;
    std::string provider_ = "git";
    float textScale_ = 1.15f;
    bool options_ = false, paletteOpen_ = false, layoutReady_ = false;
    bool resetLayout_ = false;
    bool focusSearch_ = false, quickOpenVisible_ = false;
    bool shiftChordUsed_ = false;
    double lastShiftTap_ = -1;
    std::string lastShortcut_;
    Keymap keymap_;
    ImGuiID keymapFocus_ = 0;
    std::string keymapProfile_ = "rider", keymapFilter_, keymapEditId_, keymapEditText_;
    Json keymapOverrides_ = Json::object();
    bool editorFocused_ = false, closeAll_ = false, linePopup_ = false, focusKeymap_ = false;
    int lineNumber_ = 1;
    std::string paletteMode_ = "files";
    int paletteSelection_ = 0;
    std::vector<std::string> recentFiles_, closedFiles_, executedCommands_;
    struct HistoryPosition { std::string path; size_t line, character; };
    std::vector<HistoryPosition> historyBack_, historyForward_;
    bool historyMoving_ = false;
    std::future<Json> symbolFuture_, refactorFuture_;
    std::future<Json> informationFuture_;
    std::future<Json> codeActionsFuture_;
    Json codeActionsState_=Json::object();
    std::string codeActionsKey_,codeActionsError_;
    int codeActionsFilter_=0,codeActionSelected_=-1;
    bool codeActionApply_=false;
    Json informationState_ = Json::object(), informationQuery_;
    std::string informationMethod_, informationWanted_, informationSent_, informationError_;
    double informationChanged_ = 0;
    std::map<std::string, InformationDocument> informationDocuments_;
    Json symbolResults_ = Json::object(), symbolWantedParams_, refactorPlan_, refactorPreview_, renameParameters_;
    std::string symbolWantedKey_, symbolSentKey_, symbolWantedMethod_, symbolError_;
    double symbolChanged_ = 0;
    int symbolAttempts_ = 0;
    std::string refactorOperation_, refactorFile_, refactorName_;
    bool renameOpen_ = false;
    cyui::DiffDocument refactorDiff_;
    cyui::DiffState refactorDiffState_;
    bool previewWrap_ = false;
    cyui::CodeEditor beforePreviewCode_, afterPreviewCode_;
    TextEditor &beforePreview_ = beforePreviewCode_.native(), &afterPreview_ = afterPreviewCode_.native();
    std::future<Json> terminalFuture_;
    std::string terminalId_, terminalProfile_ = "default", terminalOperation_;
    bool terminalFocused_ = false, focusTerminal_ = false, terminalScrollBottom_ = true, terminalSelecting_ = false;
    std::pair<int, int> terminalSelectionStart_{-1, 0}, terminalSelectionEnd_{-1, 0};
    std::future<Json> debugFuture_;
    Json debugState_ = Json::object(), debugProfile_ = Json::object(), debugChildren_ = Json::object();
    Json debugBreakpointDrafts_ = Json::object();
    std::string debugBreakpointApplyPath_;
    std::string debugOperation_, debugProgram_, debugCwd_ = ".", debugArgs_, debugEnv_, debugAdapter_ = "automatic", debugWatch_, debugEvaluation_, debugSourcePath_, debugJumpKey_;
    std::string debugRuntime_ = "automatic", debugDotnet_ = "automatic";
    bool debugStopOnEntry_ = false, debugTerminal_ = true, debugProfileLoaded_ = false;
    bool debugBuildBefore_ = true, pendingDebugBuild_ = false;
    std::string debugCurrentPath_;
    size_t debugCurrentLine_ = 0;
    int debugAttachPid_ = 0;
    size_t debugSourceLine_ = 0, debugSourceColumn_ = 0;
    int debugVariableReference_ = 0;
    uint64_t debugVariablesStop_ = 0;
    double lastDebugPoll_ = 0;
    bool testSession_ = false;
    bool quitRequested_ = false, done_ = false, smoke_ = false, mcpWrite_ = false, caseSensitive_ = false,
         regex_ = false, searchQueued_ = false;
    double searchChanged_ = 0, lastGitRefresh_ = 0;
    size_t searchGeneration_ = 0, runningSearchGeneration_ = 0;
    std::string taskMethod_;
    std::shared_ptr<Dialog> dialog_ = std::make_shared<Dialog>();
};
} // namespace cycode
