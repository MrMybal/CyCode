#include "cycode/EditorLanguage.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <string_view>
#include <unordered_set>
namespace cycode {
namespace {
using Iterator = TextEditor::Iterator;
using Color = TextEditor::Color;
using Words = std::unordered_set<std::string>;
bool space(ImWchar c) { return c == ' ' || c == '\t' || c == '\r'; }
bool letter(ImWchar c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool identifier(ImWchar c) { return letter(c) || (c >= '0' && c <= '9'); }
Iterator skipSpace(Iterator at, Iterator end) { size_t n=0; while (at < end && space(*at) && n++<256) ++at; return at; }
std::string word(Iterator at,Iterator end) {
    std::string result; while (at<end && identifier(*at) && result.size()<256) { result += char(*at); ++at; } return result;
}
std::string previousWord(Iterator begin, Iterator at) {
    size_t scanned=0;
    while (at>begin && scanned++<256) { auto previous=at; --previous; if (!space(*previous)) break; at=previous; }
    auto last=at;
    while (at>begin && scanned++<256) { auto previous=at; --previous; if (!identifier(*previous)) break; at=previous; }
    return word(at,last);
}
struct Directive { std::string name; Iterator arguments; bool valid=false; };
Directive directive(Iterator begin,Iterator end) {
    auto at=skipSpace(begin,end); if (at==end || *at!='#') return {};
    at=skipSpace(++at,end); auto name=word(at,end);
    for (size_t i=0;i<name.size();++i) ++at;
    return {name,skipSpace(at,end),true};
}
bool allCaps(const std::string &name) {
    if (name.size()<2) return false; bool alpha=false;
    for (const auto c : name) { if (c>='a' && c<='z') return false; alpha |= c>='A' && c<='Z'; }
    return alpha;
}
bool unrealType(const std::string &name) {
    return name.size()>1 && std::string_view("AUFETI").find(name[0]) != std::string_view::npos && name[1]>='A' && name[1]<='Z';
}
struct Rules { Words keywords, types, macros, attributes; bool unreal=false, managed=false; };
TextEditor::Language makeLanguage(const TextEditor::Language &base, bool unreal=false, bool managed=false) {
    auto result=base; auto rules=std::make_shared<Rules>(); rules->unreal=unreal; rules->managed=managed;
    rules->keywords=base.keywords; rules->keywords.insert(base.declarations.begin(),base.declarations.end());
    for (auto name : {"consteval","constinit","co_await","co_return","co_yield"}) rules->keywords.insert(name);
    for (auto name : {"size_t","ptrdiff_t","int8_t","uint8_t","int16_t","uint16_t","int32_t","uint32_t","int64_t","uint64_t"}) rules->types.insert(name);
    if (unreal) {
        result.name="C++ / Unreal";
        for (auto name : {"UCLASS","USTRUCT","UENUM","UINTERFACE","UFUNCTION","UPROPERTY","GENERATED_BODY","GENERATED_UCLASS_BODY","UE_LOG","TEXT","check","checkf","ensure","ensureMsgf","DECLARE_LOG_CATEGORY_EXTERN","DEFINE_LOG_CATEGORY","DECLARE_DYNAMIC_MULTICAST_DELEGATE","IMPLEMENT_MODULE","IMPLEMENT_PRIMARY_GAME_MODULE"}) rules->macros.insert(name);
        for (auto name : {"UObject","AActor","APawn","ACharacter","UActorComponent","USceneComponent","FVector","FRotator","FTransform","FString","FName","FText","TArray","TMap","TSet","TSubclassOf","TObjectPtr","TWeakObjectPtr","int8","uint8","int16","uint16","int32","uint32","int64","uint64","TCHAR","SIZE_T"}) rules->types.insert(name);
        for (auto name : {"BlueprintCallable","BlueprintPure","BlueprintReadWrite","BlueprintReadOnly","Blueprintable","BlueprintType","BlueprintAuthorityOnly","BlueprintCosmetic","BlueprintImplementableEvent","BlueprintNativeEvent","Category","meta","DisplayName","WorldContext","DefaultToSelf","EditAnywhere","EditDefaultsOnly","VisibleAnywhere","Transient","Replicated","ReplicatedUsing","Server","Client","Reliable","Unreliable","NetMulticast","WithValidation","CallInEditor","Exec","Abstract","MinimalAPI","Config","NotBlueprintable","AllowPrivateAccess","ClampMin","ClampMax","UIMin","UIMax"}) rules->attributes.insert(name);
    }
    result.identifiers.insert(rules->macros.begin(),rules->macros.end());
    result.declarations.insert(rules->types.begin(),rules->types.end());
    // Let the normal string/comment state machine parse the rest of a directive.
    result.preprocess=0;
    result.contextTokenizer=[rules,getIdentifier=base.getIdentifier](Iterator begin,Iterator at,Iterator end,Color &color) {
        const auto preprocessor=directive(begin,end);
        if (*at=='#' && preprocessor.valid && at==skipSpace(begin,end)) {
            auto finish=skipSpace(++at,end); const auto name=word(finish,end);
            for (size_t i=0;i<name.size();++i) ++finish;
            color=Color::preprocessor; return finish;
        }
        if (*at=='<' && preprocessor.valid && at==preprocessor.arguments &&
            (preprocessor.name=="include" || preprocessor.name=="include_next" || preprocessor.name=="import")) {
            auto finish=at; ++finish;
            while (finish<end && *finish!='>') ++finish;
            if (finish<end) ++finish;
            color=Color::string; return finish;
        }
        if (!getIdentifier) return at;
        auto finish=getIdentifier(at,end); if (finish==at) return at;
        color=Color::identifier;
        if (finish-at>256) return finish;
        std::string name; bool ascii=true;
        for (auto i=at;i<finish;++i) { if (*i>127) { ascii=false; break; } name+=char(*i); }
        if (!ascii) { if (auto next=skipSpace(finish,end); next<end && *next=='(') color=Color::function; return finish; }
        if (preprocessor.valid && ((preprocessor.name=="pragma" && name=="once") || name=="defined")) color=Color::preprocessor;
        else if (rules->keywords.contains(name)) color=Color::keyword;
        else if ((preprocessor.valid && preprocessor.name=="define" && at==preprocessor.arguments) || rules->macros.contains(name) || (!rules->managed && allCaps(name))) color=Color::macro;
        else if (rules->types.contains(name) || (rules->unreal && unrealType(name))) color=Color::declaration;
        else if (rules->unreal && rules->attributes.contains(name)) color=Color::attribute;
        else {
            auto next=skipSpace(finish,end);
            const auto previous=previousWord(begin,at);
            if (previous=="class" || previous=="struct" || previous=="enum" || previous=="interface" || previous=="namespace" || previous=="new") color=Color::declaration;
            else if (next<end && *next=='(') color=Color::function;
            else if (next<end && *next=='<') {
                // A template call is a function; a template type stays neutral/typed.
                size_t depth=0, scanned=0;
                for (;next<end && scanned++<512;++next) {
                    if (*next=='<') ++depth;
                    else if (*next=='>' && --depth==0) { ++next; break; }
                }
                next=skipSpace(next,end); if (depth==0 && next<end && *next=='(') color=Color::function;
            }
        }
        return finish;
    };
    return result;
}
}
const TextEditor::Language *editorLanguage(const std::string &path,bool unreal) {
    const auto dot=path.find_last_of('.'); auto ext=dot==std::string::npos ? "" : path.substr(dot);
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c) { return char(std::tolower(c)); });
    static const auto c=makeLanguage(*TextEditor::Language::C());
    static const auto cpp=makeLanguage(*TextEditor::Language::Cpp());
    static const auto ue=makeLanguage(*TextEditor::Language::Cpp(),true);
    static const auto cs=makeLanguage(*TextEditor::Language::Cs(),false,true);
    if (ext==".c") return unreal ? &ue : &c;
    if (ext==".cpp" || ext==".cc" || ext==".cxx" || ext==".h" || ext==".hpp" || ext==".hh" || ext==".hxx" || ext==".inl") return unreal ? &ue : &cpp;
    if (ext==".cs") return &cs;
    if (ext==".glsl" || ext==".vert" || ext==".frag" || ext==".comp") return TextEditor::Language::Glsl();
    if (ext==".hlsl" || ext==".hlsli") return TextEditor::Language::Hlsl();
    if (ext==".json") return TextEditor::Language::Json();
    if (ext==".py") return TextEditor::Language::Python();
    if (ext==".md") return TextEditor::Language::Markdown();
    if (ext==".lua") return TextEditor::Language::Lua();
    return nullptr;
}
}
