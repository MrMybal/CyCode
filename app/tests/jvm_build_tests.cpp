#include "cycode/JvmBuild.h"
#include "cycode/ProjectTasks.h"
#include "cycode/TextEdits.h"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
using Json = nlohmann::json;
namespace {
void require(bool v, const char *m) {
    if (!v)
        throw std::runtime_error(m);
}
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid JVM build contract was accepted");
}
} // namespace
void jvmBuildContracts(const std::filesystem::path &fixture) {
    using namespace cycode;
    namespace fs = std::filesystem;
    const auto root = fixture / utf8Path("jvm-model & été");
    fs::create_directories(root / "classes");
    const auto base = fs::canonical(root);
    const auto resolve = [&](const std::string &path) {
        const auto full = fs::weakly_canonical(base / utf8Path(path));
        if (!containsPath(base, full))
            throw std::runtime_error("Outside fixture");
        return full;
    };
    auto escaped = pathUtf8(base / "classes");
    size_t at = 0;
    while ((at = escaped.find('&', at)) != std::string::npos) {
        escaped.replace(at, 1, "&amp;");
        at += 5;
    }
    const auto model = mavenRuntimeModel(
        "<project "
        "xmlns=\"http://maven.apache.org/POM/4.0.0\"><artifactId>demo</artifactId><build><outputDirectory>" +
            escaped + "</outputDirectory></build></project>",
        base, resolve);
    require(model.at("outputs")[0] == pathUtf8(base / "classes"),
            "Maven effective output lost XML entities/Unicode");
    rejects([&] { mavenRuntimeModel("<projects><project/></projects>", base, resolve); });
    rejects([&] {
        mavenRuntimeModel("<project><artifactId>x</artifactId><build><outputDirectory>${unresolved}</"
                          "outputDirectory></build></project>",
                          base, resolve);
    });
    const Json project = {
        {"name", "app"},
        {"module", ":app"},
        {"directory", pathUtf8(base)},
        {"buildFile", pathUtf8(base / "build.gradle.kts")},
        {"outputs", Json::array({pathUtf8(base / "classes")})},
        {"classpath", Json::array({pathUtf8(base / "classes"), pathUtf8(fixture / "dependency.jar")})}};
    const Json gradle = {{"schemaVersion", 1}, {"projects", Json::array({project})}};
    const auto modules = gradleRuntimeModels(gradle.dump(), resolve);
    require(modules.size() == 1 && modules[0].at("module") == ":app" &&
                modules[0].at("classpath").size() == 2,
            "Gradle runtime model lost modules/dependencies");
    auto bad = gradle;
    bad["projects"].push_back(project);
    rejects([&] { gradleRuntimeModels(bad.dump(), resolve); });
    bad = gradle;
    bad["projects"][0]["outputs"][0] = pathUtf8(fixture / "outside");
    rejects([&] { gradleRuntimeModels(bad.dump(), resolve); });
    bad = gradle;
    bad["projects"][0]["classpath"][0] = "relative.jar";
    rejects([&] { gradleRuntimeModels(bad.dump(), resolve); });
    bad = gradle;
    bad["schemaVersion"] = 2;
    rejects([&] { gradleRuntimeModels(bad.dump(), resolve); });
    require(jvmRuntimeClasspath(project.at("classpath")).find(pathUtf8(base / "classes")) !=
                std::string::npos,
            "JVM classpath lost actual class outputs");
    rejects([&] { jvmRuntimeClasspath(Json::array()); });
    rejects([&] { jvmRuntimeClasspath(Json::array({"relative.jar"})); });
    rejects([&] { jvmRuntimeClasspath(Json::array({pathUtf8(base / "*.jar")})); });
    const auto arguments =
        jvmArgumentFile({"--disable-@files", "-cp", pathUtf8(base / "classes"), "demo.Main", "", "a & b",
                         "line\nnext", "\"quoted\"", "@literal"});
    require(arguments.find("\"\"\n") != std::string::npos &&
                arguments.find("line\\nnext") != std::string::npos &&
                arguments.find("\\\"quoted\\\"") != std::string::npos &&
                arguments.find("\"@literal\"") != std::string::npos,
            "JVM argument file changed empty/control/quoted/at-sign arguments");
    rejects([&] { jvmArgumentFile({std::string("x\0y", 3)}); });
    fs::create_directories(base / "maven/.mvn/wrapper");
    std::ofstream(base / "maven/pom.xml") << "<project/>";
    std::ofstream(base / "maven/.mvn/wrapper/maven-wrapper.jar") << "not executed";
    require(ProjectTasks::detect(base / "maven") == "maven", "Maven detection fell back to standalone javac");
    if (!std::getenv("CYCODE_MAVEN_HOME")) {
        const auto launch = jvmBuildLauncher(base, base / "maven", "maven", "/never-run/java", resolve);
        require(launch[0] == "/never-run/java" &&
                    launch.back() == "org.apache.maven.wrapper.MavenWrapperMain",
                "Maven wrapper was expanded through a shell");
        std::ofstream(base / "maven/.mvn/wrapper/maven-wrapper.properties") << "wrapperSha256Sum=required\n";
        rejects([&] { jvmBuildLauncher(base, base / "maven", "maven", "/never-run/java", resolve); });
    }
    fs::create_directories(base / "gradle/gradle/wrapper");
    std::ofstream(base / "gradle/settings.gradle.kts") << "rootProject.name=\"fixture\"";
    std::ofstream(base / "gradle/gradle/wrapper/gradle-wrapper.jar") << "not executed";
    require(ProjectTasks::detect(base / "gradle") == "gradle", "Gradle settings-only root was not detected");
    if (!std::getenv("CYCODE_GRADLE_HOME")) {
        const auto launch = jvmBuildLauncher(base, base / "gradle", "gradle", "/never-run/java", resolve);
        require(launch.size() == 4 && launch.back() == "org.gradle.wrapper.GradleWrapperMain",
                "Gradle wrapper was expanded through a shell");
    }
    require(gradleModelScript().find("main.runtimeClasspath.files") != std::string::npos,
            "Gradle adapter stopped using the actual runtime model");
}
