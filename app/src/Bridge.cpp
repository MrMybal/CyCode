#include "cycode/Bridge.h"
namespace cycode {
Bridge::Bridge(Engine &engine,const std::filesystem::path &path):service_(engine),descriptor_(path) {
    LocalEndpoint::Options options;
    options.descriptor={{"protocol","cycode-native/1"},{"workspace",pathUtf8(engine.workspace())},{"coreProtocol",CoreService::protocol}};
    options.respond=[this](const Json &input){return service_.respond(input);};
    options.control=[](const std::string &method){return method=="service.status" || method=="project.cancel" || method=="build.cancel" || method=="tests.cancel" || method=="analysis.status" || method=="analysis.cancel" || method=="analysis.results";};
    options.error=[](const std::string &message){return CoreService::error("TransportError",message);};
    endpoint_=std::make_unique<LocalEndpoint>(descriptor_,std::move(options));
}
Bridge::~Bridge(){service_.beginShutdown();if(endpoint_)endpoint_->stop();}
Json Bridge::serviceStatus() const {return service_.status();}
} // namespace cycode
