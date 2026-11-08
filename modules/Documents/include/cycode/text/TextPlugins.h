#pragma once
#include "cycode/text/PluginABI.h"
#include <filesystem>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
namespace cycode::text {
using Json=nlohmann::json;
class Plugins {
public:
    ~Plugins();
    void load(const std::filesystem::path &directory);
    Json describe() const;
    Json configure(const std::string &id,const Json &options);
    Json invoke(const std::string &id,const std::string &command,const Json &input);
    // Snapshot under Core's lock, execute elsewhere. Pins the DLL and serializes
    // that plugin's callbacks without holding the live document/UI mutex.
    std::function<Json()> prepare(const std::string &id,const std::string &command,const Json &input) const;
private:
    struct Plugin { Json manifest,options;std::shared_ptr<void> library;const CyCodeReaderPluginV1 *api=nullptr;std::shared_ptr<std::mutex> execution=std::make_shared<std::mutex>(); };
    std::map<std::string,Plugin> plugins_;
    Json errors_=Json::array();
};
std::string transformText(const std::string &,const std::string &operation);
}
