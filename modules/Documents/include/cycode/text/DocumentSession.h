#pragma once
#include "cycode/text/Document.h"
#include "cycode/text/TextPlugins.h"
#include <memory>
#include <mutex>
namespace cycode::text {
struct LiveDocument { size_t id=0;Document document;uint64_t revision=0;bool readOnly=false;std::string observed,observedFormat; };
class DocumentSession {
public:
    explicit DocumentSession(const std::filesystem::path &plugins={});
    std::recursive_mutex &mutex() { return mutex_; }
    std::shared_ptr<LiveDocument> add(Document document={});
    void remove(size_t id);
    void refresh();
    const auto &documents() const { return documents_; }
    Plugins &plugins() { return plugins_; }
    Json dispatch(const std::string &,const Json &params=Json::object());
private:
    Json describe(const LiveDocument &,bool text=false) const;
    std::map<size_t,std::shared_ptr<LiveDocument>> documents_;
    std::recursive_mutex mutex_;
    Plugins plugins_;
    size_t nextId_=1;
    uint64_t revision_=1;
};
}
