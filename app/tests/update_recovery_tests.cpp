#include "../src/UpdateBudget.h"
#include "../src/UpdateFiles.h"
#include "cycode/Paths.h"
#include "cycode/UpdateRecovery.h"
#include "cycode/UpdateRecoveryProgress.h"
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
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
    check(rejected, "Unsafe update recovery accepted");
}
std::string read(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
void put(const fs::path &path, const std::string &bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}
} // namespace
void updateRecoveryContracts(const std::filesystem::path &root) {
#ifdef _WIN32
    using namespace cycode;
    using namespace cycode::updates;
    fs::path transaction;
    for (const auto &entry : fs::directory_iterator(root / "data/updates"))
        if (entry.is_directory() && fs::exists(entry.path() / "recovery.json"))
            transaction = entry.path();
    check(!transaction.empty(), "Apply failed to persist recovery inputs before moving originals");
    const auto planBytes = read(transaction / "recovery.json");
    const auto plan = Json::parse(planBytes);
    const auto pending = root / "data/updates/pending.json";
    const auto marker = Json{{"format", 2},
                             {"transaction", pathUtf8(transaction)},
                             {"version", "99.0.0"},
                             {"planSha256", sha256File(transaction / "recovery.json")}}
                            .dump();
    check(plan.at("original").contains("manifest.json") && !plan.at("original").contains("app/added.txt"),
          "Recovery plan forgot baseline presence");
    fs::create_directories(transaction / "backup");
    fs::rename(root / "CyCode.exe", transaction / "backup/CyCode.exe");
    fs::rename(root / "LICENSE", transaction / "backup/LICENSE");
    const auto newExecutable = read(transaction / "new/CyCode.exe");
    put(root / "CyCode.exe", newExecutable);
    put(root / "app/added.txt", read(transaction / "new/app/added.txt"));
    put(pending, marker);
    std::atomic<bool> cancel{false};
    auto review = reviewRecovery(root, cancel);
    check(!review.conflictCount && review.restore == 2 && review.remove == 1 &&
              read(root / "CyCode.exe") == newExecutable && !fs::exists(root / "LICENSE"),
          "Read-only recovery preview lost changes or mutated the installation");
    check(review.space.known && detail::allocationUnitValid(review.space.allocationUnit) &&
              review.space.copyFiles == 2 && review.space.retainedFiles == 2 &&
              review.space.payloadBytes == plan.at("original").at("CyCode.exe").at("size").get<uint64_t>() +
                                               plan.at("original").at("LICENSE").at("size").get<uint64_t>() &&
              review.space.requiredBytes ==
                  *detail::recoveryHeadroom(review.space.allocatedCopies, review.space.copyFiles,
                                            review.space.retainedFiles, review.space.directoryAllowances,
                                            review.space.allocationUnit),
          "Recovery estimate lost copied/retained ownership or allocation rounding");
    put(transaction / "recovery.json", planBytes + " ");
    rejects([&] { reviewRecovery(root, cancel); });
    put(transaction / "recovery.json", planBytes);
    cancel = true;
    RecoveryProgress cancelledProgress(review.planDigest, pathUtf8(review.transaction));
    bool cancelledType = false;
    try {
        recover(review, cancel, &cancelledProgress);
    } catch (const detail::Cancellation &) {
        cancelledType = true;
    }
    check(cancelledType && cancelledProgress.snapshot().phase == RecoveryPhase::cancelled &&
              !cancelledProgress.snapshot().installationChanged &&
              cancelledProgress.snapshot().copiedBytes == 0 && read(pending) == marker,
          "Preflight cancellation lost its typed result or changed installed state");
    cancel = false;
    put(root / "LICENSE", "External change after review");
    rejects([&] { recover(review, cancel); });
    check(read(root / "LICENSE") == "External change after review",
          "Stale recovery overwrote an external file");
    fs::remove(root / "LICENSE");
    put(root / "CyCode.exe", "Modified destination");
    check(reviewRecovery(root, cancel).conflictCount != 0, "Modified destination did not block recovery");
    put(root / "CyCode.exe", newExecutable);
    const auto oldLicense = read(transaction / "backup/LICENSE");
    put(transaction / "backup/LICENSE", "Corrupt backup");
    check(reviewRecovery(root, cancel).conflictCount != 0, "Corrupt backup did not block recovery");
    put(transaction / "backup/LICENSE", oldLicense);
    review = reviewRecovery(root, cancel);
    {
        NativeFileLease active(root / "data/updates/instances.lock", NativeFileLeaseMode::shared);
        rejects([&] { recover(review, cancel); });
    }
    fs::path changedPreparedPath;
    std::string changedPreparedBytes;
    // Future opt-in runtime coverage: hold the first replacement against rename.
    // All original copies must be ready before that refusal, with targets intact.
    {
        struct File {
            HANDLE handle = INVALID_HANDLE_VALUE;
            ~File() {
                if (handle != INVALID_HANDLE_VALUE)
                    CloseHandle(handle);
            }
        } busy;
        busy.handle = CreateFileW((root / "CyCode.exe").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(busy.handle != INVALID_HANDLE_VALUE, "Cannot reserve recovery destination fixture");
        auto alteredObservation = review;
        alteredObservation.space.known = false;
        alteredObservation.space.availableBytes = 0;
        alteredObservation.space.requiredBytes = ~uint64_t{0};
        RecoveryProgress failedProgress(review.planDigest, pathUtf8(review.transaction));
        rejects([&] { recover(alteredObservation, cancel, &failedProgress); });
        const auto failed = failedProgress.snapshot();
        check(failed.phase == RecoveryPhase::failed && failed.prepared == 2 && failed.prepareTotal == 2 &&
                  failed.completed == 0 && failed.total == 3 && !failed.installationChanged &&
                  failed.copiedBytes == review.space.payloadBytes,
              "Failed recovery progress reported completion/mutation or charged reused bytes");
        rejects([&] { recover(review, cancel, &failedProgress); });
        RecoveryProgress foreignProgress("foreign-plan", pathUtf8(review.transaction));
        rejects([&] { recover(review, cancel, &foreignProgress); });
        check(!foreignProgress.claimed, "Foreign progress was claimed by this transaction");
        const auto status = Json::parse(read(transaction / "recovery-status.json"));
        const auto preparedCopies = utf8Path(status.at("files").get<std::string>());
        const auto failedReceipt = Json::parse(read(preparedCopies / "recovery-result.json"));
        const auto failedReport = Json::parse(recoveryReport(review, failed));
        check(failedReceipt.at("state") == "failed" && failedReceipt.at("phase") == "restoring" &&
                  failedReceipt.at("pendingCleared") == false &&
                  failedReceipt.at("installationChanged") == false && failedReport.at("phase") == "failed" &&
                  failedReport.at("planSha256") == review.planDigest && !failedReport.contains("error") &&
                  !failedReport.contains("environment"),
              "Recovery receipt/report lost its actual failure stage or included raw diagnostic data");
        check(status.at("phase") == "preparation-failed" && fs::exists(preparedCopies / "space.json") &&
                  fs::exists(preparedCopies / "space-before-commit.json") &&
                  sha256File(preparedCopies / "restored/CyCode.exe") ==
                      plan.at("original").at("CyCode.exe").at("sha256").get<std::string>() &&
                  sha256File(preparedCopies / "restored/LICENSE") ==
                      plan.at("original").at("LICENSE").at("sha256").get<std::string>() &&
                  read(root / "CyCode.exe") == newExecutable && !fs::exists(root / "LICENSE") &&
                  read(pending) == marker && reviewRecovery(root, cancel).baseline == review.baseline,
              "Recovery moved targets before staging all copies or trusted supplied space observations");
        const auto firstSpace = Json::parse(read(preparedCopies / "space.json"));
        const auto commitSpace = Json::parse(read(preparedCopies / "space-before-commit.json"));
        check(firstSpace.at("required").get<uint64_t>() - commitSpace.at("required").get<uint64_t>() ==
                      firstSpace.at("allocatedCopies").get<uint64_t>() &&
                  firstSpace.at("copiesPrepared") == false && commitSpace.at("copiesPrepared") == true &&
                  commitSpace.at("planSha256") == review.planDigest,
              "Recovery commit budget charged prepared copies again or lost plan binding");
        const auto resume = reviewRecovery(root, cancel);
        check(resume.space.known && resume.space.reusedFiles == 2 &&
                  resume.space.reusedAllocation == resume.space.allocatedCopies &&
                  resume.space.requiredBytes + resume.space.reusedAllocation == review.space.requiredBytes &&
                  resume.baseline == review.baseline,
              "Verified prepared copies were not reused or changed installation consent baseline");
        const auto oldStatus = read(transaction / "recovery-status.json");
        auto phaseOnly = Json::parse(oldStatus);
        phaseOnly["phase"] = "complete";
        put(transaction / "recovery-status.json", phaseOnly.dump());
        const auto phaseReview = reviewRecovery(root, cancel);
        check(phaseReview.restore == 2 && phaseReview.remove == 1 && phaseReview.space.reusedFiles == 2,
              "Mutable completion phase bypassed actual recovery file validation");
        put(transaction / "recovery-status.json", oldStatus);
        rejects([&] { recover(resume, cancel); });
        const auto secondStatus = Json::parse(read(transaction / "recovery-status.json"));
        const auto secondWork = utf8Path(secondStatus.at("files").get<std::string>());
        const auto secondSpace = Json::parse(read(secondWork / "space.json"));
        const auto secondMap = Json::parse(read(secondWork / "prepared.json"));
        check(secondSpace.at("reusedFiles") == 2 && !fs::exists(secondWork / "restored/CyCode.exe") &&
                  !fs::exists(secondWork / "restored/LICENSE") &&
                  secondMap.at("files").at("CyCode.exe") == pathUtf8(preparedCopies.filename()) &&
                  read(root / "CyCode.exe") == newExecutable && read(pending) == marker,
              "Resume recopied verified payloads or lost its source map before a locked target");
        const auto oldPreparedLicense = read(preparedCopies / "restored/LICENSE");
        const std::string changedCopy(oldPreparedLicense.size(), 'x');
        put(preparedCopies / "restored/LICENSE", changedCopy);
        changedPreparedPath = preparedCopies / "restored/LICENSE";
        changedPreparedBytes = changedCopy;
        const auto partial = reviewRecovery(root, cancel);
        check(partial.space.reusedFiles == 1 && partial.space.preparedIgnored &&
                  partial.space.requiredBytes > resume.space.requiredBytes && !partial.conflictCount,
              "Changed prepared payload was trusted or blocked valid backup-based preparation");
        rejects([&] { recover(partial, cancel); });
        const auto thirdStatus = Json::parse(read(transaction / "recovery-status.json"));
        const auto thirdWork = utf8Path(thirdStatus.at("files").get<std::string>());
        check(fs::exists(thirdWork / "restored/LICENSE") && !fs::exists(thirdWork / "restored/CyCode.exe") &&
                  read(thirdWork / "restored/LICENSE") == oldLicense &&
                  read(preparedCopies / "restored/LICENSE") == changedCopy,
              "Partial resume copied valid payload again or overwrote a changed earlier copy");
        const auto ownerBytes = read(preparedCopies / "work.json");
        auto foreignOwner = Json::parse(ownerBytes);
        foreignOwner["transaction"] = pathUtf8(root);
        put(preparedCopies / "work.json", foreignOwner.dump());
        const auto foreign = reviewRecovery(root, cancel);
        check(foreign.space.reusedFiles == 1 && foreign.space.preparedIgnored && !foreign.conflictCount,
              "Foreign prepared directory identity was accepted");
        put(preparedCopies / "work.json", ownerBytes);
        fs::rename(preparedCopies / "work.json", preparedCopies / "work.held.json");
        const auto legacy = reviewRecovery(root, cancel);
        check(legacy.space.reusedFiles == 1 && legacy.space.preparedIgnored,
              "A prepared directory without current identity metadata was reused");
        fs::rename(preparedCopies / "work.held.json", preparedCopies / "work.json");
        const auto mapBytes = read(thirdWork / "prepared.json");
        auto unsafeMap = Json::parse(mapBytes);
        unsafeMap["files"]["LICENSE"] = "../escape";
        put(thirdWork / "prepared.json", unsafeMap.dump());
        const auto unsafe = reviewRecovery(root, cancel);
        check(unsafe.space.reusedFiles == 0 && unsafe.space.preparedIgnored && !unsafe.conflictCount,
              "Traversal in a preparation map was followed");
        put(thirdWork / "prepared.json", mapBytes);
        const auto alias = thirdWork / "copy-alias.data";
        fs::create_hard_link(thirdWork / "restored/LICENSE", alias);
        check(reviewRecovery(root, cancel).space.reusedFiles == 1 && read(alias) == oldLicense,
              "Hard-linked prepared file was reused or modified");
        fs::remove(alias);
        fs::rename(transaction / "backup/LICENSE", transaction / "backup/LICENSE.held");
        check(reviewRecovery(root, cancel).conflictCount != 0 && fs::exists(thirdWork / "restored/LICENSE"),
              "Prepared copy bypassed the requirement to retain a verified original backup");
        fs::rename(transaction / "backup/LICENSE.held", transaction / "backup/LICENSE");
        review = reviewRecovery(root, cancel);
        check(review.space.reusedFiles == 2, "Restored metadata lost a valid mixed preparation map");
    }
    // An interrupted recovery may already have restored an original while its
    // backup remains. A fresh preview must accept this state and finish safely.
    put(root / "CyCode.exe", read(transaction / "backup/CyCode.exe"));
    review = reviewRecovery(root, cancel);
    check(review.restore == 1 && !review.conflictCount,
          "Already-restored original made recovery non-resumable");
    check(review.space.reusedFiles == 1 && review.space.copyFiles == 1,
          "Already-restored files were charged or moved as prepared-copy work");
    RecoveryProgress completedProgress(review.planDigest, pathUtf8(review.transaction));
    recover(review, cancel, &completedProgress);
    const auto completed = completedProgress.snapshot();
    check(completed.phase == RecoveryPhase::complete && completed.prepared == 1 &&
              completed.prepareTotal == 1 && completed.completed == 2 && completed.total == 2 &&
              completed.installationChanged && completed.copiedBytes == 0,
          "Completed recovery progress lost reused-copy or postflight completion semantics");
    const auto completedStatus = Json::parse(read(transaction / "recovery-status.json"));
    const auto completedWork = utf8Path(completedStatus.at("files").get<std::string>());
    const auto completedReceipt = Json::parse(read(completedWork / "recovery-result.json"));
    check(completedReceipt.at("state") == "restored" && completedReceipt.at("pendingCleared") == true &&
              completedReceipt.at("phase") == "complete" && completedReceipt.at("copiedBytes") == 0,
          "Recovery receipt claimed success before full postflight and pending removal");
    check(!fs::exists(pending) && fs::exists(transaction / "backup/CyCode.exe") &&
              fs::exists(transaction / "backup/LICENSE") && !fs::exists(root / "app/added.txt") &&
              read(root / "LICENSE") == oldLicense && fs::exists(root / "data/settings/user.json") &&
              read(changedPreparedPath) == changedPreparedBytes &&
              fs::exists(root / "extensions/user.plugin"),
          "Recovery lost backup/user data or baseline absence");
    rejects([&] { recover(review, cancel); });
    put(pending, Json{{"transaction", pathUtf8(transaction)}, {"version", "99.0.0"}}.dump());
    rejects([&] { reviewRecovery(root, cancel); });
    fs::remove(pending);
#else
    (void)root;
#endif
}
