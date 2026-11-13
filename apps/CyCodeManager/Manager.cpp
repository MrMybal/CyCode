// Thin CyUI desktop client. Configuration, dependencies and updates retain their
// existing application service owners; no Engine/editor is created by the GUI.
#include "ManagerLocale.h"
#include <cycode/Process.h>
#include <cycode/Tools.h>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <windows.h>
#include <d3d11.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
using cyui::tr;
using cyui::trLabel;
ID3D11Device* device{};
ID3D11DeviceContext* context{};
IDXGISwapChain* swapChain{};
ID3D11RenderTargetView* target{};
UINT width{}, height{};
float dpi = 1.f;

void releaseTarget() { if (target) { target->Release(); target = nullptr; } }
void makeTarget() {
    ID3D11Texture2D* texture{};
    if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&texture)))) {
        device->CreateRenderTargetView(texture, nullptr, &target);
        texture->Release();
    }
}
LRESULT WINAPI windowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, w, l)) return 1;
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(l);
        info->ptMinTrackSize = {LONG(700 * dpi), LONG(500 * dpi)}; return 0;
    }
    case WM_SIZE: if (w != SIZE_MINIMIZED) { width = LOWORD(l); height = HIWORD(l); } return 0;
    case WM_DPICHANGED: {
        dpi = float(HIWORD(w)) / 96.f;
        const auto* rectangle = reinterpret_cast<RECT*>(l);
        SetWindowPos(window, nullptr, rectangle->left, rectangle->top, rectangle->right - rectangle->left,
            rectangle->bottom - rectangle->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
    }
    case WM_CLOSE: PostMessageW(window, WM_APP + 1, 0, 0); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_SYSCOMMAND: if ((w & 0xfff0) == SC_KEYMENU) return 0; break;
    }
    return DefWindowProcW(window, message, w, l);
}
void openLink(const std::string& value) {
    const auto wide = cycode::utf8Path(value).wstring();
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
std::string chooseFolder(HWND owner) {
    IFileOpenDialog* dialog{};
    std::string result;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options{}; dialog->GetOptions(&options); dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item{};
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path{};
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = cycode::pathUtf8(fs::path(path)); CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    return result;
}
struct Reply { std::string action; Json value; std::string error; };
struct Progress { std::mutex mutex; Json value = Json::object(); };
struct Manager {
    fs::path home, root, state, cache, temporary, cancellation;
    Json snapshot = Json::object(), preview = Json::object(), release = nullptr;
    std::future<Reply> worker;
    std::shared_ptr<Progress> progress = std::make_shared<Progress>();
    std::array<char, 32768> project{};
    std::array<bool, 16> clients{};
    int page = 0, scope = 0, access = 0, profile = 0, connection = -1, theme = 0;
    bool skill = true, mcp = true, refreshConnections = true, acceptLicense = false, compact = false;
    bool closing = false, startupCheck = true;
    bool explicitLanguage = false, explicitPage = false, preferencesLoaded = false, preferencesDirty = false;
    std::string status = "Ready", error, activeAction;

    Manager() {
        clients[0] = true;
        GUID guid{}; if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Cannot allocate Manager session");
        wchar_t text[40]{}; StringFromGUID2(guid, text, 40);
        temporary = fs::temp_directory_path() / (L"CyCodeManager-" + std::wstring(text));
        if (!fs::create_directory(temporary)) throw std::runtime_error("Manager session directory already exists");
        cancellation = temporary / "cancel";
    }
    ~Manager() { if (worker.valid()) worker.wait(); std::error_code ec; fs::remove(cancellation, ec); fs::remove(temporary, ec); }
    bool busy() const { return worker.valid(); }
    void cancel() { std::ofstream(cancellation) << "cancel\n"; status = "Stopping download. Complete or cancel any visible installer."; }
    void start(Json request) {
        if (busy()) return;
        fs::remove(cancellation);
        error.clear(); status = "Working";
        { std::lock_guard lock(progress->mutex); progress->value = Json::object(); }
        const auto action = request.at("action").get<std::string>();
        activeAction = action;
        std::vector<std::string> argv{cycode::pathUtf8(cycode::mcpInterpreterPath(home)), "-B", "-X", "utf8",
            cycode::pathUtf8(home / "app/manager_backend.py"), "--home", cycode::pathUtf8(home),
            "--cancel-file", cycode::pathUtf8(cancellation), "--request", request.dump()};
        if (!root.empty()) { argv.push_back("--installation-root"); argv.push_back(cycode::pathUtf8(root)); }
        if (!state.empty()) { argv.push_back("--state"); argv.push_back(cycode::pathUtf8(state)); }
        if (!cache.empty()) { argv.push_back("--cache"); argv.push_back(cycode::pathUtf8(cache)); }
        const auto cwd = home;
        const auto observation = progress;
        worker = std::async(std::launch::async, [argv, cwd, action, observation] {
            Reply reply{action, Json::object(), {}};
            try {
                std::string pending;
                auto result = cycode::runProcess(argv, cwd, std::chrono::minutes(60), 4 * 1024 * 1024,
                    nullptr, {{"PYTHONDONTWRITEBYTECODE", "1"}, {"GIT_OPTIONAL_LOCKS", "0"}},
                    [&](std::string_view chunk) {
                        pending.append(chunk);
                        std::size_t end;
                        while ((end = pending.find('\n')) != std::string::npos) {
                            const auto row = Json::parse(pending.substr(0, end), nullptr, false);
                            pending.erase(0, end + 1);
                            if (!row.is_discarded() && row.value("event", "") == "progress") {
                                std::lock_guard lock(observation->mutex); observation->value = row.at("value");
                            }
                        }
                        if (pending.size() > 4 * 1024 * 1024) pending.clear();
                    });
                std::istringstream lines(result.output); std::string line; bool found = false;
                while (std::getline(lines, line)) {
                    const auto row = Json::parse(line, nullptr, false);
                    if (row.is_discarded()) continue;
                    if (row.value("event", "") == "result") { reply.value = row.at("value"); found = true; }
                    if (row.value("event", "") == "error") reply.error = row.value("message", "Manager operation failed");
                }
                if (reply.error.empty() && (result.exitCode != 0 || result.timedOut || result.truncated || !found))
                    reply.error = "Manager service did not complete: " + result.output.substr(0, 2048);
            } catch (const std::exception& exception) { reply.error = exception.what(); }
            return reply;
        });
    }
    void poll() {
        if (!busy() || worker.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;
        auto reply = worker.get();
        if (closing) return;
        if (!reply.error.empty()) { error = reply.error; status = "Ready"; return; }
        status = "Ready";
        if (reply.action == "snapshot") {
            snapshot = std::move(reply.value);
            if (!preferencesLoaded) {
                preferencesLoaded = true;
                const auto settings = snapshot.value("preferences", Json::object());
                const char* themes[]{"navy", "mid-grey", "light-grey", "amber-night", "violet"};
                for (int i = 0; i < 5; ++i) if (settings.value("theme", "navy") == themes[i]) theme = i;
                compact = settings.value("compact", false);
                cyui::setTheme(themes[theme]); cyui::setDensity(compact ? cyui::Density::compact : cyui::Density::comfortable);
                if (!explicitLanguage) cyui::setLocale(settings.value("language", "en").c_str());
                if (!explicitPage) for (const auto& tool : snapshot.value("dependencies", Json::array()))
                    if (tool.value("id", "") == "clangd" && tool.value("state", "") == "missing") page = 1;
            }
            if (startupCheck) { startupCheck = false; start({{"action", "updates.check"}}); }
        } else if (reply.action == "preferences.save") {
            // This is UI state only; no connection refresh or notification.
        } else if (reply.action == "updates.check") release = reply.value.value("release", Json(nullptr));
        else if (reply.action == "agents.prepare" || reply.action == "agents.refresh" || reply.action == "agents.remove") preview = reply.value;
        else {
            preview = Json::object();
            if (reply.action == "updates.install" && reply.value.contains("home")) home = cycode::utf8Path(reply.value.at("home").get<std::string>());
            cyui::notify(tr(reply.action == "dependencies.install" ? "Reload MCP clients after selecting a new dependency." : "Changes applied. Reload the affected clients.").c_str(), cyui::Tone::success);
            start({{"action", "snapshot"}});
        }
    }
    void agents(HWND window) {
        cyui::heading(tr("Agents and skills").c_str(), 1);
        cyui::textDim("%s", tr("Skill instructions and Core tools are separate. Reload your client after applying a connection.").c_str());
        bool changed = false;
        ImGui::BeginDisabled(busy());
        if (cyui::beginCard("agent-config")) {
            const auto available = snapshot.value("clients", Json::array());
            for (std::size_t i = 0; i < available.size() && i < clients.size(); ++i) {
                if (i % 3) ImGui::SameLine();
                changed |= ImGui::Checkbox(available[i].at("name").get<std::string>().c_str(), &clients[i]);
            }
            if (cyui::beginProperties("connection-properties")) {
                cyui::property(tr("Project").c_str());
                ImGui::SetNextItemWidth(-cyui::dp(105.f)); changed |= ImGui::InputText("##project", project.data(), project.size()); ImGui::SameLine();
                if (cyui::button(trLabel("Browse").c_str())) { const auto path = chooseFolder(window); if (!path.empty()) { strcpy_s(project.data(), project.size(), path.c_str()); changed = true; } }
                auto a = tr("This project"), b = tr("My user account"); const char* scopes[]{a.c_str(), b.c_str()};
                cyui::property(tr("Scope").c_str()); changed |= ImGui::Combo("##scope", &scope, scopes, 2);
                a = tr("Read only"); b = tr("Read, edit and build"); const char* rights[]{a.c_str(), b.c_str()};
                cyui::property(tr("Access").c_str()); changed |= ImGui::Combo("##access", &access, rights, 2);
                a = tr("Code (compact)"); b = tr("Full catalogue"); const char* profiles[]{a.c_str(), b.c_str()};
                cyui::property(tr("MCP catalogue").c_str()); changed |= ImGui::Combo("##profile", &profile, profiles, 2);
                cyui::endProperties();
            }
            changed |= ImGui::Checkbox(trLabel("Install skill").c_str(), &skill); ImGui::SameLine();
            changed |= ImGui::Checkbox(trLabel("Configure MCP").c_str(), &mcp);
            cyui::textDim("%s", tr("Code exposes common tools. Other operations remain available on demand.").c_str());
            if (changed) preview = Json::object();
            if (cyui::primaryButton(trLabel("Preview changes").c_str())) {
                Json selected = Json::array();
                for (std::size_t i = 0; i < available.size() && i < clients.size(); ++i) if (clients[i]) selected.push_back(available[i]["id"]);
                start({{"action", "agents.prepare"}, {"clients", selected}, {"project", project.data()}, {"scope", scope ? "user" : "project"},
                    {"access", access ? "edit" : "read"}, {"skill", skill}, {"mcp", mcp}, {"mcpProfile", profile ? "full" : "code"}});
            }
        } cyui::endCard();
        if (preview.contains("plan")) {
            if (cyui::beginCard("preview")) {
                for (const auto& file : preview.value("files", Json::array())) ImGui::TextWrapped("%s", file.get<std::string>().c_str());
                for (const auto& skipped : preview.value("skipped", Json::array())) ImGui::TextWrapped("%s", skipped.get<std::string>().c_str());
                if (cyui::button(trLabel("Apply reviewed changes").c_str())) start({{"action", "agents.apply"}, {"plan", preview.at("plan")}});
            } cyui::endCard();
        }
        cyui::sectionTitle(tr("Managed connections").c_str());
        const auto connections = snapshot.value("connections", Json::array());
        if (connections.empty()) cyui::textDim("%s", tr("No managed connection").c_str());
        for (std::size_t i = 0; i < connections.size(); ++i) {
            const auto& row = connections[i];
            const auto label = row.value("client", "") + " | " + row.value("workspace", "") + "###connection-" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), connection == int(i))) connection = int(i);
        }
        if (cyui::button(trLabel("Refresh connections").c_str())) start({{"action", "agents.refresh"}});
        ImGui::SameLine(); ImGui::BeginDisabled(connection < 0 || connection >= int(connections.size()));
        if (cyui::dangerButton(trLabel("Remove connection").c_str())) start({{"action", "agents.remove"}, {"id", connections[connection].at("id")}});
        ImGui::EndDisabled(); ImGui::EndDisabled();
    }
    void dependencies() {
        cyui::heading(tr("Dependencies").c_str(), 1);
        cyui::textDim("%s", tr("An idle language server is not necessarily missing.").c_str());
        ImGui::BeginDisabled(busy());
        if (cyui::button(trLabel("Refresh").c_str())) start({{"action", "snapshot"}});
        for (const auto& tool : snapshot.value("dependencies", Json::array())) {
            const auto id = tool.at("id").get<std::string>();
            ImGui::PushID(id.c_str());
            if (cyui::beginCard("dependency", id.c_str())) {
                const bool installed = tool.value("state", "") == "installed";
                cyui::badge(tr(installed ? "Installed" : "Missing").c_str(), installed ? cyui::Tone::success : cyui::Tone::warning);
                ImGui::SameLine(); cyui::textDim("%s", tr(tool.at("requiredFor").get<std::string>().c_str()).c_str());
                if (tool.contains("selection") && tool["selection"].is_object()) ImGui::TextWrapped("%s", tool["selection"].value("path", "").c_str());
                if (!tool.value("issue", "").empty()) cyui::callout(cyui::Tone::warning, tool.at("issue").get<std::string>().c_str());
                if (id == "clangd") {
                    const auto& download = tool.at("download");
                    ImGui::Text("%s  |  %.1f MB", download.at("version").get<std::string>().c_str(), download.at("size").get<double>() / 1e6);
                    ImGui::TextWrapped("%s", tool.at("installationDirectory").get<std::string>().c_str());
                    ImGui::BeginDisabled(!tool.value("automaticInstall", false));
                    if (cyui::primaryButton(trLabel("Install private clangd").c_str())) { acceptLicense = false; ImGui::OpenPopup("install-clangd"); }
                    ImGui::EndDisabled();
                    if (ImGui::BeginPopupModal("install-clangd", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        cyui::heading(tr("Install clangd").c_str());
                        ImGui::TextWrapped("%s", tool.at("installationDirectory").get<std::string>().c_str());
                        ImGui::TextWrapped("%s", download.at("source").get<std::string>().c_str());
                        ImGui::TextWrapped("%s", tr("No global PATH or Windows installation is changed.").c_str());
                        ImGui::Checkbox(trLabel("I accept the Apache-2.0 license with LLVM exception").c_str(), &acceptLicense);
                        if (cyui::button(trLabel("Official downloads").c_str())) openLink(download.at("source"));
                        ImGui::BeginDisabled(!acceptLicense);
                        if (cyui::primaryButton(trLabel("Install").c_str())) { start({{"action", "dependencies.install"}, {"accepted", true}}); ImGui::CloseCurrentPopup(); }
                        ImGui::EndDisabled(); ImGui::SameLine();
                        if (cyui::button(trLabel("Cancel").c_str())) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                } else if (!installed && tool.contains("source")) {
                    if (cyui::button(trLabel("Official downloads").c_str())) openLink(tool.at("source"));
                }
            } cyui::endCard(); ImGui::PopID();
        }
        ImGui::EndDisabled();
    }
    void updates() {
        cyui::heading(tr("Updates").c_str(), 1);
        cyui::textDim("%s", tr("Existing versions and running sessions are preserved.").c_str());
        ImGui::BeginDisabled(busy());
        if (cyui::button(trLabel("Check for updates").c_str())) start({{"action", "updates.check"}});
        if (release.is_object()) {
            cyui::heading(release.value("version", "").c_str());
            ImGui::TextWrapped("%s", release.value("notes", "").c_str());
            ImGui::Checkbox(trLabel("Update managed connections after installation").c_str(), &refreshConnections);
            if (cyui::primaryButton(trLabel("Download and install").c_str())) ImGui::OpenPopup("confirm-update");
            ImGui::SameLine(); if (cyui::button(trLabel("Release notes").c_str())) openLink(release.at("page"));
        } else cyui::textDim("%s", tr("No update offered").c_str());
        if (ImGui::BeginPopupModal("confirm-update", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            cyui::heading(tr("Confirm update").c_str());
            ImGui::TextUnformatted(release.value("version", "").c_str());
            ImGui::TextWrapped("%s", tr("Existing versions and running sessions are preserved.").c_str());
            if (cyui::primaryButton(trLabel("Install").c_str())) { start({{"action", "updates.install"}, {"version", release.at("version")},
                {"accepted", true}, {"refresh", refreshConnections}, {"language", cyui::locale()}}); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine(); if (cyui::button(trLabel("Cancel").c_str())) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }
    void draw(HWND window) {
        poll();
        auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos); ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("CyCodeCore Manager", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        cyui::heading("CyCodeCore Manager", 1); ImGui::SameLine();
        cyui::badge(snapshot.value("localSourceChanges", false) ? tr("Local preview").c_str() : snapshot.value("version", "preview").c_str());
        ImGui::SameLine(); if (cyui::ghostButton(cyui::locale() == "fr" ? "English" : "Français")) {
            cyui::setLocale(cyui::locale() == "fr" ? "en" : "fr"); preferencesDirty = true;
        }
        cyui::divider();
        ImGui::BeginChild("navigation", cyui::dp(185.f, -ImGui::GetFrameHeightWithSpacing() / dpi - 18.f), ImGuiChildFlags_Borders);
        const char* pages[]{"Agents and skills", "Dependencies", "Updates"};
        for (int i = 0; i < 3; ++i) if (ImGui::Selectable(trLabel(pages[i]).c_str(), page == i)) page = i;
        cyui::space(20);
        const char* themes[]{"navy", "mid-grey", "light-grey", "amber-night", "violet"};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo(trLabel("Theme").c_str(), &theme, themes, 5)) { cyui::setTheme(themes[theme]); preferencesDirty = true; }
        if (ImGui::Checkbox(trLabel("Compact density").c_str(), &compact)) {
            cyui::setDensity(compact ? cyui::Density::compact : cyui::Density::comfortable); preferencesDirty = true;
        }
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("content", cyui::dp(0.f, -ImGui::GetFrameHeightWithSpacing() / dpi - 18.f));
        if (!error.empty()) { cyui::callout(cyui::Tone::error, error.c_str()); cyui::space(); }
        if (page == 0) agents(window); else if (page == 1) dependencies(); else updates();
        ImGui::EndChild(); cyui::divider();
        if (busy()) { cyui::spinner("busy"); ImGui::SameLine(); }
        ImGui::TextUnformatted(tr(status.c_str()).c_str());
        if (busy()) {
            Json observed; { std::lock_guard lock(progress->mutex); observed = progress->value; }
            if (observed.value("total", 0.0) > 0) cyui::progressBar(float(observed.value("downloaded", 0.0) / observed.at("total").get<double>()));
        }
        if (busy() && (activeAction == "dependencies.install" || activeAction == "updates.install")) {
            ImGui::SameLine(); if (cyui::button(trLabel("Cancel download").c_str())) cancel();
        }
        if (preferencesDirty && preferencesLoaded && !busy()) {
            preferencesDirty = false;
            start({{"action", "preferences.save"}, {"preferences", {{"language", cyui::locale()}, {"theme", themes[theme]}, {"compact", compact}}}});
        }
        ImGui::End(); cyui::renderNotifications();
    }
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        Manager manager;
        wchar_t binary[32768]{}; GetModuleFileNameW(nullptr, binary, 32768); manager.home = fs::path(binary).parent_path();
        int argc{}; auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::string language = "en";
        for (int i = 1; i < argc; ++i) {
            const std::wstring key = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("Manager option requires a value");
            const auto value = cycode::pathUtf8(fs::path(argv[++i]));
            if (key == L"--home") manager.home = cycode::utf8Path(value);
            else if (key == L"--installation-root") manager.root = cycode::utf8Path(value);
            else if (key == L"--state") manager.state = cycode::utf8Path(value);
            else if (key == L"--cache") manager.cache = cycode::utf8Path(value);
            else if (key == L"--language") { language = value; manager.explicitLanguage = true; }
            else if (key == L"--page") { manager.page = value == "dependencies" ? 1 : value == "updates" ? 2 : 0; manager.explicitPage = true; }
            else throw std::runtime_error("Unknown Manager option");
        }
        LocalFree(argv);
        ImGui_ImplWin32_EnableDpiAwareness();
        dpi = float(GetDpiForSystem()) / 96.f;
        WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, windowProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"CyCodeCoreManager", nullptr};
        const auto icon = manager.home / "icons/CyCode.ico";
        wc.hIcon = static_cast<HICON>(LoadImageW(nullptr, icon.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassExW(&wc);
        RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        HWND window = CreateWindowW(wc.lpszClassName, L"CyCodeCore Manager", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            (std::min)(int(1080 * dpi), int(work.right - work.left)),
            (std::min)(int(790 * dpi), int(work.bottom - work.top)), nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("Manager window creation failed");
        DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferCount = 2; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = window; desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &desc, &swapChain, &device, nullptr, &context);
        if (FAILED(result)) result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &desc, &swapChain, &device, nullptr, &context);
        if (FAILED(result)) throw std::runtime_error("Direct3D 11 initialization failed");
        makeTarget();
        ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        cyui::Config config; config.themeKey = "navy"; config.density = cyui::Density::comfortable; config.dpiScale = dpi;
        cyui::apply(config); cyui::loadFonts(); registerManagerLocale(); cyui::setLocale(language.c_str());
        ImGui_ImplWin32_Init(window); ImGui_ImplDX11_Init(device, context);
        ShowWindow(window, SW_SHOWDEFAULT); UpdateWindow(window);
        manager.start({{"action", "snapshot"}});
        bool done = false; float appliedDpi = dpi;
        while (!done) {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_APP + 1) { manager.closing = true; if (manager.busy()) manager.cancel(); }
                if (message.message == WM_QUIT) done = true;
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (done) break;
            if (width && height) { releaseTarget(); swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0); width = height = 0; makeTarget(); }
            if (appliedDpi != dpi) { cyui::applyDpi(dpi); appliedDpi = dpi; }
            manager.poll();
            if (manager.closing && !manager.busy()) break;
            if (IsIconic(window) || !target) { Sleep(50); continue; }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); manager.draw(window); ImGui::Render();
            const auto color = ImGui::ColorConvertU32ToFloat4(cyui::palette().bg0); const float clear[]{color.x, color.y, color.z, color.w};
            context->OMSetRenderTargets(1, &target, nullptr); context->ClearRenderTargetView(target, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); swapChain->Present(1, 0);
            if (manager.closing && !manager.busy()) done = true;
        }
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); releaseTarget();
        swapChain->Release(); context->Release(); device->Release(); DestroyWindow(window); UnregisterClassW(wc.lpszClassName, instance);
        CoUninitialize(); return 0;
    } catch (const std::exception& exception) {
        MessageBoxW(nullptr, cycode::utf8Path(exception.what()).c_str(), L"CyCodeCore Manager", MB_OK | MB_ICONERROR);
        CoUninitialize(); return 1;
    }
}
