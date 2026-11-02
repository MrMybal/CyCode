#pragma once
#include "cycode/PluginAPI.h"
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
class Plugins {
  public:
    ~Plugins();
    void load(const std::filesystem::path &directory);
    nlohmann::json describe() const;
    nlohmann::json invoke(const std::string &id, const nlohmann::json &input);
    nlohmann::json vcs(const std::string &provider, const std::string &action, const nlohmann::json &input);

  private:
    struct Command {
        std::string id, title;
        CyCodeCommand function;
    };
    struct Provider {
        std::string id, title;
        nlohmann::json capabilities;
        CyCodeCommand function;
    };
    static void addCommand(void *, const char *, const char *, CyCodeCommand);
    static void addVcs(void *, const char *, const char *, const char *, CyCodeCommand);
    std::vector<void *> libraries_;
    std::vector<Command> commands_;
    std::vector<Provider> providers_;
    nlohmann::json plugins_ = nlohmann::json::array();
    mutable std::recursive_mutex mutex_;
};
} // namespace cycode
