#pragma once
#include "cycode/LanguageClient.h"
#include "cycode/Languages.h"
#include <future>
#include <optional>
#include <atomic>

namespace cycode {
// Routes live documents to independent bounded language-server owners.
class LanguageService {
  public:
    explicit LanguageService(std::filesystem::path root, bool managed = false);
    ~LanguageService();
    void configure(std::string executable, bool enabled, bool prewarm);
    void configureManaged(std::string executable, bool enabled, bool prewarm = true);
    void configureBuiltin(const std::string &backend, std::string executable, bool enabled, bool prewarm);
    Json configuration(const std::string &backend) const;
    void configureBackend(const std::string &backend, const std::vector<std::string> &argv,
                          const Json &settings, bool enabled, bool prewarm = true,
                          const Json &cargoProject = Json());
    void enableBackend(const std::string &backend, bool enabled, bool prewarm = true);
    void setProject(const std::filesystem::path &entry);
    void setCompilationDatabase(std::filesystem::path directory, bool reload = false);
    void sync(const std::string &path, const std::string &text, std::uint64_t revision, std::uint64_t identity = 0);
    void close(const std::string &path);
    std::future<Json> request(std::string method, Json params, std::shared_ptr<std::atomic<bool>> cancelled = {});
    Json status() const;
    void replaceExtensions(const Json &definitions);
    std::string backend(const std::string &path) const;
    bool accepts(const std::string &path) const {
        return !backend(path).empty();
    }
    bool hasBackend(const std::string &) const;
    uint64_t epoch(const std::string &path) const;
    uint64_t backendEpoch(const std::string &backend) const;
    static bool supports(const std::string &path);
    static bool supportsCpp(const std::string &path);
    static bool supportsManaged(const std::string &path);
    static std::string defaultExecutable();
    static std::string managedExecutable();
    static std::string managedHost();

  private:
    LanguageService(std::filesystem::path root, std::string backend, Json definition = Json());
    struct Document {
        std::string text;
        std::uint64_t revision, identity;
    };
    struct Query {
        std::string method;
        Json params;
        std::promise<Json> promise;
        std::chrono::steady_clock::time_point deadline;
        int id = 0;
        bool done = false;
        bool semanticFull = false;
        bool cancelSent = false;
        std::shared_ptr<std::atomic<bool>> cancelled;
    };
    void run();
    void configureOne(std::string executable, bool enabled, bool prewarm);
    std::filesystem::path root_;
    bool managed_;
    std::string backend_;
    std::map<std::string, std::unique_ptr<LanguageService>> children_;
    mutable std::recursive_mutex routingMutex_;
    Json extensionDefinitions_ = Json::array(), definition_;
    uint64_t routingEpoch_ = 0;
    std::vector<std::string> argv_;
    Json settings_ = Json::object();
    Json configuration_ = {{"arguments", Json::array()}, {"settings", Json::object()}};
    Json environment_ = Json::object();
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false, reconfigure_ = false, enabled_ = true, prewarm_ = false;
    bool launchPrepared_ = false;
    std::string executable_;
    std::filesystem::path compilationDatabase_;
    std::filesystem::path project_;
    std::map<std::string, std::optional<Document>> updates_;
    std::deque<std::shared_ptr<Query>> queries_;
    size_t outstanding_ = 0;
    uint64_t epoch_ = 0;
    Json status_ = {{"ready", false}, {"running", false}, {"error", ""}, {"diagnostics", Json::object()},
                   {"diagnosticGeneration",0},{"diagnosticRevisions",Json::object()}};
    std::thread worker_;
};
} // namespace cycode
