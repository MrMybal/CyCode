#include "cycode/text/SessionBridge.h"
#include "cycode/ServiceProtocol.h"
namespace cycode::text {
SessionBridge::SessionBridge(DocumentSession &session,const std::filesystem::path &path):session_(session),descriptor_(path) {
    LocalEndpoint::Options options;
    options.descriptor={{"protocol","cycodereader-native/1"},{"application","CyCodeReader"},{"coreProtocol",coreProtocol},{"profile","text"}};
    options.respond=[this](const Json &request){return respondNative(request,[this](const std::string &method,const Json &params)->Json {
        if(method=="service.status")return serviceStatus();
        if(stopping_)throw NativeError(ErrorCode::ServiceStopping,"The text session is draining");
        return session_.dispatch(method,params);
    });};
    options.control=[](const std::string &method){return method=="service.status";};
    options.error=[](const std::string &message){return nativeErrorResponse("TransportError",message);};
    endpoint_=std::make_unique<LocalEndpoint>(descriptor_,std::move(options));
}
SessionBridge::~SessionBridge(){stopping_=true;if(endpoint_)endpoint_->stop();}
Json SessionBridge::serviceStatus() const {
    return {{"protocol",coreProtocol},{"profile","text"},
            {"service",{{"state",stopping_.load()?"draining":"ready"}}},
            {"analysis",{{"supported",false}}}};
}
} // namespace cycode::text
