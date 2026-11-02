#include "cycode/EditorLanguage.h"
#include "cycode/EditorTheme.h"
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace cycode;
using Color=TextEditor::Color;
namespace fs=std::filesystem;
static const std::string fixture=R"code(// Unreal C++ - directives, reflection, types and functions
#pragma once

#include "CoreMinimal.h"
#include <vector> // Standard library
#include "Gameplay/TraceActor.generated.h"

/** A trace component with readable reflection metadata. */
UCLASS(BlueprintType)
class TRACE_API UTraceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Client prediction
    UFUNCTION(BlueprintCallable, Category = "Equipment",
        meta = (DisplayName = "Grip from Equipment Slot"))
    void GripFromHolster(ACharacter* PawnGripping);

    /** Returns true when a pending result is ready. */
    UFUNCTION(BlueprintCallable, Category = "Trace|RefData")
    static bool IsTraceReady(const FTraceResult& Data);

    // Server RPCs
    UFUNCTION(Server, Reliable, Category = "Equipment")
    void Server_ReleaseIntoHolster(APawn* HolsterOwner, uint8 SlotIndex);

    void UpdateTrace(float DeltaTime)
    {
        const int32 MaxResults = 42;
        TArray<FVector> Points;
        UE_LOG(LogTemp, Warning, TEXT("Results: %d"), MaxResults);
        check(IsValid(this));
        Points.Add(FVector(1.0f, 2.5f, 3.0f));
    }
};

#if WITH_EDITOR
#define TRACE_LIMIT 128 // Compile-time setting
#endif // WITH_EDITOR
)code";
static void require(bool condition,const std::string &message) { if (!condition) throw std::runtime_error(message); }
static TextEditor::DocPos position(const std::string &text,size_t offset) {
    TextEditor::DocPos result{0,0};
    for (size_t i=0;i<offset;++i) { const auto c=static_cast<unsigned char>(text[i]); if (c=='\n') { ++result.line; result.index=0; } else if ((c&0xc0)!=0x80) ++result.index; }
    return result;
}
static void expect(TextEditor &editor,const std::string &text,const std::string &token,Color expected) {
    const auto at=text.find(token); require(at!=std::string::npos,"Missing fixture token: "+token);
    const auto actual=editor.GetTokenColor(position(text,at));
    require(actual==expected,"Wrong role for "+token+": "+std::to_string(int(actual))+", expected "+std::to_string(int(expected)));
}
static float luminance(ImU32 color) {
    const auto rgb=ImGui::ColorConvertU32ToFloat4(color);
    auto linear=[](float c) { return c<=.04045f ? c/12.92f : std::pow((c+.055f)/1.055f,2.4f); };
    return .2126f*linear(rgb.x)+.7152f*linear(rgb.y)+.0722f*linear(rgb.z);
}
static float contrast(ImU32 a,ImU32 b) { const float x=luminance(a),y=luminance(b); return (std::max(x,y)+.05f)/(std::min(x,y)+.05f); }
static void acceptance() {
    TextEditor editor; editor.SetLanguage(editorLanguage("TraceActor.h",true)); editor.SetText(fixture);
    for (auto token : {"#pragma","once","#include","#if","#define","#endif"}) expect(editor,fixture,token,Color::preprocessor);
    for (auto token : {"\"CoreMinimal.h\"","<vector>","\"Equipment\"","\"Results: %d\""}) expect(editor,fixture,token,Color::string);
    for (auto token : {"UCLASS","TRACE_API","GENERATED_BODY","UFUNCTION","UE_LOG","TEXT(","check(","TRACE_LIMIT"}) expect(editor,fixture,token,Color::macro);
    for (auto token : {"BlueprintType","BlueprintCallable","Category","DisplayName","Server,","Reliable"}) expect(editor,fixture,token,Color::attribute);
    for (auto token : {"UTraceComponent","UActorComponent","ACharacter","FTraceResult","APawn","uint8","int32","TArray","FVector"}) expect(editor,fixture,token,Color::declaration);
    for (auto token : {"class ","public:","void ","static ","bool ","const ","float "}) expect(editor,fixture,token,Color::keyword);
    for (auto token : {"GripFromHolster","IsTraceReady","Server_ReleaseIntoHolster","UpdateTrace","IsValid","Add("}) expect(editor,fixture,token,Color::function);
    for (auto token : {"PawnGripping","HolsterOwner","SlotIndex","MaxResults","DeltaTime","Points;"}) expect(editor,fixture,token,Color::identifier);
    for (auto token : {"42;","1.0f","128 "}) expect(editor,fixture,token,Color::number);
    for (auto token : {"// Standard","/** A trace","// Compile-time"}) expect(editor,fixture,token,Color::comment);
    std::unordered_set<std::string> names;
    editor.IterateIdentifiers([&](const std::string &name) { names.insert(name); });
    for (auto name : {"GripFromHolster","UFUNCTION","Category","FTraceResult","PawnGripping"}) require(names.contains(name),std::string("Coloring removed completion identifier: ")+name);
    require(editor.GetText()==fixture,"Colorization must preserve source text");
    const std::string multiline="/* UFUNCTION Fake()\n#include <fake>\n*/\nvoid Actual();\nconst char* s = \"UFUNCTION Fake()\";\n// #include <fake>\n";
    editor.SetText(multiline); expect(editor,multiline,"#include <fake>",Color::comment); expect(editor,multiline,"Actual",Color::function); expect(editor,multiline,"\"UFUNCTION",Color::string);
    editor.SelectAll(); editor.ReplaceTextInCurrentCursor("void First();\nvoid Actual();\n"); expect(editor,"void First();\nvoid Actual();\n","Actual",Color::function);
    editor.Undo(); require(editor.GetText()==multiline,"Syntax edits must preserve ordinary undo"); expect(editor,multiline,"#include <fake>",Color::comment);
    const std::string unicode="// Unicode 🛠\nvoid été();\n"; editor.SetText(unicode); expect(editor,unicode,"été",Color::function);
    const std::string cpp="class Widget {};\nint Compute(int value) { return value + 1; }\nauto x = MakeShared<Widget>();\n";
    editor.SetLanguage(editorLanguage("Main.cpp")); editor.SetText(cpp); expect(editor,cpp,"Widget",Color::declaration); expect(editor,cpp,"Compute",Color::function); expect(editor,cpp,"MakeShared",Color::function); expect(editor,cpp,"int ",Color::keyword);
    const std::string cs="class Counter { public int Value; public int Increment(int amount) => amount + 1; }";
    editor.SetLanguage(editorLanguage("Program.cs")); editor.SetText(cs); expect(editor,cs,"Counter",Color::declaration); expect(editor,cs,"Increment",Color::function); expect(editor,cs,"int ",Color::keyword); expect(editor,cs,"Value",Color::identifier);
    require(editorLanguage("config.json")==TextEditor::Language::Json() && editorLanguage("script.lua")==TextEditor::Language::Lua(),"Existing language routing");
    const std::string overlay="FVector FValue;\nusing Alias = FVector;\n// Keep this comment green\n";
    editor.SetLanguage(editorLanguage("Overlay.h",true)); editor.SetText(overlay);
    expect(editor,overlay,"FValue",Color::declaration); expect(editor,overlay,"Alias",Color::identifier);
    const nlohmann::json tokens=nlohmann::json::array({
        {{"line",0},{"glyphStart",8},{"glyphEnd",14},{"type","variable"},{"modifiers",nlohmann::json::array()}},
        {{"line",1},{"glyphStart",6},{"glyphEnd",11},{"type","type"},{"modifiers",nlohmann::json::array()}},
        {{"line",2},{"glyphStart",3},{"glyphEnd",7},{"type","function"},{"modifiers",nlohmann::json::array()}}});
    applySemanticHighlights(editor,tokens,true);
    require(editor.GetDisplayColor({0,8})==Color::identifier && editor.GetDisplayColor({1,6})==Color::declaration,"Semantic variables and aliases override lexical guesses");
    require(editor.GetTokenColor({0,8})==Color::declaration && editor.GetDisplayColor({2,3})==Color::comment,"Display overlays preserve lexical/completion and comment state");
    bool invalid=false; try { editor.SetSemanticTokens({{0,0,999,Color::function}}); } catch (...) { invalid=true; }
    require(invalid && editor.GetDisplayColor({1,6})==Color::declaration,"Malformed overlay rejected atomically");
    editor.SelectAll(); editor.ReplaceTextInCurrentCursor(overlay+"int Next;\n");
    require(editor.GetDisplayColor({0,8})==Color::declaration,"Text edits immediately invalidate semantic colors");
    editor.Undo(); require(editor.GetText()==overlay && editor.GetDisplayColor({0,8})==Color::declaration,"Undo does not resurrect obsolete semantic spans");
    applySemanticHighlights(editor,tokens,true); editor.ClearSemanticTokens();
    require(editor.GetDisplayColor({1,6})==Color::identifier,"Explicit lexical fallback");
    const std::string tabbedBefore="\tFVector FValue;\n", tabbedAfter="\tFVector FOther;\n";
    editor.SetText(tabbedBefore); editor.SetInsertSpacesOnTabs(true);
    editor.SelectAll(); editor.ReplaceTextPreservingTabs(tabbedAfter);
    require(editor.GetText()==tabbedAfter && editor.IsInsertSpacesOnTabs(),"Core replacements preserve raw tabs and keyboard policy");
    editor.Undo(); require(editor.GetText()==tabbedBefore,"Undo preserves original tab positions");
    editor.Redo(); require(editor.GetText()==tabbedAfter,"Redo preserves new tab positions");
    editor.SetInsertSpacesOnTabs(false);
    for (auto theme : {"graphite","mid-grey","light-grey","amber-night","violet","navy"}) {
        applyEditorTheme(theme); const auto palette=editorColours(theme); const auto &p=cyui::palette();
        for (auto role : {Color::text,Color::identifier,Color::keyword,Color::declaration,Color::function,Color::macro,Color::attribute,Color::number,Color::string,Color::preprocessor,Color::comment,Color::inactive,Color::field,Color::parameter,Color::enumerator})
            for (auto ground : {p.bg0,p.bg1}) require(contrast(palette.get(role),ground)>=4.499f,std::string("Syntax contrast: ")+theme+" role "+std::to_string(int(role)));
        require(palette.get(Color::function)!=palette.get(Color::declaration) && palette.get(Color::string)!=palette.get(Color::preprocessor),std::string("Distinct syntax hues: ")+theme);
    }
    std::string large; for (int i=0;i<10000;++i) large+="UFUNCTION(BlueprintCallable) static bool Query(const FTraceResult& Data);\n";
    const auto started=std::chrono::steady_clock::now(); editor.SetLanguage(editorLanguage("Large.h",true)); editor.SetText(large);
    require(editor.GetTokenColor({9999,41})==Color::function,"Large-header actual tokenization");
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    require(ms<5000,"10,000-line highlighting exceeded five seconds");
    std::cout << "Actual editor: C++/Unreal/C# roles, split directives, comments/strings, Unicode, incremental edits, six-theme contrast; 10,000 lines in " << ms << " ms\n";
}
static void render(const fs::path &reports) {
    require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    auto window=SDL_CreateWindow("CyCode syntax validation (hidden)",1440,1040,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    require(window!=nullptr,SDL_GetError()); const auto context=SDL_GL_CreateContext(window); require(context!=nullptr,SDL_GetError());
    SDL_GL_MakeCurrent(window,context); SDL_GL_SetSwapInterval(0);
    ImGui_ImplSDL3_InitForOpenGL(window,context); require(ImGui_ImplOpenGL3_Init("#version 330 core"),"OpenGL backend");
    cyui::Config config; config.themeKey="navy"; cyui::apply(config); cyui::loadFonts();
    ImGui::GetIO().IniFilename=nullptr; ImGui::GetStyle().FontScaleMain=1.15f;
    TextEditor editor; editor.SetLanguage(editorLanguage("TraceActor.h",true)); editor.SetText(fixture);
    editor.SetReadOnlyEnabled(true); editor.SetShowMiniMapEnabled(false); editor.SetLineFoldingEnabled(true);
    fs::create_directories(reports);
    for (auto theme : {"graphite","mid-grey","light-grey","amber-night","violet","navy"}) {
        applyEditorTheme(theme); editor.SetPalette(editorColours(theme));
        for (int frame=0;frame<8;++frame) {
            SDL_Event event; while (SDL_PollEvent(&event)) {} // No external input in render checks.
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
            ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({1440,1040});
            ImGui::Begin("Syntax",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("C++ / Unreal  |  %s",theme);
            { cyui::ScopedFont mono(cyui::monoFont()); editor.Render("##code",{0,0}); }
            ImGui::End(); ImGui::Render();
            const auto ground=cyui::vec(cyui::palette().bg0); glViewport(0,0,1440,1040); glClearColor(ground.x,ground.y,ground.z,1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        auto surface=SDL_CreateSurface(1440,1040,SDL_PIXELFORMAT_RGBA32); require(surface!=nullptr,SDL_GetError());
        glReadPixels(0,0,1440,1040,GL_RGBA,GL_UNSIGNED_BYTE,surface->pixels); auto pixels=static_cast<unsigned char*>(surface->pixels);
        for (int y=0;y<520;++y) std::swap_ranges(pixels+y*surface->pitch,pixels+(y+1)*surface->pitch,pixels+(1039-y)*surface->pitch);
        const auto path=reports/(std::string("syntax-")+theme+".bmp"); require(SDL_SaveBMP(surface,path.string().c_str()),SDL_GetError()); SDL_DestroySurface(surface);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit();
    std::cout << "Six real editor renders saved using one hidden SDL/OpenGL window\n";
}
int main(int argc,char **argv) {
    ImGui::CreateContext();
    try {
        acceptance(); if (argc==3 && std::string(argv[1])=="--render") render(argv[2]);
        ImGui::DestroyContext(); return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; ImGui::DestroyContext(); return 1; }
}
