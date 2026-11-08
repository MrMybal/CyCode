#include "cycode/ProcessText.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "cycode/BuildOperations.h"
#include "cycode/TaskOutputText.h"
#include <future>
using namespace cycode;
using Json = nlohmann::json;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejected(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid build output input was accepted");
}
} // namespace
void buildOutputContracts() {
    const std::string unicode = "été \xf0\x9f\x98\x80\n";
    require(processOutputText(unicode) == unicode && processOutputText(unicode, true) == unicode,
            "Valid UTF-8 process output was transcoded");
    const std::string zero("a\0b", 3);
    require(processOutputText(zero) == zero, "Display conversion changed protocol/control bytes in its input copy");
    const std::string malformed("bad-\x82-\xc0\xaf-\xed\xa0\x80-\xf0\x9f", 15);
    const auto retained = malformed;
    const auto repaired = processOutputText(malformed);
    require(malformed == retained && repaired.find("\xef\xbf\xbd") != std::string::npos,
            "Malformed display output lost its replacement marker or changed raw bytes");
    require(Json::parse(Json(repaired).dump()).get<std::string>() == repaired,
            "Repaired process display output cannot cross JSON");
#ifdef _WIN32
    BOOL substituted = FALSE;
    const auto codepage = GetOEMCP();
    const wchar_t source[] = L"été\n";
    auto defaults = codepage == CP_UTF8 ? nullptr : &substituted;
    const auto count = WideCharToMultiByte(codepage, 0, source, 4, nullptr, 0, nullptr, defaults);
    require(count > 0, "Cannot prepare Windows console encoding fixture");
    std::string oem(size_t(count), '\0');
    require(WideCharToMultiByte(codepage, 0, source, 4, oem.data(), count, nullptr, defaults) == count,
            "Cannot encode Windows console fixture");
    if (!substituted) {
        require(processOutputText(oem, true) == "été\n", "Legacy console path lost accented characters");
        require(processOutputText(unicode + oem, true) == unicode + "été\n",
                "Legacy console line corrupted an adjacent UTF-8 line");
    }
#endif
    TaskOutput journal;
    const auto stream =
        journal.begin("project.build", "reused", {{"system", "cargo"}, {"directory", "rust"}});
    const auto before = journal.read();
    journal.context(stream, {{"system", "cargo"}, {"directory", "rust/member"}});
    const auto context = journal.read({{"streamId", stream}, {"revision", before.at("revision")}});
    require(context.at("changed") && context.at("context").at("directory") == "rust/member",
            "Task context was not revisioned");
    TaskOutputText cargo(journal.observer(stream), true);
    auto append = cargo.observer();
    const auto diagnostic = Json{{"reason", "compiler-message"},
                                 {"message", {{"rendered", "error: été\n --> src/main.rs:2:3\n"}}}}
                                .dump();
    for (const auto c : diagnostic)
        append(std::string_view(&c, 1));
    append("\n");
    append("{\"reason\":\"compiler-artifact\",\"executable\":\"private-artifact\"}\n");
    append("ordinary stderr\n");
    append(std::string(65537, 'x'));
    append("\ntrailing fragment");
    cargo.flush();
    cargo.flush();
    const auto display = journal.read().at("output").get<std::string>();
    require(display.find("error: été") != std::string::npos &&
                display.find("private-artifact") == std::string::npos &&
                display.find("oversized Cargo protocol line omitted") != std::string::npos &&
                display.ends_with("trailing fragment\n"),
            "Cargo live framing leaked protocol or lost a message");
    auto stale = journal.observer(stream);
    journal.finish();
    const auto nextStream = journal.begin("project.build", "reused");
    stale("late");
    require(journal.read().at("byteCount") == 0 && nextStream != stream,
            "Shared task journal accepted a stale callback");
    TaskOutputText plain(journal.observer(nextStream));
    plain.observer()(std::string_view("plain\0bytes", 11));
    require(journal.read().at("output") == "plain?bytes", "Plain output changed before display repair");
    journal.finish();
    BuildOperations builds;
    const auto idle = builds.output();
    require(idle.at("streamId") == "" && !idle.at("running").get<bool>() && idle.at("byteCount") == 0,
            "Initial output is not empty");
    for (const auto &bad :
         {Json::array(), Json{{"revision", 1}}, Json{{"streamId", "a"}},
          Json{{"streamId", "a"}, {"revision", -1}}, Json{{"streamId", "a"}, {"revision", true}},
          Json{{"streamId", "bad\n"}, {"revision", 0}}, Json{{"path", "arbitrary"}}})
        rejected([&] { builds.output(bad); });
    ProcessOutputObserver oldObserver;
    std::string firstStream;
    builds.execute("build.run", {{"operationId", "reused"}}, [&](auto *token) {
        const auto observer = processOutputObserver(token);
        require(bool(observer) && !processOutputObserver(nullptr), "Output owner mismatch");
        std::atomic<bool> other{false};
        require(!processOutputObserver(&other), "Another owner can publish build output");
        require(!std::async(std::launch::async, [token] { return bool(processOutputObserver(token)); }).get(),
                "Output observer crossed a thread boundary");
        {
            ProcessOutputScope nested(&other, [](auto) {});
            require(!processOutputObserver(token) && bool(processOutputObserver(&other)),
                    "Nested owner leaked");
        }
        oldObserver = observer;
        observer("initial\n");
        const auto first = builds.output();
        firstStream = first.at("streamId");
        require(first.at("running") && first.at("output") == "initial\n", "Running output is unavailable");
        auto unchanged = builds.output({{"streamId", firstStream}, {"revision", first.at("revision")}});
        require(!unchanged.at("changed").get<bool>() && unchanged.at("output") == "",
                "Unchanged tail repeated");
        observer(std::string(300 * 1024, 'x'));
        const auto tail = builds.output();
        require(tail.at("output").get<std::string>().size() == 256 * 1024 &&
                    tail.at("byteCount") == 300 * 1024 + 8 && tail.at("droppedBytes") == 44 * 1024 + 8,
                "Output retention is unbounded or miscounted");
        observer(std::string("\0\x1b\xff", 3));
        const auto repaired = builds.output().at("output").get<std::string>();
        require(repaired.substr(repaired.find_last_of('x')+1).starts_with("??"),
                "Process display control bytes are not repaired");
        require(Json::parse(Json(repaired).dump()).get<std::string>()==repaired,
                "Malformed process output cannot cross the JSON display boundary");
#ifndef _WIN32
        require(repaired.ends_with("??\xef\xbf\xbd"), "Display output is not repaired");
#endif
        require(builds.cancel({{"operationId", "reused"}}).at("requested"), "Cancellation failed");
        const auto cancelling = builds.output();
        require(cancelling.at("running") && cancelling.at("cancelling"), "Tail lost live cancellation");
        return Json{{"exitCode", 0}};
    });
    require(!processOutputObserver(nullptr), "Output scope was retained");
    const auto finished = builds.output();
    oldObserver("late");
    require(!finished.at("running").get<bool>() && finished == builds.output(),
            "Late output changed a finished build");
    rejected([&] {
        builds.execute("build.configure", {{"operationId", "reused"}}, [&](auto *token) -> Json {
            oldObserver("stale");
            const auto fresh =
                builds.output({{"streamId", firstStream}, {"revision", finished.at("revision")}});
            require(fresh.at("changed") && fresh.at("streamId") != firstStream && fresh.at("byteCount") == 0,
                    "Reused operation ID mixed two builds");
            processOutputObserver(token)("before failure");
            throw std::runtime_error("fixture failure");
        });
    });
    const auto failed = builds.output();
    require(!failed.at("running").get<bool>() && failed.at("output") == "before failure",
            "Work failure discarded the last output");
    std::atomic<bool> cancelled{true};
    bool observed = false;
    const auto stopped = runProcess({"not-an-installed-executable"}, {}, std::chrono::seconds(1), 4096,
                                    &cancelled, {}, [&](auto) { observed = true; });
    require(stopped.cancelled && !observed, "Pre-cancelled work published process output");
}
