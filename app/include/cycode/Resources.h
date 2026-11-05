#pragma once
#include "cycode/Paths.h"
#include <functional>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
namespace cycode {
// Filesystem transactions only. Engine retains ownership of live documents.
class Resources {
  public:
    using Json = nlohmann::json;
    using Commit = std::function<Json(const Json &, const std::function<void()> &)>;
    Resources(std::filesystem::path root, std::filesystem::path entry);
    ~Resources();
    Json preview(const Json &, const Json &documents);
    Json apply(const std::string &, const Commit &);
    Json undo(const std::string &, const Json &documents, const Commit &);
    Json trash(size_t limit = 256) const;
    Json children(const std::string &path) const;
    // Host-owned binary payload; never accepted from the public resource JSON API.
    Json previewFileInstall(const std::string &path, std::optional<std::string> bytes,
                            bool executable, const Json &documents);
    Json applyFileInstall(const std::string &id, const Commit &);
    Json undoFileInstall(const std::string &id, const Json &documents, const Commit &);

  private:
    struct State;
    std::unique_ptr<State> state_;
};
bool resourceContains(const std::string &folder, const std::string &path);
std::string resourceRebase(const std::string &path, const std::string &from, const std::string &to);
} // namespace cycode
