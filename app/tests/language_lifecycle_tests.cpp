#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace cycode;
static void require(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
static int delayedServer() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    size_t opened = 0;
    for (;;) {
        std::string line;
        size_t length = 0;
        while (std::getline(std::cin, line)) {
            if (line == "\r" || line.empty())
                break;
            if (line.starts_with("Content-Length:"))
                length = std::stoull(line.substr(15));
        }
        if (!std::cin || length > 16 * 1024 * 1024)
            return 0;
        std::string body(length, '\0');
        std::cin.read(body.data(), length);
        if (!std::cin)
            return 0;
        const auto request = Json::parse(body);
        if (request.value("method", "") == "textDocument/didOpen")
            ++opened;
        if (!request.contains("id"))
            continue;
        Json result;
        if (request.value("method", "") == "initialize")
            result = {{"capabilities", {{"positionEncoding", "utf-16"}}}};
        else if (request.value("method", "") == "test/received")
            result = {{"opened", opened}};
        else if (request.value("method","")=="textDocument/rename" && std::filesystem::exists("versioned-edits"))
            result={{"documentChanges",Json::array({{{"textDocument",{{"uri",request.at("params").at("textDocument").at("uri")},{"version",std::filesystem::exists("stale-version") ? 0 : 1}}},{"edits",Json::array()}}})}};
        else {
            {
                std::ofstream marker("request-received");
                marker << request["id"];
            }
            while (!std::filesystem::exists("release-response"))
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (request.value("method","")=="textDocument/semanticTokens/full") result={{"data",Json::array({0,4,5,0,0})}};
            if (request.value("method","")=="textDocument/hover") result={{"contents","Old documentation"}};
            if (request.value("method","")=="textDocument/signatureHelp") result={{"signatures",Json::array({{{"label","f(int value)"}}})}};
        }
        if (request.value("method","")=="initialize" && std::filesystem::exists("support-tokens"))
            result["capabilities"]["semanticTokensProvider"]={{"full",true},{"legend",{{"tokenTypes",Json::array({"variable"})},{"tokenModifiers",Json::array()}}}};
        if (request.value("method","")=="initialize" && std::filesystem::exists("versioned-edits")) result["capabilities"]["renameProvider"]=true;
        if (request.value("method","")=="initialize" && std::filesystem::exists("support-info")) { result["capabilities"]["hoverProvider"]=true;result["capabilities"]["signatureHelpProvider"]=Json::object(); }
        auto reply = Json{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}}.dump();
        std::cout << "Content-Length: " << reply.size() << "\r\n\r\n" << reply << std::flush;
        if (request.value("method", "") == "initialize")
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}
int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--background-index")
        return delayedServer();
    const auto root = std::filesystem::temp_directory_path() /
                      ("cycode-language-lifecycle-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    try {
        {
            std::ofstream source(root / "main.cpp");
            source << "int value;\n";
        }
        {
            Engine engine(root);
            engine.configureLanguage(pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))), true, true);
            auto doc = engine.dispatch("documents.read", {{"path", "main.cpp"}});
            const auto unsupported=engine.dispatch("language.highlight",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")}});
            require(!unsupported.at("supported").get<bool>() && unsupported.at("tokens").empty(),"Missing semantic capability must preserve lexical fallback");
            auto beginQuery = [&]() {
                auto revision = doc["revision"];
                return std::async(std::launch::async, [&, revision] {
                    return engine.dispatch("language.definition",
                                           {{"path", "main.cpp"},
                                            {"expectedRevision", revision},
                                            {"position", {{"line", 0}, {"character", 4}}}});
                });
            };
            auto waitForRequest = [&]() {
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (!std::filesystem::exists(root / "request-received") &&
                       std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                require(std::filesystem::exists(root / "request-received"),
                        "Delayed server did not receive query");
            };
            auto query = beginQuery();
            waitForRequest();
            doc = engine.dispatch(
                "documents.replace",
                {{"path", "main.cpp"}, {"expectedRevision", doc["revision"]}, {"text", "int value = 1;\n"}});
            { std::ofstream release(root / "release-response"); }
            require(query.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
                    "Delayed query did not finish");
            bool stale = false;
            try {
                query.get();
            } catch (const std::exception &e) {
                stale = std::string(e.what()).starts_with("RevisionConflict");
            }
            require(stale, "A response to an obsolete live revision was accepted");
            std::filesystem::remove(root / "request-received");
            std::filesystem::remove(root / "release-response");
            query = beginQuery();
            waitForRequest();
            engine.configureLanguage("", false, false);
            require(query.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
                    "Stop did not release in-flight query");
            bool stopped = false;
            try {
                query.get();
            } catch (...) {
                stopped = true;
            }
            require(stopped, "Stopped query unexpectedly succeeded");
        }
        {
            std::filesystem::remove(root/"request-received"); std::filesystem::remove(root/"release-response");
            { std::ofstream marker(root/"support-tokens"); }
            Engine engine(root); engine.configureLanguage(pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))),true,true);
            const auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
            auto query=std::async(std::launch::async,[&] { return engine.dispatch("language.highlight",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")}}); });
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (!std::filesystem::exists(root/"request-received") && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            require(std::filesystem::exists(root/"request-received"),"Delayed semantic request not received");
            engine.dispatch("documents.replace",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"text","int changed;\n"}});
            { std::ofstream release(root/"release-response"); }
            require(query.wait_for(std::chrono::seconds(2))==std::future_status::ready,"Delayed semantic response not released");
            bool stale=false; try { query.get(); } catch (const std::exception &e) { stale=std::string(e.what()).starts_with("RevisionConflict"); }
            require(stale,"Obsolete semantic colors accepted after edit");
        }
        {
            LanguageService service(root);
            service.configure(pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))), true, true);
            for (int i = 0; i < 2000; ++i)
                service.sync("bulk/" + std::to_string(i) + ".cpp", std::string(4096, ' ') + "int value;\n",
                             i + 1);
            auto reply = service.request("test/received", Json::object());
            require(reply.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
                    "Bulk document synchronization stalled");
            require(reply.get().at("opened") == 2000, "Transport backpressure dropped a document overlay");
        }
        {
            { std::ofstream marker(root/"versioned-edits"); }
            LanguageService service(root);service.configure("",false,false);
            bool invalid=false;try { service.configureBackend("rust",{},{{"$workspaceReadyNotification",3}},true); } catch (...) { invalid=true; }
            require(invalid,"Invalid readiness metadata accepted by the worker");
            service.configureBackend("rust",{pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))),"--background-index"},Json::object(),true);
            service.sync("main.rs","fn greet() {}\n",42);
            const Json params={{"textDocument",{{"uri",LanguageClient::uri(root/"main.rs")}}},{"newName","welcome"}};
            auto reply=service.request("textDocument/rename",params).get();
            const auto document=reply.at("documentChanges")[0].at("textDocument");
            require(document.at("cycodeRevision")==42 && !document.contains("version"),"LSP version was not mapped to the native revision");
            { std::ofstream marker(root/"stale-version"); }
            bool stale=false;try { service.request("textDocument/rename",params).get(); } catch (const std::exception &e) { stale=std::string(e.what()).starts_with("RevisionConflict"); }
            require(stale,"Obsolete LSP workspace edit version was accepted");
        }
        {
            Engine engine(root);engine.configureLanguage(pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))),true,true);
            const auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
            for (const auto *method : {"language.hover","language.signature"}) {
                bool unsupported=false;try { engine.dispatch(method,{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"position",{{"line",0},{"character",4}}}}); }
                catch (const std::exception &e) { unsupported=std::string(e.what()).find("does not support")!=std::string::npos; }
                require(unsupported,"Backend without information capability accepted a request");
            }
        }
        { std::ofstream marker(root/"support-info"); }
        for (const auto *method : {"language.hover","language.signature"}) {
            std::filesystem::remove(root/"request-received");std::filesystem::remove(root/"release-response");
            Engine engine(root);engine.configureLanguage(pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))),true,true);
            const auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
            auto query=std::async(std::launch::async,[&] { return engine.dispatch(method,{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"position",{{"line",0},{"character",4}}}}); });
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (!std::filesystem::exists(root/"request-received") && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            require(std::filesystem::exists(root/"request-received"),"Information query was not received by delayed server");
            engine.dispatch("documents.replace",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"text","int changed;\n"}});
            { std::ofstream release(root/"release-response"); }
            require(query.wait_for(std::chrono::seconds(2))==std::future_status::ready,"Information reply did not stop");
            bool stale=false;try { query.get(); } catch (const std::exception &e) { stale=std::string(e.what()).starts_with("RevisionConflict"); }
            require(stale,"Outdated documentation/signature was accepted");
        }
        std::filesystem::remove_all(root);
        std::cout << "Obsolete documentation/signature replies and unsupported methods passed.\n";
        std::cout << "Language lifecycle: late revision rejection, stop during blocked response and 2000 "
                     "queued document overlays passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}
