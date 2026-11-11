#include "cycode/WorkspaceFileIndex.h"
#include "cycode/Process.h"
#include "cycode/ProcessText.h"
#include <algorithm>
#include <stdexcept>
namespace cycode {
WorkspaceFileIndex::~WorkspaceFileIndex() {stop();}
void WorkspaceFileIndex::stop() noexcept {
    stopped_=true;abortProcess_=true;
    if(worker_.joinable())worker_.join();
}
std::shared_ptr<const WorkspaceFileSnapshot> WorkspaceFileIndex::snapshot(uint64_t revision,bool refresh) {
    std::lock_guard lifecycle(lifecycle_);
    bool restart;
    {std::lock_guard lock(mutex_);restart=refresh||!current_||current_->revision!=revision;}
    if(restart) {
        stop();stopped_=false;abortProcess_=false;
        publish(nlohmann::json::array(),revision,true);
        worker_=std::thread([this,revision]{scan(revision);});
    }
    std::unique_lock lock(mutex_);
    if(current_->indexing&&current_->files.empty()) {
        const auto before=current_->generation;
        // Preserve inexpensive first-call behavior on small trees. Large trees
        // return within this budget while their original stream keeps running.
        changed_.wait_for(lock,std::chrono::milliseconds(500),[&]{return current_->generation!=before;});
    }
    return current_;
}
void WorkspaceFileIndex::publish(const nlohmann::json &paths,uint64_t revision,bool indexing,const std::string &error) {
    auto value=std::make_shared<WorkspaceFileSnapshot>();
    value->files=paths;
    std::sort(value->files.begin(),value->files.end());
    value->related.rebuild(value->files);
    value->revision=revision;value->indexing=indexing;value->complete=!indexing&&error.empty();value->error=error;
    {std::lock_guard lock(mutex_);if(stopped_)return;value->generation=++generation_;current_=std::move(value);}
    changed_.notify_all();
}
void WorkspaceFileIndex::scan(uint64_t revision) noexcept {
    auto paths=nlohmann::json::array();
    std::string pending,failure;
    auto last=std::chrono::steady_clock::now();
    try {
        const auto result=runProcessStreamed({"rg","--files","--hidden","--no-require-git","-0",
            "-g","!.git","-g","!runtime","-g","!data","-g","!.cycode"},root_,abortProcess_,
            [&](std::string_view chunk) {
                if(abortProcess_)return;
                try {
                    pending.append(chunk);
                    size_t start=0;
                    while(start<pending.size()) {
                        const auto end=pending.find('\0',start);
                        if(end==std::string::npos)break;
                        auto path=pending.substr(start,end-start);
                        std::replace(path.begin(),path.end(),'\\','/');
                        (void)nlohmann::json(path).dump(); // Validate UTF-8 before publication.
                        paths.push_back(std::move(path));start=end+1;
                        if(stopped_)return;
                    }
                    pending.erase(0,start);
                    if(pending.size()>1024*1024)throw std::runtime_error("Invalid file-enumeration record exceeds 1 MiB");
                    if(std::chrono::steady_clock::now()-last>=std::chrono::seconds(1)) {
                        publish(paths,revision,true);last=std::chrono::steady_clock::now();
                    }
                } catch(const std::exception &error) {failure=error.what();abortProcess_=true;}
            });
        if(stopped_)return;
        if(failure.empty()&&(result.exitCode!=0&&result.exitCode!=1))
            failure="File enumeration failed: "+processOutputText(result.output,true);
        if(failure.empty()&&!pending.empty())failure="File enumeration ended with an incomplete record";
        publish(paths,revision,false,failure);
    } catch(const std::exception &error) {
        if(!stopped_)try {publish(paths,revision,false,error.what());}catch(...) {}
    } catch(...) {
        if(!stopped_)try {publish(paths,revision,false,"File enumeration failed");}catch(...) {}
    }
}
} // namespace cycode
