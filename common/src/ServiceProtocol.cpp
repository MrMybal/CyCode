#include "cycode/ServiceProtocol.h"
#include <filesystem>
namespace cycode {
using Json=nlohmann::json;
const char *errorCodeName(ErrorCode code) noexcept {
    switch(code) {
#define CYCODE_ERROR_NAME(name) case ErrorCode::name:return #name;
    CYCODE_ERROR_NAME(InvalidRequest) CYCODE_ERROR_NAME(InvalidParams)
    CYCODE_ERROR_NAME(UnsupportedProtocol) CYCODE_ERROR_NAME(ServiceStopping)
    CYCODE_ERROR_NAME(RevisionConflict) CYCODE_ERROR_NAME(DiskConflict)
    CYCODE_ERROR_NAME(ReadOnly) CYCODE_ERROR_NAME(UnsupportedOperation)
    CYCODE_ERROR_NAME(BackendUnavailable) CYCODE_ERROR_NAME(Cancelled)
    CYCODE_ERROR_NAME(DeadlineExceeded) CYCODE_ERROR_NAME(CapacityExceeded)
    CYCODE_ERROR_NAME(FileSystemError) CYCODE_ERROR_NAME(OperationFailed) CYCODE_ERROR_NAME(InternalError)
#undef CYCODE_ERROR_NAME
    }
    return "InternalError";
}
Json nativeErrorResponse(const std::string &code,const std::string &message,const Json &id,const Json &details) {
    Json error={{"code",code},{"message",message}};
    if(!details.empty())error["details"]=details;
    return {{"protocol",coreProtocol},{"id",id},{"error",std::move(error)}};
}
std::string encodeNativeResponse(const Json &response) {
    auto payload=response.dump(-1,' ',false,Json::error_handler_t::replace)+"\n";
    if(payload.size()>maximumNativeResponseBytes)
        payload=nativeErrorResponse("ResponseTooLarge","Native response exceeds 20 MiB",response.is_object()?response.value("id",Json()):Json()).dump()+"\n";
    return payload;
}
Json respondNative(const Json &input,const NativeDispatch &dispatch) noexcept {
    Json id;
    try {
        auto invalid=[&](const char *message){return nativeErrorResponse("InvalidRequest",message);};
        if(!input.is_object())return invalid("Expected a request object");
        if(input.contains("id")) {
            const auto &supplied=input.at("id");
            if(!supplied.is_null() && !supplied.is_number_integer() && !(supplied.is_string() && supplied.get_ref<const std::string &>().size()<=256))
                return invalid("Expected a scalar request id of at most 256 bytes");
            id=supplied;
        }
        if(input.contains("protocol") && input.at("protocol")!=coreProtocol)
            return nativeErrorResponse("UnsupportedProtocol","Expected cycode-core/1",id);
        if(!input.contains("method") || !input.at("method").is_string())return nativeErrorResponse("InvalidRequest","Expected a string method",id);
        const auto method=input.at("method").get<std::string>();
        if(method.empty() || method.size()>128)return nativeErrorResponse("InvalidRequest","Invalid method length",id);
        const auto params=input.value("params",Json::object());
        if(!params.is_object())return nativeErrorResponse("InvalidParams","Expected an object for params",id);
        return {{"protocol",coreProtocol},{"id",id},{"result",dispatch(method,params)}};
    }catch(const NativeError &e){return nativeErrorResponse(errorCodeName(e.code()),e.what(),id,e.details());}
     catch(const Json::exception &e){return nativeErrorResponse("InvalidParams",e.what(),id);}
     catch(const std::filesystem::filesystem_error &e){return nativeErrorResponse("FileSystemError",e.what(),id);}
     catch(const std::invalid_argument &e){return nativeErrorResponse("InvalidParams",e.what(),id);}
     catch(const std::exception &e){return nativeErrorResponse("OperationFailed",e.what(),id);}
     catch(...){return nativeErrorResponse("InternalError","Unexpected native failure",id);}
}
} // namespace cycode
