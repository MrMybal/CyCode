#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace cycode {
enum class ErrorCode {
    InvalidRequest,InvalidParams,UnsupportedProtocol,ServiceStopping,
    RevisionConflict,DiskConflict,ReadOnly,UnsupportedOperation,
    BackendUnavailable,Cancelled,DeadlineExceeded,CapacityExceeded,
    FileSystemError,OperationFailed,InternalError
};
const char *errorCodeName(ErrorCode) noexcept;
class NativeError:public std::runtime_error {
  public:
    NativeError(ErrorCode code,std::string message,nlohmann::json details=nlohmann::json::object())
        :std::runtime_error(std::move(message)),code_(code),details_(std::move(details)){}
    ErrorCode code() const noexcept {return code_;}
    const nlohmann::json &details() const noexcept {return details_;}
  private:
    ErrorCode code_;
    nlohmann::json details_;
};
} // namespace cycode
