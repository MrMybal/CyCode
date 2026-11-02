#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include "cycode/TextFile.h"
#include <imgui_internal.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>

using namespace cycode;
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static void frame(const std::function<void()> &draw, ImGuiKey key = ImGuiKey_None) {
    auto &io = ImGui::GetIO();
    if (key != ImGuiKey_None) io.AddKeyEvent(key, true);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
    draw(); ImGui::Render();
    if (!ImGui::GetDrawData()->TotalVtxCount)
        throw std::runtime_error("No migrated view draw data at frame " + std::to_string(ImGui::GetFrameCount()));
    if (key != ImGuiKey_None) io.AddKeyEvent(key, false);
}
namespace cycode {
struct ApplicationModuleTests {
    static void run(const std::filesystem::path &root) {
        const auto folder = root / "runtime/module-fixture";
        std::filesystem::create_directories(folder / "workspace");
        std::filesystem::create_directories(folder / "app/locales");
        std::filesystem::copy_file(root / "app/locales/ui.json", folder / "app/locales/ui.json", std::filesystem::copy_options::overwrite_existing);
        std::ofstream(folder / "workspace/sample.cpp") << "int main() { return 1; }\n";
        std::ofstream(folder / "workspace/shader.hlsl") << "float4 main() : SV_Target { return 1; }\n";
        std::ofstream(folder / "workspace/events.log") << "Log: Warning: fixture\n";
        LaunchOptions options; options.installation = folder; options.workspace = folder / "workspace";
        options.smoke = true; options.testSession = true;
        Application app(options, nullptr); // No SDL initialization or operating-system window.
        unsigned char *pixels=nullptr;int width=0,height=0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        ImGui::GetIO().Fonts->SetTexID(ImTextureID(1));
        app.open("sample.cpp");
        auto tab = app.tabs_.back();
        const auto before = tab->editor.GetText();
        tab->editor.SetCursor({0, 0}); tab->editor.ReplaceTextPreservingTabs("// edited\n"); app.changed(tab);
        require(app.engine_->dispatch("documents.read", {{"path", tab->path}}).at("text") == tab->editor.GetText(), "Editor transaction did not reach Engine");
        app.executeCommand("edit.undo");
        require(tab->editor.GetText() == before, "Native undo lost across CodeEditor migration");
        app.executeCommand("edit.redo"); require(tab->editor.GetText() != before, "Native redo lost");
        // Exercise the actual widget -> Engine path, including the native
        // transaction callback already owned by EditorSnippet.
        frame([&] { app.editorPanel(); });frame([&] { app.editorPanel(); });
        const auto revision=tab->revision;
        ImGui::GetIO().AddInputCharactersUTF8("x");frame([&] { app.editorPanel(); });
        require(tab->revision>revision && app.engine_->dispatch("documents.read",{{"path",tab->path}}).at("text")==tab->editor.GetText(),
                "Typing in shared CodeEditor did not synchronize the Engine");
        app.executeCommand("edit.undo");
        app.open("shader.hlsl"); require(app.tabs_.back()->codeEditor.language() == cyui::CodeLanguage::hlsl, "HLSL catalogue not used");
        app.open("events.log"); require(app.tabs_.back()->codeEditor.language() == cyui::CodeLanguage::log, "Log syntax not used");
        app.options_ = true;
        app.panels_["problems"] = true; app.panels_["information"] = true; app.panels_["refactoring"] = true;
        app.buildDiagnostics_ = Json::array({{{"path","sample.cpp"},{"line",1},{"column",2},{"severity","warning"},{"message","test diagnostic"}}});
        app.informationState_ = {{"available",true},{"contents",Json::array({{{"kind","markdown"},{"text","# API\n**Description** [link](https://example.invalid)"}},{{"kind","code"},{"language","hlsl"},{"text","float4 value;\n// ``` literal"}},{{"kind","plaintext"},{"text","<literal> [plain](text)"}}})}};
        app.refactorPlan_ = {{"id","fixture"},{"state","preview"},{"title","Fixture"},{"files",Json::array({{{"path","sample.cpp"}}})},{"editCount",1}};
        app.refactorFile_ = "sample.cpp"; app.refactorPreview_ = {{"before",before},{"after","int main() { return 2; }\n"}};
        app.refactorDiff_.setTexts(before, app.refactorPreview_.at("after"), cyui::CodeLanguage::cpp);
        for (auto *view : {&app.beforePreviewCode_, &app.afterPreviewCode_}) { configureCodeEditor(*view); view->setReadOnly(true); view->native().SetWordWrapEnabled(true); }
        app.beforePreviewCode_.setText(before); app.afterPreviewCode_.setText(app.refactorPreview_.at("after"));
        app.git_={{"repository",true},{"branch","fixture"},{"files",Json::array()}};
        app.vcsPreviewTitle_="vcs.diff";app.vcsIsDiff_=true;
        app.vcsDiff_.setUnified("--- a/shader.hlsl\n+++ b/shader.hlsl\n@@ -1 +1 @@\n-float oldValue;\n+float newValue;\n",cyui::CodeLanguage::hlsl);
        app.vcsConflict_={{"path","shader.hlsl"},{"revision",1},{"versions",{{"base","float base;\n"},{"ours","float ours;\n"},{"theirs","float theirs;\n"}}}};
        for (auto *view : {&app.vcsBaseCode_,&app.vcsOursCode_,&app.vcsTheirsCode_}) {
            configureCodeEditor(*view);view->setLanguage(cyui::CodeLanguage::hlsl);view->setReadOnly(true);view->setText("float version;\n");
        }
        // Draw each actual app view independently so docking cannot hide it.
        for (const auto *theme : {"graphite","mid-grey","light-grey","amber-night","violet","navy"})
            for (auto density : {cyui::Density::compact, cyui::Density::comfortable})
                for (float dpi : {1.f,1.5f,2.f}) {
                    cyui::Config config; config.themeKey=theme;config.density=density;config.dpiScale=dpi;cyui::apply(config); applyEditorTheme(theme); app.theme_=theme;
                    for (const auto *locale : {"en","fr"}) {
                        app.locale_=locale;cyui::setLocale(locale);
                        if (app.locale_=="fr") require(std::string(app.tr("Language"))=="Langue", "App French catalogue lost");
                        frame([&] { app.optionsPanel(); });
                        frame([&] { app.problemsPanel(); });
                        frame([&] { app.informationPanel(); });
                        app.previewWrap_=false;frame([&] { app.refactorPanel(); });
                        app.previewWrap_=true;frame([&] { app.refactorPanel(); });
                        frame([&] { app.outputPanel(); });
                        app.vcsSection_="Review";frame([&] { app.gitPanel(); });frame([&] { app.gitPanel(); });
                        app.vcsSection_="Conflicts";frame([&] { app.gitPanel(); });frame([&] { app.gitPanel(); });
                    }
                }
        require(app.informationDocuments_.at("1").code.text()=="float4 value;\n// ``` literal", "Literal code documentation was changed by Markdown parsing");
        // Actual Inspector gestures: draft -> Escape, then draft -> click-away commit.
        app.optionsFilter_.query="clangd";const auto originalClangd=app.clangd_;ImRect field;
        auto property=[&] { ImGui::Begin("Settings gesture");app.optionsProperties();field=ImGui::GetCurrentContext()->LastItemData.Rect;ImGui::End(); };
        frame(property);frame(property);
        auto &propertyIO=ImGui::GetIO();
        propertyIO.AddMousePosEvent(field.Max.x-30,field.Min.y+8);propertyIO.AddMouseButtonEvent(0,true);frame(property);
        propertyIO.AddMouseButtonEvent(0,false);frame(property);propertyIO.AddInputCharactersUTF8("draft");frame(property);
        require(app.clangd_==originalClangd,"Property draft changed app settings before commit");
        frame(property,ImGuiKey_Escape);frame(property);require(app.clangd_==originalClangd,"Escape committed app settings");
        propertyIO.AddMousePosEvent(field.Max.x-30,field.Min.y+8);propertyIO.AddMouseButtonEvent(0,true);frame(property);
        propertyIO.AddMouseButtonEvent(0,false);frame(property);propertyIO.AddInputCharactersUTF8("commit");frame(property);
        propertyIO.AddMousePosEvent(20,900);propertyIO.AddMouseButtonEvent(0,true);frame(property);
        propertyIO.AddMouseButtonEvent(0,false);frame(property);
        require(app.clangd_!=originalClangd,"Completed property gesture was not committed");app.clangd_=originalClangd;
        const auto id=uiId("options/clangd");
        app.optionsInspector_.drafts[id]={std::string("draft"),std::string("clangd"),true};
        app.optionsFilter_.query="Theme";
        frame([&] { app.optionsPanel(); });
        require(!app.optionsInspector_.drafts.at(id).active && app.clangd_!="draft", "Filtered property draft escaped into settings");
        app.paletteMode_="actions";app.paletteOpen_=true;
        auto palette=[&] { ImGui::Begin("Palette fixture");app.palette();ImGui::End(); };
        frame(palette);frame(palette);
        ImGui::GetIO().AddInputCharactersUTF8("file.saveAll");frame(palette);
        frame(palette,ImGuiKey_Enter);
        require(app.lastShortcut_=="file.saveAll" && !app.actionPaletteOpen_, "Shared palette did not dispatch the app command");
        frame(palette);
        for (bool mac : {false,true}) {
            auto &io=ImGui::GetIO();io.ConfigMacOSXBehaviors=mac;
            app.panels_["project"]=true; app.filesFilter_.clear();app.editorFocused_=false;
            auto project=[&] { app.shortcuts();app.projectPanel(); };
            frame(project);frame(project);
            ImGui::SetWindowFocus(app.label("Project","project").c_str());
            io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl,true);
            frame(project,ImGuiKey_F);frame(project);
            io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl,false);frame(project);
            io.AddInputCharactersUTF8("sample");frame(project);
            if (app.filesFilter_.query!="sample") throw std::runtime_error(
                std::string("Ctrl/Cmd+F filter focus: mac=") + std::to_string(mac) + " query=" + app.filesFilter_.query +
                " command=" + app.lastShortcut_ + " input=" + std::to_string(ImGui::GetActiveID()) +
                " ctrl=" + std::to_string(io.KeyCtrl));
            frame(project);frame(project,ImGuiKey_Escape);frame(project);
            require(!app.filesFilter_.active(), "Escape did not clear the panel filter");
        }
        ImGui::GetIO().ConfigMacOSXBehaviors=false;
        // A shared renderer must never save workspace files implicitly.
        require(normalizeEditorText(readTextFileBytes(folder / "workspace/sample.cpp")) == tab->editor.GetText(), "Explicit Save all did not persist native buffers");
    }
};
}
int main(int argc, char **argv) {
    try {
        require(argc==2,"Expected repository path");
        ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1600,1000};io.DeltaTime=1.f/60;
        io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});cyui::loadFonts();
        ProblemModel model; model.add("first",cyui::LogLevel::error,"src/é##file.cpp","Error with emoji 😀",{"src/é##file.cpp",4,8,true});
        const auto id=model.rows.front().id;model.add("second",cyui::LogLevel::warning,"other","Warning",{});
        cyui::TextFilter filter; filter.query="e##file";
        const auto matching=model.matching(filter);
        require(model.locations.at(id).utf16 && matching.size()==1 && matching.front().id==id,"Stable filtered diagnostic navigation");
        OutputLog log;std::string output="Error: failed\nWarning: retry\nnormal\n";log.synchronize(output);
        std::vector<cyui::LogEntry> rows;log.buffer.snapshot(rows,0);
        require(rows.size()==3 && rows[0].level==cyui::LogLevel::error && rows[1].level==cyui::LogLevel::warning && rows[2].level==cyui::LogLevel::info,"Shared log severity mapping");
        const auto first=rows[0].id;output+="Debug: trace\n";log.synchronize(output);log.buffer.snapshot(rows,0);require(rows[0].id==first,"Appending output changed existing identities");
        ApplicationModuleTests::run(utf8Path(argv[1]));
        ImGui::DestroyContext();std::cout<<"CyCode module integration passed (no native windows)\n";return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
