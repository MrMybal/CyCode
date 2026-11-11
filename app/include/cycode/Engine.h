#pragma once
#include "cycode/BuildOperations.h"
#include "cycode/BuildSystem.h"
#include "cycode/CompletionSelection.h"
#include "cycode/Debugger.h"
#include "cycode/DocumentReference.h"
#include "cycode/Events.h"
#include "cycode/GitRepositories.h"
#include "cycode/LanguagePreferences.h"
#include "cycode/LanguageService.h"
#include "cycode/LargeText.h"
#include "cycode/MsBuild.h"
#include "cycode/NativeFileLease.h"
#include "cycode/Plugins.h"
#include "cycode/Process.h"
#include "cycode/ProjectTasks.h"
#include "cycode/ProjectTemplates.h"
#include "cycode/Recovery.h"
#include "cycode/RelatedFiles.h"
#include "cycode/Resources.h"
#include "cycode/Terminal.h"
#include "cycode/TestRunner.h"
#include "cycode/UnrealProject.h"
#include "cycode/UserSnippets.h"
#include "cycode/WorkspaceFileIndex.h"
#include "cycode/WorkspaceMonitor.h"
#include "cycode/text/DocumentState.h"
#include <condition_variable>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>
namespace cycode {
using Json = nlohmann::json;
class Engine {
  public:
    explicit Engine(std::filesystem::path workspace, std::filesystem::path languagePreferencesDirectory = {},
                    std::filesystem::path userSnippetsDirectory = {});
    uint64_t userSnippetGeneration() const { return userSnippets_.generation(); }
    ~Engine();
    Json dispatch(const std::string &method, const Json &params = Json::object());
    Json userSnippetDraft(const Json &);
    Json relatedFiles(const Json &, bool included = false);
    Json searchText(const Json &, const std::atomic<bool> *cancellation = nullptr);
    void configureLanguage(const std::string &executable, bool enabled = true, bool prewarm = true);
    Json prepareUnrealAnalysis(const Json &, const std::atomic<bool> *cancellation = nullptr);
    Json languageStatus();
    std::string languageBackendFor(const std::string &path) const {
        return isUntitledDocument(path) ? std::string{} : language_.backend(path);
    }
    bool supportsLanguage(const std::string &path) const {
        return !isUntitledDocument(path) && language_.accepts(path);
    }
    std::filesystem::path resolve(const std::string &relative) const;
    const std::filesystem::path &workspace() const { return root_; }
    Plugins &plugins() { return plugins_; }
    Terminals &terminals() { return terminals_; }
    Debugger &debugger() { return debugger_; }
    Json navigationSource(const std::filesystem::path &, const Json &range);

  private:
    Json configureLanguageBackend(const Json &);
    void applyLanguageConfiguration(const std::string &, const Json &, bool prewarm);
    Json inspectCpp(const Json &, const std::atomic<bool> *);
    Json buildOperation(const std::string &, const Json &, const std::atomic<bool> *);
    using Document = text::BufferState;
    Json documentJson(const std::string &, const Document &) const;
    std::string documentKey(const std::string &) const;
    Json createUntitled(const Json &);
    Json saveDocumentAs(const Json &);
    void checkUntitledQuota(const std::string &, size_t) const;
    Document &openLocked(const std::string &, bool publish = true);
    void documentEvent(const std::string &type, const std::string &path, const Document &,
                       const std::string &transaction = "");
    Json files(const Json &);
    Json pollWorkspace();
    Json resourceOperation(const std::string &, const Json &);
    Json resourceDocuments();
    Json resourceCommit(const Json &, const std::function<void()> &);
    std::unique_ptr<Resources> resources_;
    std::unique_ptr<ProjectTemplates> templates_;
    std::mutex resourcesMutex_, resourceEventsMutex_;
    Json resourceEvents_ = Json::array();
    uint64_t resourceRevision_ = 0, nextDocumentIdentity_ = 1;
    size_t resourceEventBytes_ = 0;
    void checkpointRecovery();
    void retireRecovery(const std::string &);
    Json recoveryOperation(const std::string &, const Json &);
    Json recoveryStatus();
    WorkspaceMonitor monitor_;
    WorkspaceFileIndex fileIndex_;
    std::mutex workspacePollMutex_, filesMutex_;
    uint64_t polledWorkspaceRevision_ = 0, filesIndexRevision_ = 0;
    std::chrono::steady_clock::time_point lastWorkspacePoll_{};
    std::shared_ptr<const WorkspaceFileSnapshot> filesSnapshot_;
    uint64_t filesCacheGeneration_ = 0;
    Json git(const std::string &, const Json &);
    GitRepositorySelection gitSelection(const Json &) const;
    Json gitWorktrees(const std::string &, const Json &);
    Json gitWorktreeMaintenance(const std::string &, const Json &);
    Json gitSubmodules(const std::string &, const Json &);
    Json gitAdvanced(const std::string &, const Json &);
    Json gitConflicts(const std::string &, const Json &);
    Json gitCheckout(const std::string &, const Json &);
    Json gitMerge(const std::string &, const Json &);
    Json gitReplay(const std::string &, const Json &);
    std::map<std::string, Json> gitCheckoutPlans_;
    Json gitHunks(const std::string &, const Json &);
    struct HunkPlan {
        std::string repository, repositoryIdentity;
        std::string path, patch, index;
        std::vector<std::string> hunks;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
        bool staged = false;
    };
    std::map<std::string, HunkPlan> hunkPlans_;
    uint64_t nextHunkPlan_ = 1;
    Json versionControl(const std::string &, const Json &);
    Json versionControlContext(const Json &);
    Json versionControlProviders() const;
    Json vcsConfiguration_;
    std::optional<std::string> vcsBaseline_;
    uint64_t vcsConfigurationRevision_ = 1;
    void refreshGitDocuments(Json &result, const std::filesystem::path &scope = {});
    Json languageQuery(const std::string &, const Json &);
    void refreshPluginServices();
    std::mutex languagePluginsMutex_;
    std::atomic<uint64_t> languagePluginsRevision_ = 0;
    Json completionOperation(const std::string &, const Json &);
    Json snippetCompletions(const Json &);
    Json codeActionOperation(const std::string &, const Json &);
    Json codeActionDescriptionLocked(const Json &, size_t, bool, const std::string &backend);
    Json issueCompletion(const std::string &, uint64_t revision, uint64_t generation, uint64_t epoch,
                         const Json &position, const Json &result, bool local = false,
                         std::string catalogue = {}, std::optional<CompletionSelection> selection = {},
                         uint64_t pluginRevision = 0, std::string userRevision = {});
    Json highlight(const Json &);
    Json symbolQuery(const std::string &, const Json &);
    Json resolveSymbol(const Json &);
    struct SymbolSession {
        std::string backend;
        uint64_t generation = 0, context = 0, diskGeneration = 0, epoch = 0;
        bool includeExternal = false;
        size_t bytes = 0;
        Json items = Json::array(), descriptions = Json::object(), resolved = Json::object();
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    };
    void pruneSymbolSessionsLocked();
    std::map<std::string, SymbolSession> symbolSessions_;
    uint64_t nextSymbolSession_ = 1;

    Json prepareChange(const std::string &, const Json &);
    Json analysisOperation(const std::string &, const Json &);
    Json reconcileNavigationTargets(Json, const Json &, uint64_t, uint64_t);
    Json prepareAnalysis(const Json &, const std::string &symbol = {}, const std::string &source = {},
                         uint64_t *generation = nullptr, Json *renameEdits = nullptr);
    // Immutable rows keep session copies cheap; progress publication never
    // duplicates every diagnostic object on each batch.
    struct AnalysisRows {
        std::vector<std::shared_ptr<const Json>> rows;
        size_t size() const { return rows.size(); }
        const Json &at(size_t index) const { return *rows.at(index); }
        void push_back(Json value) { rows.push_back(std::make_shared<const Json>(std::move(value))); }
    };
    struct AnalysisSession {
        std::string id, scope, compilerContext, compilerDigest, checkpoint, checkpointRevision;
        size_t historicalObserved = 0;
        std::shared_ptr<NativeFileLease> checkpointLease;
        bool discoveryComplete = false;
        std::vector<std::string> paths;
        AnalysisRows records;
        Json summary = Json::object(), pathPages = Json::array(), recordPages = Json::array();
        size_t savedRecords = 0;
        uint64_t generation = 0, workspaceRevision = 0;
        std::map<std::string, uint64_t> epochs;
    };
    AnalysisSession analysisSession_;
    uint64_t nextAnalysisSession_ = 1;
    std::set<std::string>
        analysisTemporaryDocuments_; // guarded by mutex_; ordinary readers promote ownership
    std::string analysisCompilerContext() const;
    mutable std::mutex analysisMutex_, analysisStateMutex_;
    std::chrono::steady_clock::time_point analysisFileStarted_{};
    std::shared_ptr<std::atomic<bool>> analysisCancellation_;
    std::string analysisOperationId_;
    Json analysisState_ = {{"state", "idle"}, {"phase", "idle"}, {"completed", 0},
                           {"total", 0},      {"progress", 0},   {"error", ""}};
    struct NavigationCacheEntry {
        Json result;
        uint64_t generation = 0, workspaceRevision = 0, epoch = 0;
        std::string compilerContext;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
        std::vector<std::pair<std::filesystem::path, std::pair<std::filesystem::file_time_type, uintmax_t>>>
            targets;
    };
    std::map<std::string, NavigationCacheEntry> navigationCache_;
    size_t navigationCacheBytes_ = 0;
    struct NavigationContext {
        uint64_t revision = 0, generation = 0, epoch = 0;
    };
    std::map<std::string, NavigationContext> navigationContexts_;
    Json changeOperation(const std::string &, const Json &);
    Json changeDescription(const std::string &) const;
    Json sourceOperation(const std::string &, const Json &);
    Json sourceLocationLocked(const std::string &, const Json &, bool);
    std::pair<Document *, std::filesystem::path> semanticDocumentLocked(const std::string &);
    void addSdkRoots(const Json &unreal);
    struct PagedDocument {
        std::shared_ptr<LargeText> text;
        uint64_t identity = 0, revision = 0;
        bool busy = false, knownDirty = false;
    };
    std::map<std::string, PagedDocument> pagedDocuments_;
    std::set<std::string> pendingPagedDocuments_;
    std::shared_ptr<SpoolStore> workingStore_;
    std::shared_ptr<SpoolStore> workingStoreLocked();
    Json pagedDocumentOperation(const std::string &, const Json &);
    Json pagedRecoveryOperation(const std::string &, const Json &);
    Json pagedDocumentDescription(const std::string &, const PagedDocument &) const;
    struct FileChange {
        std::string path, before, after, disk;
        Json edits;
        std::uint64_t revision = 0, appliedRevision = 0;
        struct Stored {
            StoredBlob before, after, disk;
        };
        std::shared_ptr<const Stored> stored;
        std::shared_ptr<LargeText> paged;
        LargeTextSnapshot pagedBefore, pagedAfter;
    };
    void spillChangeFile(FileChange &);
    struct ChangePlan {
        Json annotations = Json::array(), coverage;
        uint64_t scopeGeneration = 0, scopeWorkspaceRevision = 0, scopeEpoch = 0;
        std::string scopeBackend;
        std::string title, state = "preview";
        std::vector<FileChange> files;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    };
    Json storeChange(ChangePlan);
    Json workspaceEditFilesLocked(const Json &);
    Json workspaceChangeLocked(const std::string &, const Json &, const std::string &oldName = {},
                               bool onlyOpened = false);
    struct ActionSession {
        std::string path;
        uint64_t revision, generation, epoch;
        Json items;
        bool resolvable = false;
        size_t bytes = 0;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    };
    std::map<std::string, ActionSession> codeActions_;
    size_t nextAction_ = 1;
    struct HierarchySession {
        std::string path, kind, backend;
        uint64_t revision = 0, identity = 0, generation = 0, diskGeneration = 0, epoch = 0;
        bool includeExternal = false;
        size_t bytes = 0;
        std::map<std::string, Json> items, descriptions;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    };
    Json hierarchyOperation(const std::string &, const Json &);
    Json hierarchyNode(HierarchySession &, const Json &);
    std::string hierarchyInvalid(const HierarchySession &) const;
    std::map<std::string, HierarchySession> hierarchies_;
    uint64_t hierarchyGeneration_ = 1, nextHierarchy_ = 1;
    struct CompletionSession {
        std::string path;
        uint64_t revision, generation, epoch;
        Json position, items;
        std::set<size_t> resolved;
        size_t bytes;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
        bool local = false;
        std::string catalogue;
        std::optional<CompletionSelection> selection;
        uint64_t snippetPluginRevision = 0;
        std::string userSnippetRevision;
    };
    std::map<std::string, CompletionSession> completions_;
    size_t nextCompletion_ = 1;
    std::map<std::string, ChangePlan> changes_;
    size_t nextChange_ = 1;
    struct ExternalSource {
        std::filesystem::path path;
        Document document;
        bool loaded = false;
    };
    std::map<std::string, ExternalSource> externalSources_;
    std::vector<std::filesystem::path> sdkRoots_;
    size_t nextSource_ = 1;
    std::filesystem::path root_, entry_;
    RecoveryStore recovery_;
    std::mutex recoveryMutex_, recoveryWaitMutex_;
    std::condition_variable_any recoveryWake_;
    Json recoveryErrors_ = Json::array();
    Json solution_;
    Events events_;
    std::map<std::string, Document> documents_;
    std::map<std::string, Document> untitled_;
    std::uint64_t nextRevision_ = 1;
    // Allocating a read-only analysis draft issues a real unique revision but
    // does not change existing documents or invalidate their semantic snapshots.
    // Both counters and this accessor are protected by mutex_.
    std::uint64_t analysisRevisionSlots_ = 0;
    std::uint64_t documentGenerationLocked() const noexcept { return nextRevision_ - analysisRevisionSlots_; }
    mutable std::mutex mutex_;
    std::recursive_mutex vcsMutex_;
    Plugins plugins_;
    BuildSystem build_;
    BuildOperations buildOperations_;
    MsBuild msbuild_;
    UnrealProject unreal_;
    LanguageService language_;
    LanguagePreferences languagePreferences_;
    UserSnippets userSnippets_;
    std::mutex languageConfigurationMutex_;
    std::map<std::string, Json> languageChoices_;
    Terminals terminals_;
    ProjectTasks projects_;
    TestRunner tests_;
    Debugger debugger_;
    std::jthread recoveryWorker_;
    std::mutex eventWaitMutex_;
    std::condition_variable_any eventWake_;
    std::jthread eventWorker_;
    std::atomic<bool> shuttingDown_{false}, eventWorkerFinished_{false};
};
} // namespace cycode
