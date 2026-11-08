#include "cycode/CoreService.h"
#include <fstream>
#include <stdexcept>
namespace cycode {
void coreServiceChecks(const std::filesystem::path &root) {
    const auto left=root/"service-left",right=root/"service-right";
    std::filesystem::create_directories(left);std::filesystem::create_directories(right);
    {std::ofstream out(left/"main.cpp");out<<"int original;\n";}
    {std::ofstream out(right/"main.cpp");out<<"int other;\n";}
    Engine first(left),second(right);
    first.configureLanguage("",false,false);second.configureLanguage("",false,false);
    CoreService a(first),b(second);
    auto check=[](bool value,const char *message){if(!value)throw std::runtime_error(message);};
    auto call=[](CoreService &service,const char *method,const Json &params) {
        const auto response=service.respond({{"protocol",CoreService::protocol},{"id",7},{"method",method},{"params",params}});
        if(response.contains("error"))throw std::runtime_error(response.dump());
        if(response.at("id")!=7 || response.at("protocol")!=CoreService::protocol)throw std::runtime_error("Lost native request identity");
        return response.at("result");
    };
    check(a.status().at("engine").at("context")!=b.status().at("engine").at("context"),"Project contexts are not isolated");
    check(a.status().at("analysis").at("projectIndexCompleteness")=="unknown","Service inferred full semantic coverage");
    const auto disabled=first.languageStatus().at("servers").at("cpp");
    check(!disabled.at("available").get<bool>() && disabled.at("state")=="disabled","Disabled backend was reported as available");
    check(disabled.at("analysis").at("projectIndexCompleteness")=="unknown","Configured backend claimed semantic index completion");
    auto document=call(a,"documents.read",{{"path","main.cpp"}});
    call(a,"documents.replace",{{"path","main.cpp"},{"expectedRevision",document.at("revision")},{"text","int unsaved;\n"}});
    const auto stale=a.respond({{"id","stale"},{"method","documents.replace"},{"params",{{"path","main.cpp"},{"expectedRevision",document.at("revision")},{"text","overwrite"}}}});
    check(stale.at("error").at("code")=="RevisionConflict" && stale.at("id")=="stale","Typed document conflict lost its owner code or request identity");
    check(a.respond({{"method","unsupported.operation"}}).at("error").at("code")=="UnsupportedOperation","Unsupported operation lost its structured code");
    check(call(b,"documents.read",{{"path","main.cpp"}}).at("text")=="int other;\n","Project state leaked across Engines");
    check(a.respond(Json::array()).at("error").at("code")=="InvalidRequest","Invalid envelope was accepted");
    check(a.respond({{"protocol","cycode-core/999"},{"method","documents.read"}}).at("error").at("code")=="UnsupportedProtocol","Unsupported protocol was accepted");
    check(a.respond({{"method","documents.read"},{"params",Json::array()}}).at("error").at("code")=="InvalidParams","Non-object parameters were accepted");
    check(a.respond({{"method","documents.read"},{"params",{{"path","missing.cpp"}}}}).at("error").at("code")=="OperationFailed","Failure lost structured error");
    check(call(a,"documents.read",{{"path","main.cpp"}}).at("text")=="int unsaved;\n","Failure replaced the persistent Engine");
    a.beginShutdown();a.beginShutdown();
    check(a.status().at("service").at("state")=="draining","Shutdown did not drain");
    check(a.respond({{"method","documents.read"},{"params",{{"path","main.cpp"}}}}).at("error").at("code")=="ServiceStopping","Draining service accepted work");
    check(call(a,"service.status",Json::object()).at("service").at("state")=="draining","Shutdown status became unavailable");
    std::ifstream disk(left/"main.cpp");std::string text((std::istreambuf_iterator<char>(disk)),{});
    check(text=="int original;\n","Service shutdown saved dirty buffers");
}
} // namespace cycode
