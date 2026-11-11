#include "cycode/Bridge.h"
#ifdef CYCODE_PRIVATE_DIAGNOSTICS
#include "RecoveryDiagnostic.h"
#endif
#include "cycode/NativeTrace.h"
#include "cycode/Tools.h"
#include "cycode/Updates.h"
#include <iostream>
#include <limits>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif
int main(int argc, char **argv) {
    try {
        std::vector<std::string> arguments;
#ifdef _WIN32
        int count = 0;
        auto wide = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!wide)
            throw std::runtime_error("Cannot read command line");
        for (int i = 0; i < count; ++i) {
            const auto length = int(wcslen(wide[i]));
            const auto bytes = WideCharToMultiByte(CP_UTF8, 0, wide[i], length, nullptr, 0, nullptr, nullptr);
            std::string value(bytes, 0); WideCharToMultiByte(CP_UTF8, 0, wide[i], length, value.data(), bytes, nullptr, nullptr);
            arguments.push_back(std::move(value));
        }
        LocalFree(wide);
#else
        for (int i = 0; i < argc; ++i)
            arguments.emplace_back(argv[i]);
#endif
        bool disablePlugins=false, diagnosticTrace=false;
#ifdef CYCODE_PRIVATE_DIAGNOSTICS
        bool diagnosticRecoveryPause=false;
#else
        for (const auto &argument : arguments)
            if (argument.starts_with("--diagnostic-") && argument != "--diagnostic-trace")
                throw std::runtime_error("Private recovery diagnostics are not compiled into this distribution");
#endif
        while(arguments.size()>1) {
            if(arguments[1]=="--disable-plugins") disablePlugins=true;
            else if(arguments[1]=="--diagnostic-trace") diagnosticTrace=true;
#ifdef CYCODE_PRIVATE_DIAGNOSTICS
            else if(arguments[1]=="--diagnostic-recovery-pause") diagnosticRecoveryPause=true;
#endif
            else break;
            arguments.erase(arguments.begin()+1);
        }
        argc = static_cast<int>(arguments.size());
#ifdef CYCODE_PRIVATE_DIAGNOSTICS
        if (diagnosticRecoveryPause && !diagnosticTrace)
            throw std::runtime_error("--diagnostic-recovery-pause requires --diagnostic-trace");
#endif
        if (argc == 2 && arguments[1] == "--version") {
            std::cout << "CyCode " << CYCODE_VERSION << '\n';
            return 0;
        }
        if (argc < 3) {
            std::cerr << "Usage: cycode_cli [--disable-plugins] [--diagnostic-trace] WORKSPACE METHOD [PARAMETERS_JSON]\n       cycode_cli WORKSPACE "
                         "--serve DESCRIPTOR\n       cycode_cli WORKSPACE --stdio\n";
            return 2;
        }
        if(diagnosticTrace) cycode::enableNativeDiagnosticTrace();
#ifdef CYCODE_PRIVATE_DIAGNOSTICS
        if(diagnosticRecoveryPause) cycode::armNativeRecoveryCleanupPause();
        if (arguments[2] == "--diagnostic-recovery-only" || arguments[2] == "--diagnostic-files-only") {
            if (!diagnosticTrace || argc != 3)
                throw std::runtime_error("Owned recovery diagnostics require --diagnostic-trace and exactly one workspace");
            return cycode::runOwnedRecoveryDiagnostic(cycode::utf8Path(arguments[1]),
                arguments[2] == "--diagnostic-files-only");
        }
        if (arguments[2] == "--diagnostic-file-operation") {
            if (!diagnosticTrace || diagnosticRecoveryPause || argc != 4)
                throw std::runtime_error("Owned operation diagnostic requires trace, workspace and one operation; no pause flag");
            return cycode::runOwnedFileOperationDiagnostic(cycode::utf8Path(arguments[1]), arguments[3]);
        }
#endif
        const auto installation=cycode::installationRoot();
        const auto installationLease=installation.empty() ? nullptr : cycode::updates::installationUse(installation);
        cycode::Engine engine(cycode::utf8Path(arguments[1]), installation.empty() ? std::filesystem::path{} : installation / "data/settings/language-servers",
            installation.empty() ? std::filesystem::path{} : installation / "data/settings/snippets");
        if(!disablePlugins&&!installation.empty())engine.plugins().loadInstalled(installation);
        if (arguments[2] == "--stdio") {
            // One Engine and its backend workers survive every request until
            // explicit shutdown or EOF. stdout contains JSON lines only.
            cycode::CoreService service(engine);
            std::cout << cycode::Json({{"event","ready"},{"protocol",cycode::CoreService::protocol},
                                      {"status",service.status()}}).dump() << '\n' << std::flush;
            std::string line;
            while (std::cin.good()) {
                line.clear();
                char c=0;
                bool oversized=false, complete=false;
                while (std::cin.get(c)) {
                    if (c=='\n') {complete=true;break;}
                    if (line.size()<cycode::CoreService::maximumRequestBytes) line.push_back(c);
                    else oversized=true;
                }
                if (!complete && line.empty() && !oversized) break;
                cycode::Json response;
                bool shutdown=false;
                try {
                    if (oversized) response=cycode::CoreService::error("RequestTooLarge","Native request exceeds 8 MiB");
                    else {
                        const auto input=cycode::Json::parse(line);
                        // A host control operation, deliberately absent from
                        // the authenticated legacy socket/MCP dispatch surface.
                        if (input.is_object() && input.value("method",cycode::Json())=="service.shutdown") {
                            auto validation=input;
                            validation["method"]="service.status";
                            response=service.respond(validation);
                            if (response.contains("result")) {
                                service.beginShutdown();shutdown=true;
                                response["result"]=service.status();
                            }
                        } else response=service.respond(input);
                    }
                } catch (const cycode::Json::exception &e) {response=cycode::CoreService::error("InvalidJson",e.what());}
                std::cout << cycode::encodeNativeResponse(response) << std::flush;
                if (shutdown) break;
            }
            service.beginShutdown();
            return 0;
        }
        if (arguments[2] == "--serve") {
            if (argc < 4)
                return 2;
            cycode::Bridge bridge(engine, cycode::utf8Path(arguments[3]));
            std::cout << "ready\n" << std::flush;
            std::string line;
            std::getline(std::cin, line);
            return 0;
        }
        auto params = argc > 3 ? cycode::Json::parse(arguments[3]) : cycode::Json::object();
        std::cout << engine.dispatch(arguments[2], params)
                         .dump(2, ' ', false, cycode::Json::error_handler_t::replace)
                  << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << cycode::Json({{"error", e.what()}}).dump() << '\n';
        return 1;
    }
}
