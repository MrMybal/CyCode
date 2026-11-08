#include "cycode/LocalEndpoint.h"
#include "cycode/LoopbackStream.h"
#include "cycode/TextFile.h"
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace cycode {
namespace {
using Json=nlohmann::json;
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
Json exchange(const Json &descriptor,Json request) {
    LoopbackStream stream;require(stream.connect(descriptor.at("port").get<uint16_t>()),"Endpoint connection failed");
    if(!request.contains("token"))request["token"]=descriptor.at("token");
    const auto payload=request.dump()+"\n";size_t at=0;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(at<payload.size()) {
        if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Endpoint write timed out");
        auto count=stream.write(payload.data()+at,payload.size()-at);
        if(count<0)throw std::runtime_error("Endpoint closed while writing");
        if(count>0)at+=static_cast<size_t>(count);else std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::string response;char buffer[8192];
    while(response.find('\n')==std::string::npos) {
        if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Endpoint response timed out");
        auto count=stream.read(buffer,sizeof(buffer));
        if(count<0)throw std::runtime_error("Endpoint closed without response");
        if(count>0)response.append(buffer,static_cast<size_t>(count));else std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return Json::parse(response);
}
}
void localEndpointChecks(const std::filesystem::path &base) {
    const auto path=base/"local-endpoint.json";
    size_t active=0;bool cancelled=false;
    std::mutex mutex;std::condition_variable condition;
    LocalEndpoint::Options options;options.descriptor={{"protocol","transport-test/1"}};
    options.control=[](const std::string &method){return method=="cancel";};
    options.respond=[&](const Json &request)->Json {
        const auto method=request.value("method","");
        if(method=="wait") {
            std::unique_lock lock(mutex);++active;condition.notify_all();
            if(!condition.wait_for(lock,std::chrono::seconds(10),[&]{return cancelled;}))throw std::runtime_error("Control request never reached the endpoint");
        } else if(method=="cancel") {std::lock_guard lock(mutex);cancelled=true;condition.notify_all();}
        else if(method=="large")return {{"result",std::string(LocalEndpoint::maximumResponseBytes+1,'x')}};
        return {{"result",request.value("id",Json{})}};
    };
    LocalEndpoint endpoint(path,options);
    const auto descriptor=Json::parse(readTextFileBytes(path));
    require(descriptor.at("host")=="127.0.0.1" && descriptor.at("protocol")=="transport-test/1","Descriptor identity changed");
    require(exchange(descriptor,{{"method","echo"},{"id","request-7"}}).at("result")=="request-7","Request dispatch lost identity");
    require(exchange(descriptor,{{"token","wrong"},{"method","echo"}}).at("error").at("code")=="TransportError","Unauthenticated transport request accepted");
    require(exchange(descriptor,{{"method","large"}}).at("error").at("message")=="Native response exceeds 20 MiB","Response memory bound was not enforced");
    // Fill all ordinary slots and verify that the reserved control path still
    // reaches the application. The endpoint does not own a second job scheduler.
    std::vector<std::future<Json>> pending;
    struct Release {std::mutex &mutex;std::condition_variable &condition;bool &cancelled;~Release(){std::lock_guard lock(mutex);cancelled=true;condition.notify_all();}} release{mutex,condition,cancelled};
    for(size_t i=0;i<LocalEndpoint::maximumClients;++i)pending.push_back(std::async(std::launch::async,[&,i]{return exchange(descriptor,{{"method","wait"},{"id",i}});}));
    {
        std::unique_lock lock(mutex);
        require(condition.wait_for(lock,std::chrono::seconds(5),[&]{return active==LocalEndpoint::maximumClients;}),"Endpoint did not accept ordinary concurrent requests");
    }
    require(exchange(descriptor,{{"method","cancel"},{"id","control"}}).at("result")=="control","Endpoint starved control requests");
    for(size_t i=0;i<pending.size();++i)require(pending[i].get().at("result")==i,"Concurrent request result was mixed");
    LocalEndpoint newer(path,options);const auto replacement=readTextFileBytes(path);
    endpoint.stop();endpoint.stop();
    require(readTextFileBytes(path)==replacement,"An old endpoint deleted a newer instance's descriptor");
    newer.stop();require(!std::filesystem::exists(path),"Stopped endpoint left its own descriptor");
    auto invalid=options;invalid.respond={};
    bool refused=false;try{LocalEndpoint bad(path,invalid);}catch(const std::invalid_argument &){refused=true;}
    require(refused && !std::filesystem::exists(path),"Failed endpoint construction published a descriptor");
}
} // namespace cycode
