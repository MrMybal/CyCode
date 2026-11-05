#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace cycode;
void rustAnalysisContracts(const std::filesystem::path &);
static void require(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
static void preferencesChecks(const std::filesystem::path &directory) {
    namespace fs = std::filesystem;
    const auto workspace = directory / "preferences-project";
    const auto storage = directory / "application-preferences";
    fs::create_directory(workspace);
    auto rejects = [](const auto &work, const char *message) {
        bool rejected = false;
        try {
            work();
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, message);
    };
    LanguagePreferences first(workspace, storage), second(workspace, storage);
    require(!fs::exists(storage), "Inspecting preferences created application files");
    const Json custom = {{"enabled", false},
                         {"arguments", {"server-not-started", "--stdio"}},
                         {"settings", {{"example", {{"strict", true}}}}}};
    first.update("python", custom, 0);
    rejects([&] { second.update("css", custom, 0); }, "Stale preferences overwrite accepted");
    require(second.snapshot().at("profiles").at("python") == custom, "Other instance lost the saved launch");
    require(!fs::exists(workspace / ".cycode/languages.json"),
            "Preferences leaked into project launch configuration");
    auto invalid = custom;
    invalid["arguments"] = {std::string("bad\0program", 11)};
    rejects([&] { first.update("python", invalid, 1); }, "NUL program accepted");
    invalid = custom;
    invalid["settings"]["$workspaceReadyNotification"] = "arbitrary/notification";
    rejects([&] { first.update("python", invalid, 1); }, "Unsupported readiness notification persisted");
    invalid = custom;
    invalid["executable"] = "another-program";
    rejects([&] { first.update("python", invalid, 1); }, "Ambiguous argv/executable persisted");
    require(first.snapshot().at("revision") == 1, "Invalid configuration advanced the revision");
    first.update("fixture.language.server", custom, 1);
    {
        Engine engine(workspace, storage);
        engine.configureLanguage("", false, false);
        const auto status = engine.languageStatus();
        require(!status.at("servers").at("python").at("enabled").get<bool>(),
                "Reading preferences enabled Python");
        require(!status.at("servers").at("python").value("running", false),
                "Reading preferences started a server");
        require(engine.dispatch("language.preferences").at("profiles").contains("fixture.language.server"),
                "Unavailable plugin profile lost");
        const auto configured =
            engine.dispatch("language.configure", {{"language", "python"}, {"enabled", false}});
        require(configured.at("servers").at("python").at("arguments") == custom.at("arguments"),
                "Saved launch not restored on explicit configuration");
        require(configured.at("servers").at("python").at("configuration").at("settings") ==
                    custom.at("settings"),
                "Saved settings not restored");
        rejects(
            [&] {
                engine.dispatch("language.configure", {{"language", "fixture.language.server"},
                                                       {"save", true},
                                                       {"expectedPreferencesRevision", 2}});
            },
            "Unavailable plugin launched");
        rejects(
            [&] {
                engine.dispatch("language.configure", {{"language", "python"},
                                                       {"enabled", false},
                                                       {"save", true},
                                                       {"arguments", {"different-server"}},
                                                       {"expectedPreferencesRevision", 1}});
            },
            "Stale launch applied");
        require(engine.languageStatus().at("servers").at("python").at("arguments") == custom.at("arguments"),
                "Rejected save changed the current launch");
        auto saved = engine.dispatch("language.configure", {{"language", "python"},
                                                            {"enabled", false},
                                                            {"arguments", Json::array()},
                                                            {"settings", Json::object()},
                                                            {"save", true},
                                                            {"expectedPreferencesRevision", 2}});
        require(saved.at("preferences").at("revision") == 3, "Default launch was not saved");
        auto profiles = engine.dispatch("language.preferences");
        require(profiles.at("profiles").at("python").at("arguments").empty(),
                "Resolved installed argv was persisted");
        require(profiles.at("profiles").at("python").at("settings").empty(),
                "Resolved installed settings were persisted");
        const auto before = engine.languageStatus().at("servers").at("python");
        profiles = engine.dispatch("language.preferencesForget",
                                   {{"language", "python"}, {"expectedPreferencesRevision", 3}});
        require(!profiles.at("profiles").contains("python"), "Forget retained the profile");
        const auto after = engine.languageStatus().at("servers").at("python");
        require(before.at("configuration") == after.at("configuration") &&
                    before.at("enabled") == after.at("enabled"),
                "Forget reconfigured the current server");
        engine.dispatch(
            "language.configure",
            {{"language", "cpp"}, {"enabled", false}, {"save", true}, {"expectedPreferencesRevision", 4}});
        engine.configureLanguage("unused-server", true, false);
        require(!engine.languageStatus().at("servers").at("cpp").at("enabled").get<bool>(),
                "Initialization overrode saved C++ disable");
    }
    {
        Engine reopened(workspace, storage);
        reopened.configureLanguage("unused-server", true, false);
        require(!reopened.languageStatus().at("servers").at("cpp").at("enabled").get<bool>(),
                "Reopening lost C++ disable");
        require(!reopened.languageStatus().at("servers").at("python").at("enabled").get<bool>(),
                "Reopening activated an additional server");
    }
    {
        Engine isolated(workspace);
        require(!isolated.dispatch("language.preferences").at("available").get<bool>(),
                "Embedding implicitly trusted a preferences path");
        isolated.configureLanguage("", false, false);
        rejects(
            [&] {
                isolated.dispatch("language.configure", {{"language", "python"},
                                                         {"enabled", false},
                                                         {"save", true},
                                                         {"expectedPreferencesRevision", 0}});
            },
            "Host without storage saved preferences");
    }
    // A corrupt application file is reported and retained, never replaced by defaults.
    fs::path savedPath;
    for (const auto &entry : fs::directory_iterator(storage))
        if (entry.path().extension() == ".json")
            savedPath = entry.path();
    {
        std::ofstream out(savedPath, std::ios::binary);
        out << "invalid preferences";
    }
    require(!first.snapshot().at("error").get<std::string>().empty(), "Corrupt preferences were hidden");
    rejects([&] { first.update("python", custom, 0); }, "Corrupt preferences overwritten");
    require(readTextFileBytes(savedPath) == "invalid preferences",
            "Corrupt preferences changed after rejection");
}
static int delayedServer() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    size_t opened = 0, closed = 0;
    Json versions = Json::object();
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
        const auto method = request.value("method", "");
        if (method == "textDocument/didClose") ++closed;
        if (method == "textDocument/didOpen" || method == "textDocument/didChange")
            versions[request.at("params").at("textDocument").at("uri").get<std::string>()] = request.at("params").at("textDocument").at("version");
        if (method == "textDocument/didOpen" && std::filesystem::exists("identity-diagnostics")) {
            const auto &document = request.at("params").at("textDocument");
            const auto publication = Json{{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"},
                {"params", {{"uri", document.at("uri")}, {"version", document.at("version")},
                    {"diagnostics", Json::array({{{"range", {{"start", {{"line", 0}, {"character", 0}}},
                        {"end", {{"line", 0}, {"character", 1}}}}}, {"message", "identity fixture"}}})}}}}.dump();
            std::cout << "Content-Length: " << publication.size() << "\r\n\r\n" << publication << std::flush;
        }
        if (!request.contains("id"))
            continue;
        Json result;
        if (request.value("method", "") == "initialize")
            result = {{"capabilities", {{"positionEncoding", "utf-16"}}}};
        else if (request.value("method", "") == "test/received")
            result = {{"opened", opened}, {"closed", closed}, {"versions", versions}};
        else if (request.value("method","")=="textDocument/rename" && std::filesystem::exists("versioned-edits"))
            result={{"documentChanges",Json::array({{{"textDocument",{{"uri",request.at("params").at("textDocument").at("uri")},{"version",std::filesystem::exists("stale-version") ? 0 : 1}}},{"edits",Json::array()}}})}};
        else if (method=="workspace/symbol" && std::filesystem::exists("support-workspace-resolve")) {
            result=Json::array({{{"name","Lazy"},{"kind",5},
                {"location",{{"uri",LanguageClient::uri(std::filesystem::current_path()/"main.cpp")}}},
                {"data",{{"fixture",17}}}}});
            if (std::filesystem::exists("oversized-symbol")) result.at(0)["data"]["padding"]=std::string(65536,'x');
        }
        else {
            // Resolve handshake files live outside the observed source tree so
            // the fixture does not invalidate its own filesystem guard.
            const auto control=method=="workspaceSymbol/resolve" ? std::filesystem::path(".cycode/symbol-resolve-control") : std::filesystem::path(".");
            {
                std::ofstream marker(control/"request-received");
                marker << request["id"];
            }
            while (!std::filesystem::exists(control/"release-response"))
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (std::filesystem::exists("support-navigation") &&
                (method=="textDocument/declaration" || method=="textDocument/typeDefinition" || method=="textDocument/implementation")) {
                const auto range=[](int a,int b) { return Json{{"start",{{"line",0},{"character",a}}},{"end",{{"line",0},{"character",b}}}}; };
                const Json link={{"targetUri",request.at("params").at("textDocument").at("uri")},
                    {"targetSelectionRange",range(0,3)},{"targetRange",range(0,10)},{"originSelectionRange",range(0,3)}};
                result=Json::array({link,link});
            }
            if (method=="textDocument/documentHighlight" && std::filesystem::exists("support-navigation")) {
                const Json span={{"start",{{"line",0},{"character",0}}},{"end",{{"line",0},{"character",3}}}};
                result=Json::array({{{"range",span},{"kind",2}},{{"range",span},{"kind",3}}});
            }
            if (method=="workspaceSymbol/resolve") {
                result=request.at("params");
                require(result.at("data").at("fixture")==17 && !result.contains("cycodeLanguage"),"Resolve lost opaque data or leaked routing metadata");
                result["location"]["range"]={{"start",{{"line",0},{"character",0}}},{"end",{{"line",0},{"character",3}}}};
                if (std::filesystem::exists("wrong-symbol-uri")) result["location"]["uri"]=LanguageClient::uri(std::filesystem::current_path()/"other.cpp");
                if (std::filesystem::exists("missing-symbol-range")) result["location"].erase("range");
                std::ofstream count(control/"resolve-count",std::ios::app); count << "resolved\n";
            }
            if (request.value("method","")=="textDocument/semanticTokens/full") result={{"data",Json::array({0,4,5,0,0})}};
            if (request.value("method","")=="textDocument/hover") result={{"contents","Old documentation"}};
            if (request.value("method","")=="textDocument/signatureHelp") result={{"signatures",Json::array({{{"label","f(int value)"}}})}};
        }
        if (request.value("method","")=="initialize" && std::filesystem::exists("support-tokens"))
            result["capabilities"]["semanticTokensProvider"]={{"full",true},{"legend",{{"tokenTypes",Json::array({"variable"})},{"tokenModifiers",Json::array()}}}};
        if (request.value("method","")=="initialize" && std::filesystem::exists("versioned-edits")) result["capabilities"]["renameProvider"]=true;
        if (request.value("method","")=="initialize" && std::filesystem::exists("support-info")) { result["capabilities"]["hoverProvider"]=true;result["capabilities"]["signatureHelpProvider"]=Json::object(); }
        if (method=="initialize" && std::filesystem::exists("support-workspace-resolve")) {
            require(request.at("params").at("capabilities").at("workspace").at("symbol").at("resolveSupport").at("properties")==Json::array({"location.range"}),"Client omitted workspace lazy-range capability");
            result["capabilities"]["workspaceSymbolProvider"]=std::filesystem::exists("unsupported-resolve") ? Json(true) : Json{{"resolveProvider",true}};
        }
        if (method=="initialize" && std::filesystem::exists("support-navigation") && !std::filesystem::exists("unsupported-navigation")) {
            require(request.at("params").at("capabilities").at("textDocument").at("documentHighlight").at("dynamicRegistration")==false,"Highlight registration capability missing");
            result["capabilities"]["documentHighlightProvider"]=true;
            for (const auto *key:{"declaration","typeDefinition","implementation"}) {
                require(request.at("params").at("capabilities").at("textDocument").at(key).at("linkSupport")==true,"Navigation link support was not advertised");
                result["capabilities"][std::string(key)+"Provider"]=true;
            }
        }
        if (method == "initialize" && std::filesystem::exists("support-structure")) result["capabilities"]["documentSymbolProvider"] = true;
        auto reply = Json{{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", result}}.dump();
        std::cout << "Content-Length: " << reply.size() << "\r\n\r\n" << reply << std::flush;
        if (request.value("method", "") == "initialize")
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}
static void documentIdentityChecks(const std::filesystem::path &parent, const std::string &executable) {
    const auto root = parent / "identity-lifecycle";
    std::filesystem::create_directory(root);
    { std::ofstream marker(root / "identity-diagnostics"); }
    LanguageService service(root);
    service.configure(executable, true, true);
    const auto uri = LanguageClient::uri(root / "same.cpp");
    auto received = [&]() {
        auto future = service.request("test/received", Json::object());
        require(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "Identity fixture timed out");
        return future.get();
    };
    service.sync("same.cpp", "int first;", 1, 11);
    const auto first = received();
    // Simulate close/open coalescing in the worker queue: path/revision unchanged,
    // owner identity changed. The old publication must not match this new owner.
    service.sync("same.cpp", "int second;", 1, 12);
    const auto second = received();
    require(second.at("opened") == 2 && second.at("closed") == 1 &&
                second.at("versions").at(uri) > first.at("versions").at(uri),
            "Replacement document reused an LSP lifetime/version");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool current = false;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = service.status();
        const auto &revisions = status.at("diagnosticRevisions");
        if (revisions.contains("same.cpp") && revisions.at("same.cpp").value("id", uint64_t(0)) == 12) {
            current = revisions.at("same.cpp").at("revision") == 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    require(current, "Diagnostic publication did not carry the replacement identity");
    service.close("same.cpp");
    received();
    service.sync("same.cpp", "int third;", 1, 13);
    const auto third = received();
    require(third.at("opened") == 3 && third.at("closed") == 2 &&
                third.at("versions").at(uri) > second.at("versions").at(uri),
            "Closed/reopened document reused an LSP version");
}
static void structureLifecycleChecks(const std::filesystem::path &parent, const std::string &executable) {
    for (const auto *scenario : {"unrelated", "edited", "closed", "replaced", "disabled"}) {
        const auto root = parent / (std::string("structure-") + scenario);
        std::filesystem::create_directory(root);
        { std::ofstream file(root / "main.cpp"); file << "int first;\n"; }
        { std::ofstream file(root / "other.cpp"); file << "int other;\n"; }
        { std::ofstream marker(root / "support-structure"); }
        Engine engine(root);
        engine.configureLanguage(executable, true, true);
        const auto doc = engine.dispatch("documents.read", {{"path", "main.cpp"}});
        const auto other = engine.dispatch("documents.read", {{"path", "other.cpp"}});
        auto query = std::async(std::launch::async, [&] {
            return engine.dispatch("language.documentSymbols", {{"path", "main.cpp"},
                {"expectedRevision", doc.at("revision")}, {"expectedDocumentId", doc.at("id")}});
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!std::filesystem::exists(root / "request-received") && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        require(std::filesystem::exists(root / "request-received"), "Structure fixture received no request");
        const std::string kind = scenario;
        if (kind == "unrelated" || kind == "edited") {
            const auto &changed = kind == "unrelated" ? other : doc;
            engine.dispatch("documents.replace", {{"path", changed.at("path")}, {"expectedRevision", changed.at("revision")}, {"text", "int changed;\n"}});
        } else if (kind == "closed" || kind == "replaced") {
            engine.dispatch("documents.close", {{"path", "main.cpp"}, {"expectedRevision", doc.at("revision")}});
            if (kind == "replaced") engine.dispatch("documents.read", {{"path", "main.cpp"}});
        } else engine.configureLanguage("", false, false);
        { std::ofstream release(root / "release-response"); }
        require(query.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "Structure reply did not settle");
        bool accepted = false;
        try {
            const auto report = query.get(); accepted = true;
            require(report.at("documentId") == doc.at("id") && report.at("revision") == doc.at("revision"), "Structure reply lost ownership");
        } catch (const std::exception &) { if (kind == "unrelated") throw; }
        require(accepted == (kind == "unrelated"), "Structure response accepted stale ownership or rejected an unrelated edit");
    }
}
static void workspaceSymbolResolveChecks(const std::filesystem::path &parent,const std::string &executable) {
    for (const auto *scenario:{"stable","edited","closed","disabled","wrong-symbol-uri","missing-symbol-range","oversized-symbol","evicted","unsupported-resolve"}) {
        const std::string kind=scenario;
        const auto root=parent/("symbol-resolve-"+kind); std::filesystem::create_directory(root);
        const auto control=root/".cycode/symbol-resolve-control"; std::filesystem::create_directories(control);
        { std::ofstream file(root/"main.cpp"); file << "int value;\n"; }
        { std::ofstream marker(root/"support-workspace-resolve"); }
        if (kind=="wrong-symbol-uri" || kind=="missing-symbol-range" || kind=="oversized-symbol" || kind=="unsupported-resolve") { std::ofstream marker(root/kind); }
        Engine engine(root); engine.configureLanguage(executable,true,true);
        const auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
        const auto report=engine.dispatch("language.symbols",{{"query","Lazy"}});
        if (kind=="oversized-symbol") {
            require(report.at("invalid")==1 && report.at("symbols").empty() && !report.contains("session"),"Oversized opaque symbol data escaped the cache bound");
            continue;
        }
        const auto &row=report.at("symbols").at(0);
        require(row.at("unresolved")==true && !row.contains("range") && !row.contains("fullRange"),"URI-only symbol acquired a fabricated position");
        Json parameters={{"session",report.at("session")},{"index",row.at("resolveIndex")},{"expectedEpoch",report.at("epoch")}};
        auto rejects=[&](const Json &value) { bool rejected=false;try { engine.dispatch("language.symbolResolve",value); } catch (const std::exception &) { rejected=true; } require(rejected,"Unissued or stale symbol reference was accepted"); };
        auto forged=parameters; forged["index"]=999; rejects(forged); forged["session"]="ws-symbol-999999"; rejects(forged);
        if (kind=="unsupported-resolve") { rejects(parameters); continue; }
        if (kind=="evicted") {
            for (int i=0;i<4;++i) engine.dispatch("language.symbols",{{"query","Lazy"}});
            rejects(parameters); continue;
        }
        auto query=std::async(std::launch::async,[&] { return engine.dispatch("language.symbolResolve",parameters); });
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while (!std::filesystem::exists(control/"request-received") && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        require(std::filesystem::exists(control/"request-received"),"Resolve fixture received no request");
        if (kind=="edited") engine.dispatch("documents.replace",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"text","int changed;\n"}});
        if (kind=="closed") engine.dispatch("documents.close",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")}});
        if (kind=="disabled") engine.configureLanguage("",false,false);
        { std::ofstream release(control/"release-response"); }
        require(query.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Resolve reply did not settle");
        bool accepted=false;
        try {
            const auto resolved=query.get(); accepted=true;
            require(resolved.at("symbol").at("path")=="main.cpp" && resolved.at("symbol").at("node")==row.at("node") &&
                    resolved.at("symbol").at("range").at("end").at("character")==3 && !resolved.at("symbol").contains("unresolved"),"Resolved symbol lost its issued identity or location");
            require(engine.dispatch("language.symbolResolve",parameters)==resolved,"Cached resolution changed its result");
            std::ifstream count(control/"resolve-count"); std::string text((std::istreambuf_iterator<char>(count)),{});
            require(text=="resolved\n","Cached resolution sent another server request");
        } catch (const std::exception &) { if (kind=="stable") throw; }
        require(accepted==(kind=="stable"),"Resolution accepted a changed context or an invalid destination");
    }
}
static void navigationLifecycleChecks(const std::filesystem::path &parent,const std::string &executable) {
    for (const auto *scenario:{"declaration","typeDefinition","implementation","unrelated","edited","closed","replaced","disabled","unsupported"}) {
        const std::string kind=scenario;
        const auto root=parent/("navigation-"+kind); std::filesystem::create_directory(root);
        { std::ofstream file(root/"main.cpp"); file << "int first;\n"; }
        { std::ofstream file(root/"other.cpp"); file << "int other;\n"; }
        { std::ofstream marker(root/"support-navigation"); }
        if (kind=="unsupported") { std::ofstream marker(root/"unsupported-navigation"); }
        Engine engine(root); engine.configureLanguage(executable,true,true);
        const auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
        const auto other=engine.dispatch("documents.read",{{"path","other.cpp"}});
        const std::string method="language."+(kind=="declaration" || kind=="typeDefinition" ? kind : "implementation");
        const Json parameters={{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"expectedDocumentId",doc.at("id")},
            {"expectedEpoch",engine.languageStatus().at("epoch")},{"position",{{"line",0},{"character",1}}}};
        auto rejects=[&](const Json &value) { bool failed=false;try { engine.dispatch(method,value); } catch (const std::exception &) { failed=true; } require(failed,"Invalid navigation caller snapshot was accepted"); };
        auto invalid=parameters; invalid["expectedDocumentId"]=0; rejects(invalid);
        invalid=parameters; invalid["expectedEpoch"]=0; rejects(invalid);
        invalid=parameters; invalid["position"]["character"]=true; rejects(invalid);
        invalid=parameters; invalid["expectedRevision"]=doc.at("revision").get<double>()+.25; rejects(invalid);
        if (kind=="unsupported") { rejects(parameters); continue; }
        auto query=std::async(std::launch::async,[&] { return engine.dispatch(method,parameters); });
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while (!std::filesystem::exists(root/"request-received") && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        require(std::filesystem::exists(root/"request-received"),"Navigation fixture received no request");
        if (kind=="edited" || kind=="unrelated") {
            const auto &changed=kind=="unrelated" ? other : doc;
            engine.dispatch("documents.replace",{{"path",changed.at("path")},{"expectedRevision",changed.at("revision")},{"text","int changed;\n"}});
        }
        if (kind=="closed" || kind=="replaced") {
            engine.dispatch("documents.close",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")}});
            if (kind=="replaced") engine.dispatch("documents.read",{{"path","main.cpp"}});
        }
        if (kind=="disabled") engine.configureLanguage("",false,false);
        { std::ofstream release(root/"release-response"); }
        require(query.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Navigation reply did not settle");
        const bool expected=kind=="declaration" || kind=="typeDefinition" || kind=="implementation" || kind=="unrelated";
        bool accepted=false;
        try {
            const auto result=query.get(); accepted=true;
            require(result.at("documentId")==doc.at("id") && result.at("revision")==doc.at("revision") && result.at("method")==method &&
                result.at("locations").size()==1 && result.at("invalid")==0 && result.at("locations").at(0).contains("originRange"),"Navigation lost ownership, link data or duplicate filtering");
        } catch (const std::exception &) { if (expected) throw; }
        require(accepted==expected,"Navigation accepted a changed source/backend or rejected an unrelated edit");
    }
}
static void occurrenceLifecycleChecks(const std::filesystem::path &parent,const std::string &executable) {
    for (const auto *scenario:{"stable","unrelated","edited","closed","replaced","disabled","unsupported"}) {
        const std::string kind=scenario;
        const auto root=parent/("occurrences-"+kind); std::filesystem::create_directory(root);
        { std::ofstream file(root/"main.cpp"); file << "int first;\n"; }
        { std::ofstream file(root/"other.cpp"); file << "int other;\n"; }
        { std::ofstream marker(root/"support-navigation"); }
        if (kind=="unsupported") { std::ofstream marker(root/"unsupported-navigation"); }
        Engine engine(root); engine.configureLanguage(executable,true,true);
        const auto doc=engine.dispatch("documents.read",{{"path","main.cpp"}});
        const auto other=engine.dispatch("documents.read",{{"path","other.cpp"}});
        const std::string method="language.documentHighlights";
        const Json parameters={{"path","main.cpp"},{"expectedRevision",doc.at("revision")},{"expectedDocumentId",doc.at("id")},
            {"expectedEpoch",engine.languageStatus().at("epoch")},{"position",{{"line",0},{"character",1}}}};
        auto rejects=[&](const Json &value) { bool failed=false;try { engine.dispatch(method,value); } catch (const std::exception &) { failed=true; } require(failed,"Invalid navigation caller snapshot was accepted"); };
        auto invalid=parameters; invalid["expectedDocumentId"]=0; rejects(invalid);
        invalid=parameters; invalid["expectedEpoch"]=0; rejects(invalid);
        invalid=parameters; invalid["position"]["character"]=true; rejects(invalid);
        invalid=parameters; invalid["expectedRevision"]=doc.at("revision").get<double>()+.25; rejects(invalid);
        if (kind=="unsupported") { rejects(parameters); continue; }
        auto query=std::async(std::launch::async,[&] { return engine.dispatch(method,parameters); });
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while (!std::filesystem::exists(root/"request-received") && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        require(std::filesystem::exists(root/"request-received"),"Navigation fixture received no request");
        if (kind=="edited" || kind=="unrelated") {
            const auto &changed=kind=="unrelated" ? other : doc;
            engine.dispatch("documents.replace",{{"path",changed.at("path")},{"expectedRevision",changed.at("revision")},{"text","int changed;\n"}});
        }
        if (kind=="closed" || kind=="replaced") {
            engine.dispatch("documents.close",{{"path","main.cpp"},{"expectedRevision",doc.at("revision")}});
            if (kind=="replaced") engine.dispatch("documents.read",{{"path","main.cpp"}});
        }
        if (kind=="disabled") engine.configureLanguage("",false,false);
        { std::ofstream release(root/"release-response"); }
        require(query.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Navigation reply did not settle");
        const bool expected=kind=="stable" || kind=="unrelated";
        bool accepted=false;
        try {
            const auto result=query.get(); accepted=true;
            require(result.at("documentId")==doc.at("id") && result.at("revision")==doc.at("revision") && result.at("occurrences").size()==1 && result.at("invalid")==0 && result.at("occurrences").at(0).at("kind")==3,"Navigation lost ownership, link data or duplicate filtering");
        } catch (const std::exception &) { if (expected) throw; }
        require(accepted==expected,"Navigation accepted a changed source/backend or rejected an unrelated edit");
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
        documentIdentityChecks(root, pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))));
        structureLifecycleChecks(root, pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))));
        workspaceSymbolResolveChecks(root, pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))));
        navigationLifecycleChecks(root, pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))));
        occurrenceLifecycleChecks(root, pathUtf8(std::filesystem::absolute(utf8Path(argv[0]))));
        preferencesChecks(root);
        rustAnalysisContracts(root);
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
