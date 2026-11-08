#include "../src/UpdateBudget.h"
#include "cycode/update/detail/UpdateFiles.h"
#include "cycode/update/detail/UpdateInstall.h"
#include "cycode/UpdateRecoveryProgress.h"
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using namespace cycode::updates;
using namespace cycode::updates::detail;
namespace fs = std::filesystem;
constexpr uint64_t unit = 4096;
static_assert(allocationUnitValid(512) && allocationUnitValid(16ull * 1024 * 1024));
static_assert(!allocationUnitValid(0) && !allocationUnitValid(511));
static_assert(!allocationUnitValid(513) && !allocationUnitValid(32ull * 1024 * 1024));
static_assert(allocatedBytes(0, unit) == 0);
static_assert(allocatedBytes(1, unit) == unit);
static_assert(allocatedBytes(unit, unit) == unit);
static_assert(allocatedBytes(unit + 1, unit) == 2 * unit);
static_assert(allocatedBytes(maximumPackage, unit) == maximumPackage);
static_assert(!allocatedBytes(maximumPackage + 2ull * 1024 * 1024 + 1, unit));
static_assert(!allocatedBytes(1, 3));
static_assert(installationHeadroom(2 * unit, 2, 6, unit) ==
              2 * unit + (2 + 6 + 64) * unit + installMetadataReserve + installSpare);
static_assert(*installationHeadroom(2 * unit, 2, 6, unit) - *installationHeadroom(0, 2, 6, unit) == 2 * unit);
static_assert(!installationHeadroom(0, 0, 0, unit));
static_assert(!installationHeadroom(0, 4097, 0, unit));
static_assert(!installationHeadroom(0, 1, 5000001, unit));
static_assert(!installationHeadroom(0, 1, 0, 0));
static_assert(!installationHeadroom(maximumPackage + unit + 1, 1, 0, unit));
static_assert(installationHeadroom(maximumPackage + 4096 * unit, 4096, 5000000, unit).has_value());
static_assert(installFailure(installFailureCode(InstallFailure::preparation)) ==
                  InstallFailure::preparation &&
              installFailure(installFailureCode(InstallFailure::space)) == InstallFailure::space);
static_assert(installFailure(installFailureCode(InstallFailure::rolledBack)) == InstallFailure::rolledBack &&
              installFailure(installFailureCode(InstallFailure::recoveryRequired)) ==
                  InstallFailure::recoveryRequired);
static_assert(installFailure("") == InstallFailure::none &&
              installFailure("unknown") == InstallFailure::none &&
              installFailureCode(InstallFailure::none).empty());
static_assert(recoveryHeadroom(2 * unit, 2, 2, 6, unit) ==
              2 * unit + (2 + 2 + 6 + 64) * unit + installMetadataReserve + installSpare);
static_assert(*recoveryHeadroom(2 * unit, 2, 2, 6, unit) - *recoveryHeadroom(0, 2, 2, 6, unit) == 2 * unit);
static_assert(recoveryHeadroom(0, 0, 0, 0, unit) == 64 * unit + installMetadataReserve + installSpare);
static_assert(recoveryHeadroom(maximumPackage + 2ull * 1024 * 1024 + 4096 * unit, 4096, 4096, 5000000, unit)
                  .has_value());
static_assert(!recoveryHeadroom(0, 0, 0, 0, 0));
static_assert(!recoveryHeadroom(0, 0, 0, 0, 513));
static_assert(!recoveryHeadroom(0, 4097, 0, 0, unit));
static_assert(!recoveryHeadroom(0, 0, 4097, 0, unit));
static_assert(!recoveryHeadroom(0, 0, 0, 5000001, unit));
static_assert(!recoveryHeadroom(maximumPackage + 2ull * 1024 * 1024 + unit + 1, 1, 0, 0, unit));
static_assert(!recoveryHeadroom(1, 0, 0, 0, unit));
static_assert(!recoveryHeadroom(~uint64_t{0}, 1, 1, 0, unit));
static_assert(!recoveryHeadroom(0, ~uint64_t{0}, ~uint64_t{0}, 0, unit));
static_assert(!recoveryHeadroom(0, 1, 1, ~uint64_t{0}, unit));
static_assert(recoveryHeadroom(0, 0, 0, 0, 16ull * 1024 * 1024).has_value());
static_assert(*recoveryHeadroom(0, 1, 1, 0, unit) - *recoveryHeadroom(0, 1, 0, 0, unit) == unit);
static_assert(recoveryResumeHeadroom(2 * unit, 0, 2, 0, 2, 6, unit) ==
              recoveryHeadroom(2 * unit, 2, 2, 6, unit));
static_assert(*recoveryResumeHeadroom(2 * unit, unit, 2, 1, 2, 6, unit) ==
              *recoveryHeadroom(2 * unit, 2, 2, 6, unit) - unit);
static_assert(recoveryResumeHeadroom(2 * unit, 2 * unit, 2, 2, 2, 6, unit) ==
              recoveryHeadroom(0, 2, 2, 6, unit));
static_assert(!recoveryResumeHeadroom(unit, 2 * unit, 1, 1, 0, 0, unit));
static_assert(!recoveryResumeHeadroom(unit, unit, 1, 2, 0, 0, unit));
static_assert(!recoveryResumeHeadroom(unit, unit, 1, 0, 0, 0, unit));
static_assert(!recoveryResumeHeadroom(~uint64_t{0}, ~uint64_t{0}, 1, 1, 0, 0, unit));
static_assert(!recoveryResumeHeadroom(0, 0, 4097, 0, 0, 0, unit));
static_assert(!recoveryResumeHeadroom(0, 0, 0, 0, 0, 0, 3));
static_assert(recoveryResumeHeadroom(0, 0, 0, 0, 0, 0, unit) == recoveryHeadroom(0, 0, 0, 0, unit));
static_assert(recoveryResumeHeadroom(0, 0, 1, 1, 0, 0, unit) == recoveryHeadroom(0, 1, 0, 0, unit));
static_assert(recoveryResumeHeadroom(maximumPackage + 2ull * 1024 * 1024 + 4096 * unit,
                                     maximumPackage + 2ull * 1024 * 1024 + 4096 * unit, 4096, 4096, 4096,
                                     5000000, unit) == recoveryHeadroom(0, 4096, 4096, 5000000, unit));
static_assert(!RecoverySnapshot{}.percent());
static_assert(!RecoverySnapshot{RecoveryPhase::preparing}.percent());
static_assert(RecoverySnapshot{RecoveryPhase::preparing, 0, 2}.percent() == 0);
static_assert(RecoverySnapshot{RecoveryPhase::preparing, 1, 2}.percent() == 50);
static_assert(RecoverySnapshot{RecoveryPhase::preparing, 2, 2}.percent() == 100);
static_assert(!RecoverySnapshot{RecoveryPhase::preparing, 3, 2}.percent());
static_assert(!RecoverySnapshot{RecoveryPhase::preparing, 1, 4097}.percent());
static_assert(RecoverySnapshot{RecoveryPhase::restoring, 0, 0, 1, 3}.percent() == 33);
static_assert(!RecoverySnapshot{RecoveryPhase::restoring}.percent());
static_assert(!RecoverySnapshot{RecoveryPhase::restoring, 0, 0, 1, 8193}.percent());
static_assert(!RecoverySnapshot{RecoveryPhase::checking, 2, 2, 3, 3}.percent());
static_assert(RecoverySnapshot{RecoveryPhase::complete}.percent() == 100);
static_assert(!RecoverySnapshot{RecoveryPhase::cancelled}.percent());
static_assert(!RecoverySnapshot{RecoveryPhase::failed}.percent());
static_assert(recoveryPhaseCode(RecoveryPhase::inspecting) == "inspecting" &&
              recoveryPhaseCode(RecoveryPhase::preparing) == "preparing" &&
              recoveryPhaseCode(RecoveryPhase::verifying) == "verifying" &&
              recoveryPhaseCode(RecoveryPhase::restoring) == "restoring");
static_assert(recoveryPhaseCode(RecoveryPhase::checking) == "checking" &&
              recoveryPhaseCode(RecoveryPhase::complete) == "complete" &&
              recoveryPhaseCode(RecoveryPhase::cancelled) == "cancelled" &&
              recoveryPhaseCode(RecoveryPhase::failed) == "failed");
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Unsafe update marker removal accepted");
}
} // namespace

// Opt-in native coverage only. Ordinary builds and source checks never execute
// this body, create its files, fill a disk or launch an update helper.
void updateInstallContracts(const std::filesystem::path &parent) {
#ifdef _WIN32
    const auto directory = fs::absolute(parent / "update-marker-policy");
    fs::create_directories(directory);
    check(allocationUnitValid(allocationUnit(directory)), "Invalid observed volume geometry");
    (void)availableSpace(directory);
    const auto marker = directory / "pending.json", alias = directory / "alias.json";
    const std::string baseline = "owned-marker";
    auto put = [&](const std::string &bytes) { std::ofstream(marker, std::ios::binary) << bytes; };
    auto retained = [&](const std::string &bytes) {
        check(read(marker, 8192) == bytes, "Marker changed after refusal");
    };
    put(baseline);
    removeOwnedText(marker, baseline);
    check(!fs::exists(marker), "Verified owned marker was retained");
    put("other-marker");
    rejects([&] { removeOwnedText(marker, baseline); });
    retained("other-marker");
    removeOwnedText(marker, "other-marker");
    put(baseline);
    fs::create_hard_link(marker, alias);
    rejects([&] { removeOwnedText(marker, baseline); });
    retained(baseline);
    check(read(alias, 8192) == baseline, "Marker alias was changed");
    fs::remove(alias);
    struct File {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ~File() {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    } writer;
    writer.handle =
        CreateFileW(marker.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(writer.handle != INVALID_HANDLE_VALUE, "Cannot reserve marker writer fixture");
    rejects([&] { removeOwnedText(marker, baseline); });
    retained(baseline);
    CloseHandle(writer.handle);
    writer.handle = INVALID_HANDLE_VALUE;
    removeOwnedText(marker, baseline);
    rejects([&] { removeOwnedText(marker, baseline); });
    check(!fs::exists(marker), "Missing marker removal recreated a file");
#else
    (void)parent;
#endif
}
