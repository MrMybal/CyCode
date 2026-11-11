#pragma once
#include "cycode/PluginAPI.h"
#include "cycode/PluginPackages.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <stop_token>
namespace cycode {
class Plugins {
  public:
    using Json = nlohmann::json;
    using HostRequest = std::function<Json(const std::string &, const Json &)>;
    ~Plugins();
    void shutdown();
    void pumpEvents(const std::function<Json(const Json &)> &read,std::stop_token stop={});
    void workspace(const std::filesystem::path &, HostRequest);
    void load(const std::filesystem::path &directory);
    void loadInstalled(const std::filesystem::path &installation);
    Json describe() const;
    Json configure(const Json &);
    Json contributions(const std::string &kind, const Json &context = Json::object()) const;
    Json invoke(const std::string &, const Json &);
    Json vcs(const std::string &, const std::string &, const Json &);
    Json project(const std::string &, const std::string &, const Json &);
    uint64_t revision() const;
    Json templateSnapshot() const;
    Json snippetSnapshot() const;
    Json languageSnapshot() const;
    Json serviceSnapshot() const;
    Json createFromTemplate(const std::string &owner, uint64_t revision, const std::function<Json()> &);

  private:
    struct Record {
        Json info;
        void *library = nullptr;
        CyCodePluginShutdown stop = nullptr;
        CyCodePluginShutdownV2 stopV2 = nullptr;
        CyCodePluginActiveV2 activationV2 = nullptr;
        void *instance = nullptr;
        bool enabled = true, loaded = false;
        bool lifecycleActive = false;
        std::shared_ptr<NativeFileLease> packageLease;
    };
    struct Contribution {
        std::string owner, kind;
        Json info;
        CyCodeCommand function;
        CyCodeCommandV2 functionV2;
    };
    static void addCommand(void *, const char *, const char *, CyCodeCommand);
    static void addVcs(void *, const char *, const char *, const char *, CyCodeCommand);
    static int contribute(void *, const char *, const char *, CyCodeCommandV2);
    static const char *request(void *, const char *, const char *);
    void add(const std::string &, Json, CyCodeCommand, CyCodeCommandV2, bool legacy = false);
    void initialize(const std::string &);
    void notifyActivation(const std::string &, bool);
    void activateDormant();
    void loadDirectories(const std::vector<std::filesystem::path> &, const std::vector<ManagedPluginCandidate> & = {});
    bool active(const std::string &) const;
    Json call(const Contribution &, const Json &, bool allowMutations = true);
    Json provider(const std::string &, const std::string &, const std::string &, const Json &);
    std::filesystem::path statePath() const;
    std::map<std::string, Record> records_;
    std::vector<std::string> loadOrder_;
    std::vector<Contribution> contributions_;
    std::filesystem::path workspace_;
    HostRequest host_;
    Json state_ = Json::object(), errors_ = Json::array();
    std::optional<std::string> baseline_;
    std::string registering_, invoking_, registrationError_, response_;
    std::atomic<uint64_t> revision_ = 1;
    bool configurationValid_ = true;
    bool hostMutationAllowed_ = false;
    CyCodeHostV1 hostV1_{};
    CyCodeHostV2 hostV2_{};
    mutable std::recursive_mutex mutex_;
    struct EventState {std::string epoch,error;uint64_t cursor=0;bool started=false;};
    std::map<std::string,EventState> eventStates_;
    size_t eventRoundRobin_=0;
};
} // namespace cycode
