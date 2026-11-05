#pragma once
#include "cycode/Process.h"
#include <functional>
#include <nlohmann/json.hpp>
#include <set>
namespace cycode {
// Read-only metadata. CMake remains the authority for executing preset semantics.
class CMakePresets {
  public:
    using Json = nlohmann::json;
    using Resolver = std::function<std::filesystem::path(const std::string &)>;
    CMakePresets(std::filesystem::path source, Resolver);
    Json catalogue();
    Json selection(std::string configure, const std::string &build);
    Json action(const std::string &kind, const std::string &preset);
    std::string revision(const Json &selection = Json::object()) const;
    void checkUnchanged() const;

  private:
    struct Entry {
        Json value;
        std::filesystem::path file;
    };
    struct Resolved {
        Json value;
        std::map<std::string, std::filesystem::path> origins;
        std::filesystem::path file;
    };
    void load(const std::filesystem::path &);
    Resolved resolve(const std::string &kind, const std::string &, std::set<std::string> &);
    std::string expand(const std::string &, const Resolved &, const std::filesystem::path &,
                       std::set<std::string> &);
    Resolved actionContext(const std::string &kind, const std::string &name);
    std::filesystem::path source_;
    Resolver resolver_;
    std::map<std::string, std::string> files_;
    std::map<std::string, bool> rootPresence_;
    std::map<std::string, std::set<std::string>> reachable_;
    std::map<std::string, std::map<std::string, Entry>> presets_;
    std::map<std::string, Resolved> resolved_;
    std::set<std::string> loading_;
    size_t bytes_ = 0;
    size_t resolvedBytes_ = 0;
};
} // namespace cycode
