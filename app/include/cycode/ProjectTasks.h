#pragma once
#include "cycode/Terminal.h"
#include "cycode/TaskOutput.h"
#include <functional>
#include <mutex>
namespace cycode {
class ProjectTasks {
  public:
    using Json=nlohmann::json;
    using Resolver=std::function<std::filesystem::path(const std::string &)>;
    ProjectTasks(std::filesystem::path root,std::filesystem::path entry,Resolver resolve,Terminals &terminals);
    Json inspect(const Json &params=Json::object());
    Json execute(const std::string &method,const Json &params);
    Json cancel(const std::string &operationId="");
    Json status() const;
    Json output(const Json &parameters=Json::object()) const;
    static std::string detect(const std::filesystem::path &directory);
  private:
    Json profile(const Json &) const;
    Json model(const Json &) const;
    Json perform(const std::string &,const Json &);
    void save(const Json &) const;
    std::filesystem::path root_,entry_;
    Resolver resolve_;
    Terminals &terminals_;
    std::mutex operationMutex_;
    mutable std::mutex stateMutex_;
    std::atomic<bool> cancellation_{false};
    std::string runningId_,runningMethod_,outputStream_;
    TaskOutput output_;
};
}
