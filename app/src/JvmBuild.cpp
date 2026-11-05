#include "cycode/JvmBuild.h"
#include "cycode/Paths.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>
#include <tinyxml2.h>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
fs::path environmentPath(const char *key) {
#ifdef _WIN32
    const auto name = utf8Path(key).wstring();
    const auto length = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (!length)
        return {};
    std::wstring value(length, 0);
    GetEnvironmentVariableW(name.c_str(), value.data(), length);
    value.resize(length - 1);
    return value;
#else
    const auto value = std::getenv(key);
    return value ? fs::path(value) : fs::path{};
#endif
}
fs::path installed(const std::string &system) {
    for (const auto *key : system == "maven"
                               ? std::vector<const char *>{"CYCODE_MAVEN_HOME", "MAVEN_HOME", "M2_HOME"}
                               : std::vector<const char *>{"CYCODE_GRADLE_HOME", "GRADLE_HOME"}) {
        const auto path = environmentPath(key);
        if (path.is_absolute() && fs::is_directory(path))
            return path;
    }
    const auto tool = toolPath(system == "maven" ? "mvn" : "gradle");
    if (!tool.empty())
        return fs::weakly_canonical(utf8Path(tool)).parent_path().parent_path();
    return {};
}
fs::path oneJar(const fs::path &directory, const std::string &prefix) {
    std::vector<fs::path> matches;
    std::error_code error;
    for (const auto &file : fs::directory_iterator(directory, error))
        if (file.is_regular_file() && pathUtf8(file.path().filename()).starts_with(prefix) &&
            file.path().extension() == ".jar")
            matches.push_back(file.path());
    if (matches.size() != 1)
        throw std::runtime_error("Select an installed Java build-tool distribution with exactly one " +
                                 prefix + " launcher jar");
    return matches[0];
}
std::string string(const Json &item, const char *key, size_t max = 32768) {
    const auto value = item.at(key).get<std::string>();
    if (value.empty() || value.size() > max || value.find('\0') != std::string::npos ||
        value.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error(std::string("Invalid JVM model ") + key);
    return value;
}
const tinyxml2::XMLElement *element(const tinyxml2::XMLElement *parent, const char *name) {
    return parent ? parent->FirstChildElement(name) : nullptr;
}
std::string xmlText(const tinyxml2::XMLElement *parent, const char *key) {
    const auto child = element(parent, key);
    const auto value = child && child->GetText() ? std::string(child->GetText()) : std::string{};
    if (value.size() > 32768 || value.find("${") != std::string::npos)
        throw std::runtime_error("Maven returned an unresolved or excessive model field");
    return value;
}
} // namespace
std::vector<std::string> jvmBuildLauncher(const fs::path &workspace, const fs::path &directory,
                                          const std::string &system, const std::string &java,
                                          const JvmResolver &resolve) {
    if (java.empty())
        throw std::runtime_error("An installed Java runtime is required for Maven/Gradle");
    if (system != "maven" && system != "gradle")
        throw std::runtime_error("Unsupported JVM build tool");
    const auto overrideHome = environmentPath(system == "maven" ? "CYCODE_MAVEN_HOME" : "CYCODE_GRADLE_HOME");
    for (auto parent = directory; overrideHome.empty(); parent = parent.parent_path()) {
        const auto path = parent / (system == "maven" ? ".mvn/wrapper/maven-wrapper.jar"
                                                      : "gradle/wrapper/gradle-wrapper.jar");
        if (fs::is_regular_file(path)) {
            const auto jar = resolve(pathUtf8(path));
            if (system == "maven") {
                const auto properties = resolve(pathUtf8(parent / ".mvn/wrapper/maven-wrapper.properties"));
                if (fs::is_regular_file(properties) &&
                    readTextFileBytes(properties).find("wrapperSha256Sum") != std::string::npos)
                    throw std::runtime_error(
                        "Maven wrapper requests shell-side jar checksum validation; use its official wrapper "
                        "externally or explicitly set CYCODE_MAVEN_HOME");
                return {java, "-Dmaven.multiModuleProjectDirectory=" + pathUtf8(parent), "-cp", pathUtf8(jar),
                        "org.apache.maven.wrapper.MavenWrapperMain"};
            }
            return {java, "-cp", pathUtf8(jar), "org.gradle.wrapper.GradleWrapperMain"};
        }
        if (parent == workspace || parent.parent_path() == parent ||
            !containsPath(workspace, parent.parent_path()))
            break;
    }
    const auto home = installed(system);
    if (home.empty())
        throw std::runtime_error(
            "Install " + system + " and set " +
            (system == "maven" ? std::string("CYCODE_MAVEN_HOME") : std::string("CYCODE_GRADLE_HOME")) +
            ", or supply the project's wrapper jar");
    if (system == "gradle") {
        std::error_code error;
        bool modern = false;
        for (const auto &file : fs::directory_iterator(home / "lib", error))
            if (pathUtf8(file.path().filename()).starts_with("gradle-gradle-cli-main-") &&
                file.path().extension() == ".jar")
                modern = true;
        return {java, "-cp",
                pathUtf8(oneJar(home / "lib", modern ? "gradle-gradle-cli-main-" : "gradle-launcher-")),
                "org.gradle.launcher.GradleMain"};
    }
    if (!fs::is_regular_file(home / "bin/m2.conf"))
        throw std::runtime_error("Maven installation has no bin/m2.conf");
    return {java,
            "-Dclassworlds.conf=" + pathUtf8(home / "bin/m2.conf"),
            "-Dmaven.home=" + pathUtf8(home),
            "-Dmaven.multiModuleProjectDirectory=" + pathUtf8(directory),
            "-cp",
            pathUtf8(oneJar(home / "boot", "plexus-classworlds-")),
            "org.codehaus.plexus.classworlds.launcher.Launcher"};
}
std::string gradleModelScript() {
    return R"CYCODE(import groovy.json.JsonOutput
gradle.projectsEvaluated {
    def root = gradle.rootProject
    def taskName = System.getProperty('cycode.modelTask')
    def output = System.getProperty('cycode.modelOutput')
    if (!taskName || !output || root.tasks.findByName(taskName)) throw new GradleException('Invalid or conflicting CyCode model task')
    root.tasks.register(taskName) {
        doLast {
            def models = []
            root.allprojects.each { p ->
                def sets = p.extensions.findByName('sourceSets')
                def main = sets == null ? null : sets.findByName('main')
                if (main != null) {
                    models.add([name:p.name, module:p.path, directory:p.projectDir.absolutePath,
                        buildFile:p.buildFile.absolutePath,
                        outputs:main.output.classesDirs.files.collect { it.absolutePath },
                        classpath:main.runtimeClasspath.files.collect { it.absolutePath }])
                }
            }
            def file = new File(output)
            file.parentFile.mkdirs()
            file.setText(JsonOutput.toJson([schemaVersion:1,projects:models]), 'UTF-8')
        }
    }
}
)CYCODE";
}
nlohmann::json mavenRuntimeModel(const std::string &xml, const fs::path &directory,
                                 const JvmResolver &resolve) {
    if (xml.empty() || xml.size() > 4 * 1024 * 1024 || xml.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid Maven effective model size");
    tinyxml2::XMLDocument document(true, tinyxml2::PRESERVE_WHITESPACE);
    if (document.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error("Maven did not produce a valid effective POM");
    const auto project = document.RootElement();
    if (!project || std::string(project->Name()) != "project")
        throw std::runtime_error("Select one Maven module directory for runtime metadata");
    const auto build = element(project, "build");
    auto name = xmlText(project, "artifactId");
    if (name.empty())
        throw std::runtime_error("Maven artifactId is missing");
    const auto output = xmlText(build, "outputDirectory");
    if (output.empty())
        throw std::runtime_error("Maven effective model has no class output");
    const auto classes = resolve(pathUtf8(directory / utf8Path(output)));
    return {{"name", name},
            {"module", ""},
            {"directory", pathUtf8(directory)},
            {"outputs", Json::array({pathUtf8(classes)})},
            {"classpath", Json::array({pathUtf8(classes)})}};
}
nlohmann::json gradleRuntimeModels(const std::string &source, const JvmResolver &resolve) {
    if (source.empty() || source.size() > 4 * 1024 * 1024 || source.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid Gradle model size");
    auto data = Json::parse(source, [](int depth, Json::parse_event_t, Json &) {
        if (depth > 12)
            throw std::runtime_error("Gradle model nesting exceeds its limit");
        return true;
    });
    if (data.at("schemaVersion") != 1 || !data.at("projects").is_array() || data.at("projects").size() > 256)
        throw std::runtime_error("Invalid Gradle model version or project count");
    Json models = Json::array();
    std::set<std::string> seen;
    for (const auto &item : data.at("projects")) {
        const auto name = string(item, "name", 1024), module = string(item, "module", 4096);
        if (!seen.insert(module).second)
            throw std::runtime_error("Duplicate Gradle module");
        const auto directory = resolve(string(item, "directory")), build = resolve(string(item, "buildFile"));
        if (!item.at("outputs").is_array() || item.at("outputs").size() > 32 ||
            !item.at("classpath").is_array() || item.at("classpath").size() > 10000)
            throw std::runtime_error("Excessive Gradle runtime metadata");
        Json outputs = Json::array(), classpath = Json::array();
        for (const auto &path : item.at("outputs"))
            outputs.push_back(pathUtf8(resolve(path.get<std::string>())));
        for (const auto &path : item.at("classpath")) {
            const auto value = path.get<std::string>();
            if (value.empty() || value.size() > 32768 || !utf8Path(value).is_absolute() ||
                value.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid Gradle classpath entry");
            classpath.push_back(value);
        }
        models.push_back({{"name", name},
                          {"module", module},
                          {"directory", pathUtf8(directory)},
                          {"buildFile", pathUtf8(build)},
                          {"outputs", outputs},
                          {"classpath", classpath}});
    }
    return models;
}
std::string jvmRuntimeClasspath(const nlohmann::json &entries) {
    if (!entries.is_array() || entries.size() > 10000)
        throw std::runtime_error("Invalid JVM runtime classpath");
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    std::string result;
    for (const auto &entry : entries) {
        const auto path = entry.get<std::string>();
        if (path.empty() || path.size() > 32768 || !utf8Path(path).is_absolute() ||
            path.find('\0') != std::string::npos || path.find_first_of("\r\n*") != std::string::npos ||
            path.find(separator) != std::string::npos)
            throw std::runtime_error("Invalid or unrepresentable JVM classpath entry");
        if (!result.empty())
            result += separator;
        result += path;
        if (result.size() > 1024 * 1024)
            throw std::runtime_error("JVM runtime classpath exceeds 1 MiB");
    }
    if (result.empty())
        throw std::runtime_error("JVM runtime classpath is empty");
    return result;
}
std::string jvmArgumentFile(const std::vector<std::string> &arguments) {
    if (arguments.size() > 256)
        throw std::runtime_error("Excessive JVM argument count");
    std::string output;
    for (const auto &arg : arguments) {
        if (arg.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid NUL in JVM argument");
        output += '"';
        for (const auto c : arg)
            switch (c) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            case '\f':
                output += "\\f";
                break;
            default:
                output += c;
            }
        output += "\"\n";
        if (output.size() > 4 * 1024 * 1024)
            throw std::runtime_error("JVM argument file exceeds 4 MiB");
    }
    return output;
}
} // namespace cycode
