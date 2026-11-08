#include "cycode/ProcessText.h"
#include "cycode/TestRunner.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <set>
namespace cycode {
namespace {
std::string ctest() {
    auto program = toolPath("ctest");
    if (!program.empty())
        return program;
    const auto cmake = toolPath("cmake");
    if (!cmake.empty()) {
#ifdef _WIN32
        const auto sibling = utf8Path(cmake).parent_path() / "ctest.exe";
#else
        const auto sibling = utf8Path(cmake).parent_path() / "ctest";
#endif
        if (std::filesystem::is_regular_file(sibling))
            return pathUtf8(sibling);
    }
    return "ctest";
}
} // namespace
TestRunner::Json TestRunner::catalog(const std::filesystem::path &directory, const std::string &configuration,
                                     const std::atomic<bool> *cancelled) {
    const auto result =
        runProcess({ctest(), "--test-dir", pathUtf8(directory), "-C", configuration, "--show-only=json-v1"},
                   root_, std::chrono::seconds(30), 4 * 1024 * 1024, cancelled);
    if (result.exitCode || result.truncated || result.timedOut || result.cancelled)
        throw std::runtime_error("CTest discovery failed: " + result.output);
    const auto data = Json::parse(result.output);
    if (!data.at("tests").is_array() || data.at("tests").size() > 5000)
        throw std::runtime_error("CTest catalogue exceeds 5000 tests");
    return data;
}
TestRunner::Json TestRunner::discover(const Json &params) {
    std::unique_lock operation(operation_, std::try_to_lock);
    if (!operation.owns_lock())
        throw std::runtime_error("A test operation is already running");
    const auto directory = resolve_(params.value("buildDirectory", "build"));
    const auto configuration = params.value("configuration", "Debug");
    if (configuration.empty() || configuration.size() > 128 || configuration.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid test configuration");
    auto token = std::make_shared<std::atomic<bool>>(false);
    {
        std::lock_guard lock(state_);
        cancellation_ = token;
        operationId_ = params.value("operationId", "");
        issued_ = Json();
    }
    struct Finish {
        TestRunner *owner;
        ~Finish() {
            std::lock_guard lock(owner->state_);
            owner->cancellation_.reset();
        }
    } finish{this};
    const auto raw = catalog(directory, configuration, token.get());
    Json tests = Json::array();
    size_t index = 0;
    for (const auto &t : raw.at("tests")) {
        Json labels = Json::array();
        bool disabled = false;
        std::string cwd;
        for (const auto &p : t.value("properties", Json::array())) {
            const auto name = p.value("name", "");
            if (name == "LABELS")
                labels = p.at("value");
            if (name == "DISABLED")
                disabled = p.at("value") == true;
            if (name == "WORKING_DIRECTORY")
                cwd = p.at("value").get<std::string>();
        }
        tests.push_back({{"index", index++},
                         {"name", t.at("name")},
                         {"labels", labels},
                         {"disabled", disabled},
                         {"directory", cwd}});
    }
    if (token->load())
        throw std::runtime_error("Test discovery cancelled");
    Json result = {{"id", "tests-" + std::to_string(next_++)},
                   {"buildDirectory", pathUtf8(directory.lexically_relative(root_))},
                   {"configuration", configuration},
                   {"tests", tests}};
    issued_ = {{"public", result}, {"raw", raw}};
    return result;
}
TestRunner::Json TestRunner::run(const Json &params) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    std::unique_lock operation(operation_, std::try_to_lock);
    if (!operation.owns_lock())
        throw std::runtime_error("A test operation is already running");
    if (issued_.is_null() || issued_.at("public").at("id") != params.at("id"))
        throw std::runtime_error("Unknown test catalogue; discover tests again");
    const auto &info = issued_.at("public");
    const auto directory = resolve_(info.at("buildDirectory"));
    const auto configuration = info.at("configuration").get<std::string>();
    const auto &chosen = params.at("tests");
    if (!chosen.is_array() || chosen.empty() || chosen.size() > 128)
        throw std::runtime_error("Select 1..128 issued tests");
    std::set<size_t> indices;
    for (const auto &v : chosen) {
        if (!v.is_number_integer())
            throw std::runtime_error("Invalid test index");
        const auto n = v.get<int64_t>();
        if (n < 0 || size_t(n) >= info.at("tests").size() || !indices.insert(size_t(n)).second)
            throw std::runtime_error("Unknown or duplicate test index");
    }
    const int seconds = params.value("timeoutSeconds", 60);
    if (seconds < 1 || seconds > 120)
        throw std::runtime_error("Test timeout must be 1..120 seconds");
    auto token = std::make_shared<std::atomic<bool>>(false);
    {
        std::lock_guard lock(state_);
        cancellation_ = token;
        operationId_ = params.value("operationId", "");
    }
    struct Finish {
        TestRunner *owner;
        ~Finish() {
            std::lock_guard lock(owner->state_);
            owner->cancellation_.reset();
        }
    } finish{this};
    if (catalog(directory, configuration, token.get()) != issued_.at("raw"))
        throw std::runtime_error("CTest definitions changed; discover tests again");
    Json results = Json::array();
    bool failed = false, truncated = false, timedOut = false;
    size_t bytes = 0;
    for (auto i : indices) {
        if (token->load())
            break;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            timedOut = true;
            failed = true;
            break;
        }
        const auto &test = info.at("tests").at(i);
        Json row = {{"index", i},   {"name", test.at("name")}, {"status", "disabled"}, {"exitCode", 0},
                    {"output", ""}, {"milliseconds", 0}};
        if (!test.at("disabled").get<bool>()) {
            const auto number = std::to_string(i + 1);
            const auto start = std::chrono::steady_clock::now();
            const auto r = runProcess(
                {ctest(), "--test-dir", pathUtf8(directory), "-C", configuration, "--no-tests=error",
                 "--output-on-failure", "--timeout", std::to_string(seconds), "-I", number + "," + number},
                root_,
                std::min(remaining, std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::seconds(seconds + 10))),
                65536, token.get());
            row.update({{"status", r.cancelled   ? "cancelled"
                                   : r.timedOut  ? "timeout"
                                   : r.truncated ? "output-limit"
                                   : r.exitCode  ? "failed"
                                                 : "passed"},
                        {"exitCode", r.exitCode},
                        {"output", processOutputText(r.output, true)},
                        {"milliseconds", std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now() - start)
                                             .count()}});
            failed = failed || r.exitCode != 0 || r.cancelled || r.timedOut || r.truncated;
            timedOut = timedOut || r.timedOut;
            truncated = truncated || r.truncated;
            bytes += r.output.size();
        }
        results.push_back(row);
        if (bytes >= 2 * 1024 * 1024) {
            truncated = true;
            failed = true;
            break;
        }
    }
    for (auto i : indices)
        if (std::none_of(results.begin(), results.end(), [&](const auto &r) { return r.at("index") == i; }))
            results.push_back({{"index", i},
                               {"name", info.at("tests").at(i).at("name")},
                               {"status", "not-run"},
                               {"exitCode", -1},
                               {"output", ""},
                               {"milliseconds", 0}});
    return {{"id", info.at("id")},  {"results", results},     {"cancelled", token->load()},
            {"timedOut", timedOut}, {"truncated", truncated}, {"exitCode", failed || token->load() ? 1 : 0}};
}
TestRunner::Json TestRunner::cancel(const Json &params) {
    std::lock_guard lock(state_);
    const bool matches =
        cancellation_ && (!params.contains("operationId") || params.at("operationId") == operationId_);
    if (matches)
        cancellation_->store(true);
    return {{"requested", matches}};
}
} // namespace cycode
