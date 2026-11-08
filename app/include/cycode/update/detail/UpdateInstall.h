#pragma once
#include "cycode/Updates.h"

namespace cycode::updates {
enum class InstallFailure { none, preparation, space, rolledBack, recoveryRequired };
class InstallationError : public std::runtime_error {
  public:
    InstallationError(InstallFailure failure, const std::string &message)
        : std::runtime_error(message), failure_(failure) {}
    InstallFailure failure() const noexcept {
        return failure_;
    }

  private:
    InstallFailure failure_;
};
namespace detail {
constexpr std::string_view installFailureCode(InstallFailure failure) {
    switch (failure) {
    case InstallFailure::preparation:
        return "preparation";
    case InstallFailure::space:
        return "space";
    case InstallFailure::rolledBack:
        return "rolled-back";
    case InstallFailure::recoveryRequired:
        return "recovery-required";
    default:
        return "";
    }
}
constexpr InstallFailure installFailure(std::string_view code) {
    if (code == "preparation")
        return InstallFailure::preparation;
    if (code == "space")
        return InstallFailure::space;
    if (code == "rolled-back")
        return InstallFailure::rolledBack;
    if (code == "recovery-required")
        return InstallFailure::recoveryRequired;
    return InstallFailure::none;
}
constexpr std::string_view installFailureLabel(InstallFailure failure) {
    switch (failure) {
    case InstallFailure::preparation:
        return "Update preparation failed before replacing installed files.";
    case InstallFailure::space:
        return "The update was blocked by available disk space before replacement.";
    case InstallFailure::rolledBack:
        return "The update failed and the previous installation was verified after rollback.";
    case InstallFailure::recoveryRequired:
        return "The update stopped and requires recovery. Keep pending.json and all backups.";
    default:
        return "The application update could not be completed.";
    }
}
} // namespace detail
} // namespace cycode::updates
