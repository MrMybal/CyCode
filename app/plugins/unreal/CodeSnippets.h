#pragma once
#include <nlohmann/json.hpp>
// Plain insertion templates. The application owns fields, scopes and Undo;
// these do not query reflection, add includes or perform a semantic refactoring.
inline nlohmann::json unrealCodeSnippets() {
    using Json=nlohmann::json;
    return {{"id","cycode.unreal.snippets"},{"title","Unreal C++ snippets"},
        {"definitions",{
            {"Unreal log",{{"prefix",Json::array({"ue_log","uelog"})},{"scope","cpp"},
                {"description","Insert UE_LOG with a category, verbosity and message"},
                {"body","UE_LOG(${1:LogTemp}, ${2|Log,Display,Warning,Error|}, TEXT(\"${3:message}\"));$0"}}},
            {"Blueprint property",{{"prefix","ue_property"},{"scope","cpp"},
                {"description","Insert an editable Blueprint property declaration"},
                {"body",Json::array({"UPROPERTY(${1:EditAnywhere}, BlueprintReadWrite, Category = \"${2:Default}\")",
                    "${3:float} ${4:Value} = ${5:0.0f};$0"})}}},
            {"Blueprint function",{{"prefix","ue_function"},{"scope","cpp"},
                {"description","Insert a Blueprint-callable function declaration"},
                {"body",Json::array({"UFUNCTION(BlueprintCallable, Category = \"${1:Default}\")",
                    "void ${2:FunctionName}();$0"})}}},
            {"Editor-only selection",{{"prefix","ue_editor"},{"scope","cpp"},
                {"description","Wrap selected code in WITH_EDITOR guards; literal indentation"},
                {"body",Json::array({"#if WITH_EDITOR","$TM_SELECTED_TEXT","#endif$0"})}}}
        }}};
}
