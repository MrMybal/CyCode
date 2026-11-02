#pragma once
#include "cycode/BuildSystem.h"
#include "cycode/LanguageService.h"
#include "cycode/Plugins.h"
#include "cycode/Process.h"
#include "cycode/Terminal.h"
#include "cycode/Debugger.h"
#include "cycode/UnrealProject.h"
#include "cycode/MsBuild.h"
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
using Json = nlohmann::json;
class Engine {
  public:
    explicit Engine(std::filesystem::path workspace);
    Json dispatch(const std::string &method, const Json &params = Json::object());
    Json searchText(const Json &, const std::atomic<bool> *cancellation = nullptr);
    void configureLanguage(const std::string &executable, bool enabled = true, bool prewarm = true);
    Json prepareUnrealAnalysis(const Json &, const std::atomic<bool> *cancellation = nullptr);
    Json languageStatus() const {
        return language_.status();
    }
    std::filesystem::path resolve(const std::string &relative) const;
    const std::filesystem::path &workspace() const {
        return root_;
    }
    Plugins &plugins() {
        return plugins_;
    }
    Terminals &terminals() { return terminals_; }
    Debugger &debugger() { return debugger_; }
    Json navigationSource(const std::filesystem::path &, const Json &range);

  private:
    struct Document {
        std::string text, saved, disk;
        std::uint64_t revision = 1;
        bool bom = false;
        bool crlf = false;
        bool crOnly = false;
        bool dirty = false;
    };
    Json documentJson(const std::string &, const Document &) const;
    Document &openLocked(const std::string &);
    Json files();
    Json git(const std::string &, const Json &);
    Json gitAdvanced(const std::string &, const Json &);
    void refreshGitDocuments(Json &result);
    Json languageQuery(const std::string &, const Json &);
    Json highlight(const Json &);
    Json symbolQuery(const std::string &, const Json &);
    Json prepareChange(const std::string &, const Json &);
    Json changeOperation(const std::string &, const Json &);
    Json changeDescription(const std::string &) const;
    Json sourceOperation(const std::string &, const Json &);
    Json sourceLocationLocked(const std::string &, const Json &, bool);
    std::pair<Document *, std::filesystem::path> semanticDocumentLocked(const std::string &);
    void addSdkRoots(const Json &unreal);
    struct FileChange {
        std::string path, before, after, disk;
        Json edits;
        std::uint64_t revision = 0, appliedRevision = 0;
    };
    struct ChangePlan {
        std::string title, state = "preview";
        std::vector<FileChange> files;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    };
    std::map<std::string, ChangePlan> changes_;
    size_t nextChange_ = 1;
    struct ExternalSource { std::filesystem::path path; Document document; bool loaded = false; };
    std::map<std::string, ExternalSource> externalSources_;
    std::vector<std::filesystem::path> sdkRoots_;
    size_t nextSource_ = 1;
    std::filesystem::path root_, entry_;
    Json solution_;
    std::map<std::string, Document> documents_;
    std::uint64_t nextRevision_ = 1;
    mutable std::mutex mutex_;
    std::recursive_mutex vcsMutex_;
    Plugins plugins_;
    BuildSystem build_;
    MsBuild msbuild_;
    UnrealProject unreal_;
    LanguageService language_;
    Terminals terminals_;
    Debugger debugger_;
};
} // namespace cycode
