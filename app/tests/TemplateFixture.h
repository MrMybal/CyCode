#pragma once
#include <nlohmann/json.hpp>
inline nlohmann::json fixtureProjectTemplate() {
    using Json = nlohmann::json;
    return {{"id", "fixture.native.cmake"},
            {"title", "Plugin CMake console"},
            {"language", "cpp"},
            {"buildSystem", "cmake"},
            {"requirements", {"CMake and C++ compiler"}},
            {"entry", "."},
            {"openFile", "src/main.cpp"},
            {"fields",
             Json::array(
                 {{{"id", "standard"},
                   {"title", "C++ standard"},
                   {"type", "choice"},
                   {"choices", {"17", "20"}},
                   {"default", "20"}},
                  {{"id", "feature"}, {"title", "Enable feature"}, {"type", "boolean"}, {"default", false}},
                  {{"id", "scope"}, {"title", "Namespace"}, {"type", "identifier"}, {"default", "demo"}}})},
            {"instructions", "Configure and build ${project.name} through CMake."},
            {"files",
             Json::array(
                 {{{"path", "CMakeLists.txt"},
                   {"text", "cmake_minimum_required(VERSION 3.24)\nproject(${project.identifier} LANGUAGES "
                            "CXX)\nadd_executable(${project.identifier} src/main.cpp)\nset_property(TARGET "
                            "${project.identifier} PROPERTY CXX_STANDARD ${option.standard})\n# Preserve "
                            "CMake variables: ${CMAKE_CURRENT_SOURCE_DIR}\n"}},
                  {{"path", "src/main.cpp"},
                   {"text", "namespace ${option.scope} { constexpr bool feature = ${option.feature}; }\nint "
                            "main() { return ${option.scope}::feature ? 0 : 0; }\n"}},
                  {{"path", "${project.slug}.ini"},
                   {"text", "symbol=${project.identifier}\napi=${project.upper}\n"}}})}};
}
