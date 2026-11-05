#pragma once
#include <nlohmann/json.hpp>
inline nlohmann::json unrealProjectTemplate() {
    using Json = nlohmann::json;
    return {
        {"id", "cycode.unreal.cppGame"},
        {"title", "Unreal Engine C++ game"},
        {"language", "cpp"},
        {"buildSystem", "unreal"},
        {"nameStyle", "identifier"},
        {"requirements", {"Unreal Engine installation and its C++ toolchain"}},
        {"fields", Json::array({{{"id", "engineAssociation"},
                                 {"title", "Engine association"},
                                 {"type", "value"},
                                 {"default", "5.3"},
                                 {"help", "Installed engine version or source-build association GUID"}}})},
        {"entry", "${project.name}.uproject"},
        {"openFile", "Source/${project.name}/${project.name}.cpp"},
        {"instructions", "Open the .uproject in CyCode. Select your installed engine root in Unreal Engine, "
                         "generate project files, then build the Editor target. Project creation does not "
                         "install Unreal Engine or build the game."},
        {"files",
         Json::array(
             {{{"path", "${project.name}.uproject"},
               {"text",
                "{\n  \"FileVersion\": 3,\n  \"EngineAssociation\": \"${option.engineAssociation}\",\n  "
                "\"Category\": \"\",\n  \"Description\": \"\",\n  \"Modules\": [{\"Name\": "
                "\"${project.name}\", \"Type\": \"Runtime\", \"LoadingPhase\": \"Default\"}]\n}\n"}},
              {{"path", "Source/${project.name}.Target.cs"},
               {"text",
                "using UnrealBuildTool;\nusing System.Collections.Generic;\n\npublic class "
                "${project.name}Target : TargetRules\n{\n    public ${project.name}Target(TargetInfo Target) "
                ": base(Target)\n    {\n        Type = TargetType.Game;\n        DefaultBuildSettings = "
                "BuildSettingsVersion.V2;\n        ExtraModuleNames.Add(\"${project.name}\");\n    }\n}\n"}},
              {{"path", "Source/${project.name}Editor.Target.cs"},
               {"text", "using UnrealBuildTool;\nusing System.Collections.Generic;\n\npublic class "
                        "${project.name}EditorTarget : TargetRules\n{\n    public "
                        "${project.name}EditorTarget(TargetInfo Target) : base(Target)\n    {\n        Type "
                        "= TargetType.Editor;\n        DefaultBuildSettings = BuildSettingsVersion.V2;\n     "
                        "   ExtraModuleNames.Add(\"${project.name}\");\n    }\n}\n"}},
              {{"path", "Source/${project.name}/${project.name}.Build.cs"},
               {"text",
                "using UnrealBuildTool;\n\npublic class ${project.name} : ModuleRules\n{\n    public "
                "${project.name}(ReadOnlyTargetRules Target) : base(Target)\n    {\n        PCHUsage = "
                "PCHUsageMode.UseExplicitOrSharedPCHs;\n        PublicDependencyModuleNames.AddRange(new "
                "string[] { \"Core\", \"CoreUObject\", \"Engine\", \"InputCore\" });\n    }\n}\n"}},
              {{"path", "Source/${project.name}/${project.name}.h"},
               {"text", "#pragma once\n\n#include \"CoreMinimal.h\"\n"}},
              {{"path", "Source/${project.name}/${project.name}.cpp"},
               {"text",
                "#include \"${project.name}.h\"\n#include "
                "\"Modules/ModuleManager.h\"\n\nIMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, "
                "${project.name}, \"${project.name}\");\n"}},
              {{"path", "Config/DefaultGame.ini"},
               {"text", "[/Script/EngineSettings.GeneralProjectSettings]\nProjectName=${project.name}\n"}},
              {{"path", ".gitignore"},
               {"text", ".cycode/\n.vs/\n.idea/\nBinaries/\nDerivedDataCache/\nIntermediate/\nSaved/\n"}},
              {{"path", "README.md"},
               {"text", "# ${project.name}\n\nUnreal Engine C++ game, associated with "
                        "${option.engineAssociation}.\n\nOpen ${project.name}.uproject in CyCode. Select the "
                        "installed engine root in Unreal Engine, generate project files, then build "
                        "${project.name}Editor.\n\nCreating this project does not install Unreal Engine, "
                        "initialize Git or choose a project license.\n"}}})}};
}
