#include "cycode/Engine.h"
#include <fstream>
#include <future>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
static void check(bool v, const std::string &message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F f, const char *message) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const auto root =
        fs::temp_directory_path() /
        ("cycode-ctest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    bool success = false;
    try {
        const auto cmake = std::string(argv[1]);
        auto write = [&](const std::string &suffix = "") {
            std::ofstream out(root / "CMakeLists.txt");
            out << "cmake_minimum_required(VERSION 3.24)\nproject(Tests "
                   "NONE)\nenable_testing()\nadd_test(NAME passing COMMAND \"${CMAKE_COMMAND}\" -E "
                   "true)\nadd_test(NAME failing COMMAND \"${CMAKE_COMMAND}\" -E false)\nadd_test(NAME "
                   "delayed COMMAND \"${CMAKE_COMMAND}\" -E sleep 10)\nset_tests_properties(passing "
                   "PROPERTIES LABELS \"unit;fast\")\n"
                << suffix;
        };
        write();
        auto configure = [&] {
            const auto r = runProcess({cmake, "-S", pathUtf8(root), "-B", pathUtf8(root / "build")}, root);
            check(!r.exitCode, r.output);
        };
        configure();
        {
            Engine engine(root);
            engine.configureLanguage("", false, false);
            auto discover = [&] { return engine.dispatch("tests.discover"); };
            auto cat = discover();
            check(cat.at("tests").size() == 3, "Actual CTest discovery");
            auto run = [&](Json selection) {
                return engine.dispatch("tests.run", {{"id", cat.at("id")}, {"tests", selection}});
            };
            auto result = run({0, 1});
            check(result.at("results")[0].at("status") == "passed" &&
                      result.at("results")[1].at("status") == "failed" && result.at("exitCode") == 1,
                  "Per-test results: " + result.dump());
            rejects([&] { run({0, 0}); }, "Duplicate test selection accepted");
            auto active = std::async(std::launch::async, [&] { return run({2}); });
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            check(engine.dispatch("tests.cancel").at("requested"), "No active cancellation token");
            check(active.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
                  "Cancellation did not stop native tree");
            check(active.get().at("cancelled"), "Run did not report cancellation");
            write("add_test(NAME added COMMAND \"${CMAKE_COMMAND}\" -E true)\n");
            configure();
            rejects([&] { run({0}); }, "Changed CTest catalogue accepted");
            cat = discover();
            check(cat.at("tests").size() == 4, "Refreshed test catalogue");
            const auto doc = engine.dispatch("documents.read", {{"path", "CMakeLists.txt"}});
            engine.dispatch(
                "documents.replace",
                {{"path", "CMakeLists.txt"}, {"expectedRevision", doc.at("revision")}, {"text", "unsaved"}});
            rejects([&] { run({0}); }, "Dirty project test run accepted");
        }
        success = true;
        std::cout << "Actual CTest discovery, selected results, cancellation and stale/dirty guards passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
    if (success) {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    return success ? 0 : 1;
}
