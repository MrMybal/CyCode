#pragma once
#include "cycode/Process.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>
namespace cycode {
class LoopbackStream;
using Json = nlohmann::json;
// Single-owner LSP transport. Callbacks run on the thread calling poll().
class LanguageClient {
  public:
    LanguageClient();
    ~LanguageClient();
    bool start(const std::filesystem::path &root, const std::string &executable,
               const std::filesystem::path &compilationDatabase = {}, Json configuration = Json::object());
    bool startServer(const std::filesystem::path &root, const std::vector<std::string> &argv,
                     Json configuration = Json::object(), bool waitForWorkspace = false,
                     Json environment = Json::object());
    // Reuse the bounded framed-JSON child transport for DAP, without LSP messages.
    bool startRaw(const std::filesystem::path &root, const std::string &executable);
    bool startRaw(const std::filesystem::path &root, const std::vector<std::string> &argv,
                  Json environment = Json::object());
    // Only a validated port returned by the local JDT LS debug-session command.
    bool connectRaw(std::uint16_t port);
    bool sendRaw(Json message) { return running_ && enqueue(std::move(message)); }
    std::deque<Json> receiveRaw();
    void stop();
    void poll();
    bool notify(const std::string &method, Json params);
    int request(const std::string &method, Json params, std::function<void(const Json &)> callback);
    void cancel(int id);
    bool ready() const {
        return initialized_ && running_ && (!waitForWorkspace_ || workspaceReady_);
    }
    bool initialized() const { return initialized_ && running_; }
    bool running() const {
        return running_;
    }
    std::string error() const;
    std::string logs() const;
    const Json &diagnostics() const {
        return diagnostics_;
    }
    const Json &diagnosticVersions() const { return diagnosticVersions_; }
    size_t diagnosticGeneration() const {
        return diagnosticGeneration_;
    }
    const Json &capabilities() const { return capabilities_; }
    size_t semanticGeneration() const { return semanticGeneration_; }
    size_t semanticRefreshGeneration() const { return semanticRefreshGeneration_; }
    void invalidateSemantic() { ++semanticGeneration_; }
    static std::string uri(const std::filesystem::path &);
    static std::filesystem::path fromUri(const std::string &);
    static std::string uriKey(const std::string &);
    static size_t utf16Length(const std::string &);

  private:
    struct Child;
    bool startImpl(const std::filesystem::path &, const std::vector<std::string> &, Json, bool raw,
                   const Json &environment = Json::object());
    Json configuration_, initializationOptions_, workspaceFolders_, capabilities_ = Json::object();
    size_t semanticGeneration_ = 0;
    size_t semanticRefreshGeneration_ = 0;
    bool waitForWorkspace_ = false, workspaceReady_ = false;
    std::string workspaceReadyNotification_;
    Json workspaceLoadToken_;
    std::unique_ptr<Child> child_;
    std::unique_ptr<LoopbackStream> socket_;
    bool enqueue(Json);
    void readLoop();
    void writeLoop();
    std::thread reader_, writer_;
    std::atomic<bool> running_{false};
    bool initialized_ = false;
    bool raw_ = false;
    size_t incomingBytes_ = 0;
    int nextId_ = 1;
    mutable std::mutex mutex_;
    std::condition_variable outgoingReady_;
    std::deque<std::string> outgoing_;
    size_t outgoingBytes_ = 0;
    std::deque<Json> incoming_;
    std::map<int, std::function<void(const Json &)>> callbacks_;
    Json diagnostics_ = Json::object();
    Json diagnosticVersions_=Json::object();
    size_t diagnosticGeneration_ = 0;
    std::string error_,logs_;
};
} // namespace cycode
