#pragma once
#include "cycode/LanguageClient.h"
#include <future>
#include <optional>

namespace cycode {
// Routes live documents to independent bounded C++ and C# server owners.
class LanguageService {
  public:
    explicit LanguageService(std::filesystem::path root, bool managed = false);
    ~LanguageService();
    void configure(std::string executable, bool enabled, bool prewarm);
    void configureManaged(std::string executable, bool enabled, bool prewarm = true);
    void setProject(const std::filesystem::path &entry);
    void setCompilationDatabase(std::filesystem::path directory, bool reload = false);
    void sync(const std::string &path, const std::string &text, std::uint64_t revision);
    void close(const std::string &path);
    std::future<Json> request(std::string method, Json params);
    Json status() const;
    static bool supports(const std::string &path);
    static bool supportsCpp(const std::string &path);
    static bool supportsManaged(const std::string &path);
    static std::string defaultExecutable();
    static std::string managedExecutable();
    static std::string managedHost();

  private:
    struct Document {
        std::string text;
        std::uint64_t revision;
    };
    struct Query {
        std::string method;
        Json params;
        std::promise<Json> promise;
        std::chrono::steady_clock::time_point deadline;
        int id = 0;
        bool done = false;
    };
    void run();
    std::filesystem::path root_;
    bool managed_;
    std::unique_ptr<LanguageService> managedService_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false, reconfigure_ = false, enabled_ = true, prewarm_ = false;
    std::string executable_;
    std::filesystem::path compilationDatabase_;
    std::filesystem::path project_;
    std::map<std::string, std::optional<Document>> updates_;
    std::deque<std::shared_ptr<Query>> queries_;
    size_t outstanding_ = 0;
    Json status_ = {{"ready", false}, {"running", false}, {"error", ""}, {"diagnostics", Json::object()}};
    std::thread worker_;
};
} // namespace cycode
