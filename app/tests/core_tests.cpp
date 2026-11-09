#include "cycode/TextFile.h"
#include "cycode/TextEdits.h"
#include "cycode/NativeFileLease.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "cycode/Engine.h"
#include "cycode/Tools.h"
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace cycode { void coreServiceChecks(const std::filesystem::path &); void nativeDocumentContracts(const std::filesystem::path &); void localEndpointChecks(const std::filesystem::path &); void sharedSnippetContracts(); }
using namespace cycode;
static void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F fn, const char *message) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
static void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}
static void pathContainmentChecks() {
    check(containsPath(utf8Path("/workspace"),utf8Path("/workspace/src/main.cpp")),
          "Ordinary path containment failed");
    check(!containsPath(utf8Path("/workspace"),utf8Path("/workspace-other/main.cpp")),
          "Containment accepted a string-prefix neighbour");
#ifdef _WIN32
    check(containsPath(utf8Path("D:/Workspace"),std::filesystem::path(L"D:\\Workspace\\src\\main.cpp")),
          "Windows containment treated equivalent root separators as different");
    check(containsPath(std::filesystem::path(L"D:\\WORKSPACE"),utf8Path("d:/workspace/src/main.cpp")),
          "Windows containment lost case-insensitive component comparison");
    check(!containsPath(utf8Path("D:/Workspace"),utf8Path("E:/Workspace/src/main.cpp")),
          "Windows containment crossed a drive");
    check(!containsPath(utf8Path("D:/Workspace"),utf8Path("D:/Workspace-other/main.cpp")),
          "Windows containment accepted a string-prefix neighbour");
#endif
}
static void longLeaseChecks(const std::filesystem::path &root) {
    const auto file = root / utf8Path("lease-" + std::string(224, 'x') + ".lock");
    {
        NativeFileLease first(file, NativeFileLeaseMode::shared);
        NativeFileLease second(file, NativeFileLeaseMode::shared);
        NativeFileLease blocked(file);
        check(first.acquired() && second.acquired() && !blocked.acquired(),
              "Long-path shared leases do not retain their common file identity");
    }
    {
        NativeFileLease exclusive(file);
        NativeFileLease blocked(file, NativeFileLeaseMode::shared);
        check(exclusive.acquired() && !blocked.acquired(), "Long-path exclusive lease failed");
    }
#ifdef _WIN32
    const auto filename = L"\\\\?\\" + std::filesystem::absolute(file).lexically_normal().make_preferred().native();
    check(DeleteFileW(filename.c_str()), "Cannot retire the owned long-path lease fixture");
#else
    check(std::filesystem::remove(file), "Cannot retire the owned lease fixture");
#endif
}
static void unicodeSearchChecks(const std::filesystem::path &root) {
    std::filesystem::create_directory(root);
    const std::string baseline="// Original disk only\n";
    write(root/"main.cpp",baseline);
    Engine engine(root);engine.configureLanguage("",false,false);
    auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
    auto replace=[&](const std::string &text) {
        doc=engine.dispatch("documents.replace",{{"path","main.cpp"},{"text",text},{"expectedRevision",doc.at("revision")}});
    };
    auto search=[&](const std::string &query,bool sensitive=false,bool regex=false) {
        return engine.dispatch("search.text",{{"query",query},{"caseSensitive",sensitive},{"regex",regex}}).at("matches");
    };
    const std::string unicode="// ÉTÉ Σ ς K Straße ẞ\n";
    replace(unicode);
    const auto accents=search("été");
    check(accents.size()==1 && accents[0].at("path")=="main.cpp" && accents[0].at("column")==4,
          "Unsaved accent-insensitive search lost byte columns");
    const auto sigma=search("σ");
    check(sigma.size()==2 && sigma[0].at("column")==unicode.find("Σ")+1 &&
          sigma[1].at("column")==unicode.find("ς")+1,"Unicode simple-case equivalents do not match");
    check(search("k").size()==1,"ASCII query did not match the Unicode Kelvin equivalent");
    check(search("ss").empty() && search("ß").size()==2,"Literal search changed Unicode simple-fold semantics");
    check(search("été",true).empty() && search("ÉTÉ",true).size()==1,"Case-sensitive Unicode search changed");
    replace("[ÉTÉ].*\n");
    check(search("[été].*").size()==1,"Literal metacharacters became regular expressions");
    replace(std::string(511,'x')+"éNeedle\n");
    for (const bool regex:{false,true}) {
        const auto result=search("Needle",false,regex);
        check(result.size()==1 && result[0].at("column")==514 &&
              result[0].at("text").get<std::string>().size()==511,"UTF-8 preview split a scalar or lost a byte column");
        (void)result.dump(); // The public result must remain serializable UTF-8.
    }
    replace("needle needle\nneedle\n");
    const auto limited=engine.dispatch("search.text",{{"query","needle"},{"limit",1}});
    check(limited.at("matches").size()==1 && limited.at("truncated").get<bool>(),
          "Literal search did not report its exact result limit");
    std::atomic<bool> cancellation{true};
    const auto cancelled=engine.searchText({{"query","needle"}},&cancellation);
    check(cancelled.value("cancelled",false) && cancelled.at("matches").empty(),
          "Cancelled literal search exposed a partial observation");
    check(readTextFileBytes(root/"main.cpp")==baseline,"Unsaved Unicode search modified source bytes");
}
static void toolAliasChecks(const std::filesystem::path &root) {
    check(sameExecutableInvocation(root / "compiler-cpp", root / "." / "compiler-cpp"),
          "Compiler invocation comparison did not normalize parent directories");
    check(!sameExecutableInvocation(root / "compiler-c", root / "compiler-cpp"),
          "Compiler invocation comparison discarded the driver name");
    check(!sameExecutableInvocation("compiler-cpp", root / "compiler-cpp"),
          "Relative compiler invocation accepted");
#ifdef _WIN32
    check(sameExecutableInvocation(root / "Compiler-Cpp", root / "compiler-cpp"),
          "Windows compiler invocation comparison was case-sensitive");
#else
    check(!sameExecutableInvocation(root / "Compiler-Cpp", root / "compiler-cpp"),
          "POSIX compiler invocation comparison ignored case");
#endif
#ifndef _WIN32
    namespace fs = std::filesystem;
    struct Environment {
        std::string name, value;
        bool present;
        explicit Environment(const char *key) : name(key), present(std::getenv(key) != nullptr) {
            if (present) value = std::getenv(key);
        }
        ~Environment() {
            if (present) setenv(name.c_str(), value.c_str(), 1);
            else unsetenv(name.c_str());
        }
    } pathGuard("PATH"), overrideGuard("CYCODE_CYCODE_ALIAS_TEST_CPP");
    const auto directory = root / "tool aliases été";
    fs::create_directory(directory);
    const auto driver = directory / "driver-data";
    write(driver, "Data-only tool discovery fixture; never executed.\n");
    fs::permissions(driver, fs::perms::owner_read | fs::perms::owner_exec);
    const auto alias = directory / "cycode_alias_test_cpp";
    fs::create_symlink(driver.filename(), alias);
    const auto directoryAlias = root / "tool-directory-alias";
    fs::create_directory_symlink(directory.filename(), directoryAlias);
    check(sameExecutableInvocation(alias, directoryAlias / alias.filename()),
          "Compiler invocation comparison rejected a directory alias");
    check(!sameExecutableInvocation(alias, driver),
          "C and C++ driver aliases were treated as one invocation");
    check(setenv("PATH", pathUtf8(directory).c_str(), 1) == 0, "Cannot isolate tool path fixture");
    unsetenv("CYCODE_CYCODE_ALIAS_TEST_CPP");
    check(utf8Path(toolPath("cycode_alias_test_cpp")) == alias,
          "Tool lookup resolved the executable alias and changed argv[0]");
    check(findExecutableOnPath("cycode_alias_test_cpp") == alias,
          "CMake tool discovery resolved a compiler alias");
    check(findExecutableOnPath("cycode_alias_test_cpp", {driver}) == alias,
          "Explicit discovery hints replaced the first PATH candidate");
    check(findExecutableOnPath("cycode_absent_test_tool", {alias}) == alias,
          "Absolute discovery hint lost its invocation path");
    check(findExecutableOnPath("cycode_absent_test_tool", {alias.filename()}).empty(),
          "Relative discovery hint accepted");
    const auto blocked = directory / "cycode_unexecutable_test_tool";
    write(blocked, "Non-executable data fixture.\n");
    fs::permissions(blocked, fs::perms::owner_read);
    check(toolPath("cycode_unexecutable_test_tool").empty(), "Tool lookup accepted a non-executable file");
    check(findExecutableOnPath("cycode_unexecutable_test_tool", {blocked}).empty(),
          "CMake tool discovery accepted a non-executable candidate");
    fs::create_symlink("absent", directory / "cycode_broken_test_tool");
    check(toolPath("cycode_broken_test_tool").empty(), "Tool lookup accepted a broken alias");
    check(findExecutableOnPath("cycode_broken_test_tool").empty(), "CMake tool discovery accepted a broken alias");
    check(setenv("PATH", ("\"" + pathUtf8(directory) + "\"").c_str(), 1) == 0,
          "Cannot quote tool path fixture");
    check(findExecutableOnPath("cycode_alias_test_cpp") == alias, "Quoted absolute PATH entry lost its alias");
#else
    (void)root;
#endif
}
static void journalChecks(const std::filesystem::path &root) {
    Events journal;
    const auto epoch = journal.read(Json::object()).at("epoch");
    for (int i = 0; i < 550; ++i)
        journal.publish("document.changed", {{"path", "a.cpp"}, {"revision", i + 1}});
    const auto first = journal.read({{"limit", 3}, {"types", {"document.*"}}});
    check(first.at("gap").get<bool>() && first.at("events").size() == 3 && first.at("hasMore").get<bool>(),
          "Journal overflow and bounded pages");
    const auto skipped = journal.read({{"types", {"build.*"}}});
    check(skipped.at("events").empty() && skipped.at("nextSequence") == 550,
          "Filtered journal cursor did not advance");
    rejects([&] { journal.read({{"afterSequence", 1}}); }, "Unscoped event resume accepted");
    rejects([&] { journal.read({{"afterSequence", 551}, {"epoch", epoch}}); },
            "Future event cursor accepted");
    rejects([&] { journal.read({{"epoch", "different"}}); }, "Foreign event epoch accepted");
    rejects([&] { journal.read({{"limit", 0}}); }, "Unbounded event limit accepted");
    journal.publish("oversized", {{"value", std::string(70000, 'x')}});
    const auto dropped = journal.read({{"epoch", epoch}, {"afterSequence", 550}});
    check(dropped.at("gap").get<bool>() && dropped.at("events").empty() && dropped.at("nextSequence") == 551,
          "Dropped event not signalled");
    const auto tail = journal.read({{"tail", true}});
    check(!tail.at("gap").get<bool>() && tail.at("events").empty() && tail.at("nextSequence") == 551,
          "Journal tail subscription");
    Events origins;
    {
        EventOrigin scope("sample.plugin");
        origins.publish("sample", Json::object());
    }
    check(origins.read({{"excludeOrigin", "sample.plugin"}}).at("events").empty(),
          "Origin filter returned own events");
    Events concurrent;
    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i)
        writers.emplace_back([&] {
            for (int j = 0; j < 100; ++j)
                concurrent.publish("sample", Json::object());
        });
    for (auto &writer : writers)
        writer.join();
    check(concurrent.read({{"limit", 512}}).at("events").size() == 400,
          "Concurrent event publication lost entries");
    const auto workspace = root / "event-workspace";
    std::filesystem::create_directory(workspace);
    write(workspace / "a.cpp", "int value=1;\n");
    Engine engine(workspace);
    engine.configureLanguage("", false, false);
    const auto start = engine.dispatch("events.read", {{"tail", true}});
    auto doc = engine.dispatch("documents.read", {{"path", "a.cpp"}});
    doc = engine.dispatch(
        "documents.replace",
        {{"path", "a.cpp"}, {"expectedRevision", doc.at("revision")}, {"text", "int value=2;\n"}});
    engine.dispatch("documents.save", {{"path", "a.cpp"}, {"expectedRevision", doc.at("revision")}});
    write(workspace / "a.cpp", "int value=3;\n");
    doc = engine.dispatch("documents.reload", {{"path", "a.cpp"}, {"expectedRevision", doc.at("revision")}});
    engine.dispatch("documents.close", {{"path", "a.cpp"}});
    rejects([&] {engine.dispatch("documents.reload",{{"path","a.cpp"},{"expectedRevision",doc.at("revision")}});},
            "Reload accepted a retired document revision");
    check(engine.dispatch("documents.list").at("documents").empty(),
          "Rejected reload opened a closed document or published a new lifetime");
    const auto events = engine
                            .dispatch("events.read", {{"epoch", start.at("epoch")},
                                                      {"afterSequence", start.at("nextSequence")},
                                                      {"types", {"document.*"}}})
                            .at("events");
    check(events.size() == 5 && events[0].at("type") == "document.opened" &&
              events[1].at("type") == "document.changed" && events[2].at("type") == "document.saved" &&
              events[3].at("type") == "document.reloaded" && events[4].at("type") == "document.closed",
          "Native document event order");
    check(events[0].at("data").at("id") == events[3].at("data").at("id"),
          "Reload published a transient document identity");
    check(!events[2].at("data").at("dirty").get<bool>() && events[2].at("data").at("diskConflict") == "",
          "Save event used uncommitted metadata");
    for (const auto &event : events)
        check(!event.at("data").contains("text"), "Document text leaked into event metadata");
}
void workspaceSessionContracts(const std::filesystem::path &);
void relatedFileContracts(const std::filesystem::path &);
void draftDocumentContracts(const std::filesystem::path &);
void hierarchyContracts();
void semanticTokenContracts();
void annotationContracts();
void updateContracts(const std::filesystem::path &);
static void progressiveFileIndexContracts(const std::filesystem::path &root) {
    const auto folder=root/"progressive-index";std::filesystem::create_directories(folder);
    write(folder/"visible.cpp","int visible;\n");write(folder/"ignored.cpp","int ignored;\n");
    write(folder/".gitignore","ignored.cpp\n");
    WorkspaceFileIndex index(folder);
    auto wait=[&](uint64_t revision,bool refresh=false) {
        auto state=index.snapshot(revision,refresh);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
        while(state->indexing&&std::chrono::steady_clock::now()<deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));state=index.snapshot(revision,false);
        }
        check(state->complete&&state->error.empty(),"Progressive file enumeration did not finish");
        return state;
    };
    const auto first=wait(1);
    check(first->files==Json::array({".gitignore","visible.cpp"}),"Streaming index lost sorted ignore-aware paths");
    write(folder/"added.cpp","int added;\n");
    const auto second=wait(2);
    check(second->generation>first->generation&&second->files.size()==3,"A new filesystem revision did not rebuild the stream");
    check(first->files==Json::array({".gitignore","visible.cpp"}),"Published file-index snapshot was mutated");
    const auto refreshed=wait(2,true);
    check(refreshed->generation>second->generation&&refreshed->revision==second->revision,
          "Refresh reused an old cache generation");
    index.stop();
}
int main(int argc, char **argv) {
    if(argc>1&&std::string(argv[1])=="--stream-child") {
        const std::string block(4096,'x');
        for(size_t i=0;i<9216;++i)std::cout.write(block.data(),block.size());
        return 0;
    }
#ifndef _WIN32
    if (argc>1 && std::string(argv[1])=="--group-child") {
        if (getpgrp()!=getpid()) return 3;
        std::cout << "owned-group\n";
        return 0;
    }
#endif
    if (argc > 1 && std::string(argv[1]) == "--sleep-child") {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 0;
    }
    auto root =
        std::filesystem::temp_directory_path() /
        ("cycode-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(root);
        nativeDocumentContracts(root);
        localEndpointChecks(root);
        sharedSnippetContracts();
        pathContainmentChecks();
        progressiveFileIndexContracts(root);
        longLeaseChecks(root);
        toolAliasChecks(root);
        relatedFileContracts(root);
        workspaceSessionContracts(root);
        draftDocumentContracts(root);
        hierarchyContracts();
        semanticTokenContracts();
        annotationContracts();
        updateContracts(root);
        journalChecks(root);
        write(root / "test.cpp", "\xef\xbb\xbfint main() { return 0; }\r\n");
        write(root / ".gitignore", "ignored.txt\n");
        write(root / "ignored.txt", "private_needle\n");
        {
            Engine engine(root);
            auto opened = engine.dispatch("documents.open", {{"path", "test.cpp"}});
            check(opened["revision"] == 1, "Initial revision");
            check(opened["lineEnding"] == "CRLF", "CRLF detection");
            check(opened["encoding"] == "UTF-8 BOM", "BOM detection");
            rejects(
                [&] {
                    engine.dispatch("documents.replace",
                                    {{"path", "test.cpp"}, {"text", "oops"}, {"expectedRevision", 0}});
                },
                "Stale edit must fail");
            auto edited = engine.dispatch(
                "documents.replace",
                {{"path", "test.cpp"}, {"text", "int main() { return 42; }\n"}, {"expectedRevision", 1}});
            check(edited["dirty"] == true, "Unsaved edit");
            auto search = engine.dispatch("search.text", {{"query", "42"}});
            check(search["matches"].size() == 1, "Search unsaved buffers");
            const auto regex=engine.dispatch("search.text",{{"query","4[2]"},{"regex",true}});
            check(regex.at("matches").size()==1&&regex.at("matches")[0].at("path")=="test.cpp"&&!regex.at("unsavedRegexExcluded").get<bool>(),"Regex must include unsaved buffers");
            check(engine.dispatch("search.text", {{"query", "return 0"}})["matches"].empty(),
                  "Dirty buffer must hide old disk hits");
            check(engine.dispatch("search.text", {{"query", "private_needle"}})["matches"].empty(),
                  "Respect gitignore");
            engine.dispatch("documents.save", {{"path", "test.cpp"}, {"expectedRevision", 2}});
            std::ifstream file(root / "test.cpp", std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(file)), {});
            file.close();
            check(bytes == "\xef\xbb\xbfint main() { return 42; }\r\n", "Preserve BOM and CRLF");
            write(root / "test.cpp", "external\n");
            rejects(
                [&] { engine.dispatch("documents.save", {{"path", "test.cpp"}, {"expectedRevision", 2}}); },
                "External edit must not be overwritten");
            rejects([&] { engine.dispatch("documents.open", {{"path", "../outside.txt"}}); },
                    "Reject path traversal");
            std::error_code ec;
            std::filesystem::create_directory_symlink(root.parent_path(), root / "escape", ec);
            if (!ec)
                rejects([&] { engine.resolve("escape/outside.txt"); }, "Reject symlink escape");
            rejects(
                [&] {
                    engine.dispatch(
                        "documents.replace",
                        {{"path", "test.cpp"}, {"text", std::string("a\0b", 3)}, {"expectedRevision", 2}});
                },
                "Reject binary replacement");
            engine.dispatch("documents.close", {{"path", "test.cpp"}});
            auto reopened = engine.dispatch("documents.open", {{"path", "test.cpp"}});
            check(reopened["revision"].get<std::uint64_t>() > 2, "Reopen must not reuse an old revision");
            rejects(
                [&] {
                    engine.dispatch("documents.replace",
                                    {{"path", "test.cpp"}, {"text", "stale"}, {"expectedRevision", 2}});
                },
                "Reject a revision from an earlier document lifetime");
            auto created = engine.dispatch("documents.create", {{"path", "new/日本語.cpp"}});
            check(created["text"] == "" && std::filesystem::exists(root / utf8Path("new/日本語.cpp")),
                  "Unicode file creation");
            rejects([&] { engine.dispatch("documents.create", {{"path", "new/日本語.cpp"}}); },
                    "Create must not overwrite");
            write(root / "classic.txt", "alpha\rbeta\r");
            auto classic = engine.dispatch("documents.open", {{"path", "classic.txt"}});
            check(classic["lineEnding"] == "CR" && classic["text"] == "alpha\nbeta\n",
                  "Classic CR line endings");
            auto classicEdit =
                engine.dispatch("documents.replace", {{"path", "classic.txt"},
                                                      {"expectedRevision", classic["revision"]},
                                                      {"text", "gamma\nbeta\n"}});
            engine.dispatch("documents.save",
                            {{"path", "classic.txt"}, {"expectedRevision", classicEdit["revision"]}});
            std::ifstream classicFile(root / "classic.txt", std::ios::binary);
            std::string classicBytes((std::istreambuf_iterator<char>(classicFile)), {});
            classicFile.close();
            check(classicBytes == "gamma\rbeta\r", "Preserve CR-only source files when saving");
            auto init = runProcess({"git", "init"}, root);
            check(init.exitCode == 0, "Git init");
            auto status = engine.dispatch("vcs.status");
            check(status["repository"] == true, "Detect Git repository");
            check(engine.dispatch("vcs.stage", {{"path", "test.cpp"}})["exitCode"] == 0, "Git stage");
            status = engine.dispatch("vcs.status");
            bool staged = false;
            for (const auto &f : status["files"])
                if (f["path"] == "test.cpp" && f["status"] == "A ")
                    staged = true;
            check(staged, "Staged status");
            check(engine.dispatch("vcs.unstage", {{"path", "test.cpp"}})["exitCode"] == 0,
                  "Unstage before the initial commit");
            check(std::filesystem::exists(root / "test.cpp"), "Unstage retains the source file");
            Engine nested(root / "new");
            auto nestedStatus = nested.dispatch("vcs.status");
            bool relative = false;
            for (const auto &file : nestedStatus["files"])
                if (file["path"] == "日本語.cpp")
                    relative = true;
            check(relative, "Nested Git workspace paths must be relative to that workspace");
            std::atomic<bool> cancel{false};
            std::thread cancellation([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                cancel = true;
            });
            auto stopped = runProcess({pathUtf8(std::filesystem::absolute(argv[0])), "--sleep-child"}, root,
                                      std::chrono::seconds(3), 4096, &cancel);
            cancellation.join();
            check(stopped.cancelled && stopped.exitCode != 0,
                  "Cancellation terminates the actual child process");
            {
                std::atomic<bool> streamCancellation{false};size_t delivered=0;
                const auto streamed=runProcessStreamed({pathUtf8(std::filesystem::absolute(argv[0])),"--stream-child"},root,
                    streamCancellation,[&](std::string_view bytes) {
                        check(bytes.find_first_not_of('x')==std::string_view::npos,"Stream content changed");
                        delivered+=bytes.size();
                    });
                check(streamed.exitCode==0&&!streamed.truncated&&!streamed.timedOut&&delivered==36u*1024u*1024u,
                      "Streaming discovery stopped at the former 32-MiB boundary");
                check(streamed.output.size()<=8192,"Streaming diagnostics retained unbounded output");
                const auto noDeadline=runProcessStreamed({pathUtf8(std::filesystem::absolute(argv[0])),"--sleep-child"},root,
                    streamCancellation,[](std::string_view){});
                check(noDeadline.exitCode==0&&!noDeadline.timedOut,"Continuous discovery inherited an immediate zero deadline");
                std::thread stopStream([&] {std::this_thread::sleep_for(std::chrono::milliseconds(30));streamCancellation=true;});
                const auto start=std::chrono::steady_clock::now();
                const auto interrupted=runProcessStreamed({pathUtf8(std::filesystem::absolute(argv[0])),"--sleep-child"},root,
                    streamCancellation,[](std::string_view){});
                stopStream.join();
                check(interrupted.cancelled&&interrupted.exitCode!=0&&std::chrono::steady_clock::now()-start<std::chrono::seconds(2),
                      "Continuous discovery lost owned cancellation");
            }
#ifndef _WIN32
            {
                const auto executable=pathUtf8(std::filesystem::absolute(argv[0]));
                std::vector<std::future<ProcessResult>> groups;
                groups.reserve(4);
                for (int worker=0;worker<4;++worker)
                    groups.push_back(std::async(std::launch::async,[executable,root] {
                        for (int attempt=0;attempt<20;++attempt) {
                            auto result=runProcess({executable,"--group-child"},root,std::chrono::seconds(5),4096);
                            if (result.exitCode || result.timedOut || result.output!="owned-group\n") {
                                if (!result.exitCode) result.exitCode=-1;
                                return result;
                            }
                        }
                        ProcessResult result;result.exitCode=0;return result;
                    }));
                std::string failure;
                for (auto &group:groups) {
                    const auto result=group.get();
                    if (result.exitCode || result.timedOut)
                        failure="Concurrent child group setup failed: "+std::to_string(result.exitCode)+" "+result.output;
                }
                if (!failure.empty()) throw std::runtime_error(failure);
            }
#endif
            auto process = runProcess({"git", "-c", "alias.cycode=!echo unsafe", "--version"}, root);
            check(process.exitCode == 0 && process.output.starts_with("git version"),
                  "Process argv execution");
            if (argc > 1) {
                auto library = std::filesystem::path(argv[1]);
                auto pluginDir = root / "plugins";
                std::filesystem::create_directory(pluginDir);
                std::filesystem::copy_file(library, pluginDir / library.filename());
                write(pluginDir / "sample.json",
                      Json({{"id", "sample"}, {"abi", 1}, {"library", library.stem().string()}}).dump());
                engine.plugins().load(pluginDir);
                auto stats = engine.dispatch("plugins.invoke", {{"command", "sample.text_statistics"},
                                                                {"input", {{"text", "hello world\n"}}}});
                check(stats["words"] == 2 && stats["lines"] == 2, "Real dynamic plugin");
            }
        }
        coreServiceChecks(root);
        unicodeSearchChecks(root/"unicode-search");
        std::filesystem::remove_all(root);
        std::cout << "Core tests passed: revisions, disk conflicts, encoding, boundaries, unsaved search, "
                     "Git, native plugin.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED: " << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}
