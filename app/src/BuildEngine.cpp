#include "cycode/Engine.h"
#include "cycode/Solution.h"

namespace cycode {
Json Engine::buildOperation(const std::string &method, const Json &params,
                            const std::atomic<bool> *cancellation) {
    checkBuildCancellation(cancellation);
    if (method == "analysis.inspect") return inspectCpp(params, cancellation);
    const auto succeeded = [&](const Json &result) {
        return result.value("exitCode", -1) == 0 && !result.value("cancelled", false) &&
               !result.value("truncated", false) && !result.value("timedOut", false) && !cancellation->load();
    };
    if(method=="build.presetRun") {
        { std::lock_guard lock(mutex_);
          for(const auto &[path,doc]:documents_)
              if(doc.dirty)throw std::runtime_error("Save workspace documents before executing a CMake preset"); }
        auto result=build_.runPreset(params,cancellation);
        if(succeeded(result) && result.at("project").is_object()) {
            const auto &database=result.at("project").at("compilationDatabase");
            if(database.at("available").get<bool>())language_.setCompilationDatabase(resolve(database.at("path")).parent_path(),true);
        }
        monitor_.invalidate();
        return result;
    }
    if (method.starts_with("msbuild.")) {
        auto result = msbuild_.run(method, params, cancellation);
        if (method == "msbuild.prepare" && succeeded(result) &&
            result.at("project").at("compilationDatabase").at("available").get<bool>())
            language_.setCompilationDatabase(
                resolve(result.at("project").at("compilationDatabase").at("path")).parent_path(), true);
        return result;
    }
    if (method == "unreal.prepare")
        return prepareUnrealAnalysis(params, cancellation);
    if (method == "unreal.automationRun") {
        { std::lock_guard lock(mutex_);
          for (const auto &[path,doc] : documents_)
              if (doc.dirty || !doc.diskConflict.empty())
                  throw std::runtime_error("Save or reconcile workspace documents before Unreal cooking/packaging"); }
        auto result=unreal_.automationRun(params,cancellation);
        monitor_.invalidate();
        return result;
    }
    if (method.starts_with("unreal.")) {
        if (method == "unreal.build") {
            std::lock_guard lock(mutex_);
            for (const auto &[path,doc] : documents_)
                if (doc.dirty || !doc.diskConflict.empty()) throw std::runtime_error("Save or reconcile workspace documents before Unreal building");
        }
        auto result = unreal_.run(method, params, cancellation);
        if (result.at("project").value("available",false)) addSdkRoots(result.at("project"));
        if (succeeded(result) && result.at("project").value("available",false) && !result.contains("inspectionError")) {
            if (method == "unreal.generate") {
                auto refreshed = inspectSolution(entry_);
                std::lock_guard lock(mutex_);
                solution_ = std::move(refreshed);
            }
            if (method == "unreal.database") {
                const auto &database = result.at("project").at("compilationDatabase");
                if (database.at("available").get<bool>())
                    language_.setCompilationDatabase(resolve(database.at("path")).parent_path(), true);
            }
        }
        return result;
    }
    auto result = build_.run(method, params, cancellation);
    if (succeeded(result)) {
        const auto &database = result.at("project").at("compilationDatabase");
        language_.setCompilationDatabase(database.at("available").get<bool>()
                                             ? resolve(database.at("path")).parent_path()
                                             : std::filesystem::path{},
                                         method == "build.configure");
    }
    return result;
}
} // namespace cycode
