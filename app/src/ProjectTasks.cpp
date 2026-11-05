#include "cycode/ProjectTasks.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include "cycode/Languages.h"
#include "cycode/JvmBuild.h"
#include "cycode/ShaderBuild.h"
#include "cycode/CargoBuild.h"
#include "cycode/RustAnalysis.h"
#include "cycode/RustToolchain.h"
#include "cycode/JavaDebug.h"
#include "cycode/BuildLimits.h"
#include "cycode/TaskOutputText.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <iomanip>
namespace cycode {
namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
std::string read(const fs::path &path) { return readTextFileBytes(path); }
Json jsonFile(const fs::path &path) { return Json::parse(read(path)); }
void atomic(const fs::path &path,const Json &value) {
    const auto text=value.dump(2)+"\n";if (text.size()>maxTextFileBytes) throw std::runtime_error("Project metadata exceeds 4 MiB");
    fs::create_directories(path.parent_path());
    const auto baseline=fs::exists(path) ? std::optional<std::string>(read(path)) : std::nullopt;
    writeTextFileAtomic(path,text,baseline);
}
std::string stamp(const fs::path &path) {
    if (!fs::is_regular_file(path)) return "missing";
    uint64_t hash=14695981039346656037ULL;for (unsigned char c : read(path)) { hash^=c;hash*=1099511628211ULL; }
    return std::to_string(hash);
}
std::string limited(const Json &value,const char *name,size_t maximum=4096) {
    const auto text=value.at(name).get<std::string>();
    if (text.size()>maximum || text.find('\0')!=std::string::npos || text.find('\r')!=std::string::npos || text.find('\n')!=std::string::npos) throw std::runtime_error("Invalid project field: "+std::string(name));
    return text;
}
std::string npmCli(const std::string &node) {
    if (auto configured=toolPath("npm_cli");!configured.empty()) return configured;
    if (node.empty()) return "";
    const auto bin=utf8Path(node).parent_path();
    for (const auto &path : {bin/"node_modules/npm/bin/npm-cli.js",bin.parent_path()/"lib/node_modules/npm/bin/npm-cli.js",fs::path("/usr/share/nodejs/npm/bin/npm-cli.js")})
        if (fs::is_regular_file(path)) return pathUtf8(path);
    return "";
}
std::string cargo() {
    auto path=toolPath("cargo"); if (!path.empty()) return path;
#ifdef _WIN32
    const auto bundled=installationRoot()/"runtime/tools/rust/bin/cargo.exe";
#else
    const auto bundled=installationRoot()/"runtime/tools/rust/bin/cargo";
#endif
    return fs::is_regular_file(bundled) ? pathUtf8(bundled) : "";
}
std::map<std::string,std::string> cargoEnvironment(const fs::path &directory,const std::string &toolchain) {
    std::map<std::string,std::string> environment;
    const auto launch=defaultServerLaunch(directory,"rust");
    if(toolchain.empty()) {
        if (toolPath("cargo").empty()) for (const auto &[name,value] : launch.environment.items()) if (value.is_string()) environment[name]=value.get<std::string>();
    } else {const auto selected=rustToolchainEnvironment(toolchain);for(const auto &[name,value]:selected.items())environment[name]=value.get<std::string>();}
    environment["CARGO_TARGET_DIR"]=pathUtf8(directory/".cycode/cargo-target");environment["CARGO_TERM_COLOR"]="never";
    return environment;
}
std::vector<fs::path> sources(const fs::path &directory,const std::string &extension,size_t limit=5000,const std::atomic<bool> *cancel=nullptr) {
    std::vector<fs::path> files;size_t visited=0;
    for (auto it=fs::recursive_directory_iterator(directory,fs::directory_options::skip_permission_denied);it!=fs::recursive_directory_iterator();++it) {
        if(cancel&&cancel->load())break;
        if(++visited>200000||it.depth()>64)throw std::runtime_error("Java source traversal exceeds its entry/depth limit");
        const auto name=it->path().filename().string();
        if (it->is_directory() && (name==".git" || name==".cycode" || name=="node_modules" || (extension!=".class"&&(name=="target" || name=="build" || name==".gradle")))) { it.disable_recursion_pending();continue; }
        if (it->is_regular_file() && it->path().extension()==extension) { if (files.size()>=limit) throw std::runtime_error("Project source limit exceeded");files.push_back(it->path()); }
    }
    std::sort(files.begin(),files.end());return files;
}
// Read executable entry points from actual class files, without source regexes.
std::string javaMain(const fs::path &path) {
    try {
        const auto bytes=read(path);size_t at=0;
        auto number=[&](size_t count)->uint32_t { if (at+count>bytes.size()) throw std::runtime_error("Invalid class file");uint32_t value=0;while (count--) value=value*256+static_cast<unsigned char>(bytes[at++]);return value; };
        auto skip=[&](size_t count) { if (count>bytes.size()-at) throw std::runtime_error("Invalid class file");at+=count; };
        if (number(4)!=0xcafebabe) return "";skip(4);const auto count=number(2);
        std::vector<std::string> strings(count);std::vector<uint32_t> classes(count);
        for (uint32_t i=1;i<count;++i) switch (number(1)) {
            case 1: { const auto size=number(2);if (at+size>bytes.size()) return "";strings[i]=bytes.substr(at,size);skip(size);break; }
            case 7: classes[i]=number(2);break;
            case 8:case 16:case 19:case 20: skip(2);break;
            case 3:case 4:case 9:case 10:case 11:case 12:case 17:case 18: skip(4);break;
            case 5:case 6: skip(8);++i;break;
            case 15: skip(3);break;
            default:return "";
        }
        skip(2);const auto self=number(2);skip(2);const auto interfaces=number(2);skip(interfaces*2);
        auto attributes=[&] { const auto count=number(2);for (uint32_t i=0;i<count;++i) { skip(2);const auto size=number(4);skip(size); } };
        const auto fields=number(2);for (uint32_t i=0;i<fields;++i) { skip(6);attributes(); }
        const auto methods=number(2);bool main=false;
        for (uint32_t i=0;i<methods;++i) { const auto flags=number(2),name=number(2),descriptor=number(2);
            if ((flags&9)==9 && name<count && descriptor<count && strings[name]=="main" && strings[descriptor]=="([Ljava/lang/String;)V") main=true;
            attributes(); }
        if (!main || self>=count || classes[self]>=count) return "";
        auto name=strings[classes[self]];std::replace(name.begin(),name.end(),'/','.');(void)Json(name).dump();return name;
    } catch (...) { return ""; }
}
Json process(const ProcessResult &value) { return {{"exitCode",value.exitCode},{"output",value.output},{"timedOut",value.timedOut},{"truncated",value.truncated},{"cancelled",value.cancelled}}; }
std::vector<fs::path> shaderSources(const fs::path &directory) {
    std::vector<fs::path> files;size_t visited=0;
    for(auto it=fs::recursive_directory_iterator(directory,fs::directory_options::skip_permission_denied);it!=fs::recursive_directory_iterator();++it) {
        if(++visited>200000||it.depth()>64)throw std::runtime_error("Shader source traversal exceeds its entry/depth limit");
        const auto name=pathUtf8(it->path().filename());
        if(it->is_directory()&&(name==".git"||name==".cycode"||name=="node_modules"||name=="Binaries"||name=="Intermediate"||name=="build"||name=="target")) {it.disable_recursion_pending();continue;}
        if(!it->is_symlink()&&it->is_regular_file()&&shaderSourceFile(it->path())) {if(files.size()>=4096)throw std::runtime_error("Shader source count exceeds 4096");files.push_back(it->path());}
    }
    std::sort(files.begin(),files.end());return files;
}
}
ProjectTasks::ProjectTasks(fs::path root,fs::path entry,Resolver resolve,Terminals &terminals)
    : root_(std::move(root)),entry_(std::move(entry)),resolve_(std::move(resolve)),terminals_(terminals) {}
std::string ProjectTasks::detect(const fs::path &directory) {
    if (fs::is_regular_file(directory/"Cargo.toml")) return "cargo";
    if (fs::is_regular_file(directory/"package.json")) return "npm";
    if (fs::is_regular_file(directory/"pom.xml")) return "maven";
    if (fs::is_regular_file(directory/"build.gradle") || fs::is_regular_file(directory/"build.gradle.kts") || fs::is_regular_file(directory/"settings.gradle") || fs::is_regular_file(directory/"settings.gradle.kts")) return "gradle";
    if (fs::is_directory(directory/"src/main/java")) return "java";
    std::error_code ec;for (const auto &entry : fs::directory_iterator(directory,ec)) if (entry.path().extension()==".java") return "java";
    // Shader tasks must not replace the primary CMake project adapter.
    if(!fs::exists(directory/"CMakeLists.txt")) {size_t visited=0;for(const auto &item:fs::directory_iterator(directory,ec)){if(++visited>20000)break;if(!item.is_symlink()&&item.is_regular_file()&&shaderSourceFile(item.path()))return "shader";}}
    return "";
}
Json ProjectTasks::profile(const Json &params) const {
    Json value={{"system","automatic"},{"directory","."},{"configuration","Debug"},{"package",""},{"target",""},{"script",""},{"mainClass",""},{"sourceDirectory","."},{"offline",false},{"args",Json::array()},{"timeoutSeconds",180}};
    value.update({{"shaderSource",""},{"shaderLanguage","automatic"},{"shaderStage","automatic"},{"shaderTarget","automatic"},{"shaderEntry","main"},{"shaderModel","6_0"},{"shaderDefines",Json::array()},{"shaderIncludes",Json::array()}});
    value.update({{"cargoToolchain",""},{"cargoWorkspace",false},{"cargoTargetKind","bin"},{"cargoProfile",""},{"cargoFeatures",Json::array()},
                  {"cargoAllFeatures",false},{"cargoNoDefaultFeatures",false},{"cargoLocked",false}});
    const auto saved=resolve_(".cycode/project-tasks.json");
    if (fs::is_regular_file(saved)) { const auto previous=jsonFile(saved);for (auto it=value.begin();it!=value.end();++it) if (previous.contains(it.key())) it.value()=previous.at(it.key()); }
    for (auto it=value.begin();it!=value.end();++it) if (params.contains(it.key())) it.value()=params.at(it.key());
    for (auto name : {"system","directory","configuration","package","target","script","mainClass","sourceDirectory"}) (void)limited(value,name);
    const auto directory=resolve_(value.at("directory"));if (!fs::is_directory(directory)) throw std::runtime_error("Project directory does not exist");
    value["directory"]=pathUtf8(directory.lexically_relative(root_));
    if (value.at("system")=="automatic") value["system"]=detect(directory);
    const auto system=value.at("system").get<std::string>();
    if (system!="" && system!="cargo" && system!="npm" && system!="java" && system!="maven" && system!="gradle" && system!="shader") throw std::runtime_error("Unsupported project system");
    if(system=="cargo")validateCargoOptions(value);
    if(system=="shader") {
        for(auto key:{"shaderSource","shaderLanguage","shaderStage","shaderTarget","shaderEntry","shaderModel"})(void)limited(value,key,8192);
        if(value.at("shaderSource")==""&&fs::is_regular_file(entry_)&&shaderSourceFile(entry_))value["shaderSource"]=pathUtf8(entry_.lexically_relative(root_));
        if(!value.at("shaderDefines").is_array()||value.at("shaderDefines").size()>128||!value.at("shaderIncludes").is_array()||value.at("shaderIncludes").size()>64)throw std::runtime_error("Invalid shader definitions/include directories");
        if(!value.at("args").empty())throw std::runtime_error("Shader tasks use typed compiler options; clear application arguments");
    }
    if(system=="gradle"&&!value.at("package").get_ref<const std::string &>().empty()&&!value.at("package").get_ref<const std::string &>().starts_with(':'))value["package"]=":"+value.at("package").get<std::string>();
    (void)buildTimeout(value);
    if (value.at("configuration")!="Debug" && value.at("configuration")!="Release") throw std::runtime_error("Project configuration must be Debug or Release");
    if (!value.at("offline").is_boolean() || !value.at("args").is_array() || value.at("args").size()>128) throw std::runtime_error("Invalid project arguments");
    for (const auto &arg : value.at("args")) if (!arg.is_string() || arg.get_ref<const std::string &>().size()>8192 || arg.get_ref<const std::string &>().find('\0')!=std::string::npos) throw std::runtime_error("Invalid project argument");
    for (auto name : {"package","target","script"}) { const auto field=value.at(name).get<std::string>();if (field.starts_with('-') || field.find_first_of("*?[]")!=std::string::npos) throw std::runtime_error("Invalid project target or script"); }
    if(system=="java") {const auto source=resolve_(pathUtf8(directory/utf8Path(value.at("sourceDirectory"))));
        if (!fs::is_directory(source)) throw std::runtime_error("Java source directory does not exist");}
    return value;
}
Json ProjectTasks::model(const Json &value) const {
    const auto directory=resolve_(value.at("directory"));const auto kind=value.at("system").get<std::string>();
    const auto node=toolPath("node");Json tools={{"cargo",cargo()},{"rustup",toolPath("rustup")},{"node",node},{"npm",npmCli(node)},{"java",toolPath("java")},{"javac",toolPath("javac")},{"dxc",toolPath("dxc")},{"glslangValidator",toolPath("glslangValidator")}};
    Json result={{"available",!kind.empty()},{"kind",kind},{"name",pathUtf8(directory.filename())},{"profile",value},{"tools",tools},
        {"targets",Json::array()},{"scripts",Json::array()},{"sources",Json::array()},{"artifacts",Json::array()},{"configured",false},{"stale",false},{"message",""},
        {"packages",Json::array()},{"workspaceRoot",""}};
    if (kind=="cargo" && !fs::is_regular_file(resolve_(pathUtf8(directory/"Cargo.toml")))) throw std::runtime_error("Cargo.toml is missing");
    if(kind=="cargo")result["rustAnalysis"]=cargoRustAnalysisProfile(value,root_);
    if (kind=="npm") {
        auto manifest=jsonFile(resolve_(pathUtf8(directory/"package.json")));result["name"]=manifest.value("name",result.at("name").get<std::string>());
        if (manifest.contains("scripts")) { if (!manifest.at("scripts").is_object() || manifest.at("scripts").size()>500) throw std::runtime_error("Invalid or excessive npm scripts");
            for (const auto &[name,command] : manifest.at("scripts").items()) if (command.is_string()) result["scripts"].push_back({{"name",name},{"command",command}}); }
    }
    if (kind=="java") {
        for (const auto &file : sources(resolve_(pathUtf8(directory/utf8Path(value.at("sourceDirectory")))),".java")) result["sources"].push_back(pathUtf8(resolve_(pathUtf8(file)).lexically_relative(root_)));
        if (fs::exists(directory/"pom.xml") || fs::exists(directory/"build.gradle") || fs::exists(directory/"build.gradle.kts")) result["message"]="Maven/Gradle descriptor detected. This adapter compiles standalone Java sources; use its source directory explicitly.";
    }
    const auto cache=resolve_(".cycode/project-build.json");
    if (kind!="shader"&&fs::is_regular_file(cache)) {
        const auto saved=jsonFile(cache);
        const auto same=saved.value("profile",Json::object());bool matching=true;
        for (auto key : {"system","directory","configuration","package","target","sourceDirectory"}) if (!same.contains(key) || same.at(key)!=value.at(key)) matching=false;
        if(kind=="cargo")for(auto key:{"cargoToolchain","cargoWorkspace","cargoTargetKind","cargoProfile","cargoFeatures","cargoAllFeatures","cargoNoDefaultFeatures","cargoLocked"})
            if(!same.contains(key)||same.at(key)!=value.at(key))matching=false;
        if (matching) {
            const auto savedInputs=saved.value("inputs",Json::object());
            bool stale=false;for (const auto &[path,hash] : savedInputs.items()) if (stamp(resolve_(path))!=hash.get<std::string>()) stale=true;
            const bool complete=saved.value("complete",true);result["configured"]=complete&&!stale;result["stale"]=stale;
            if (!stale&&complete) { result["targets"]=saved.value("targets",Json::array());result["artifacts"]=saved.value("artifacts",Json::array());
                if(kind=="cargo"){result["packages"]=saved.value("packages",Json::array());result["workspaceRoot"]=saved.value("workspaceRoot",std::string{});} }
        }
    }
    if (kind=="maven" || kind=="gradle") {
        const auto descriptor=kind=="maven"?"pom.xml":fs::is_regular_file(directory/"build.gradle.kts")?"build.gradle.kts":"build.gradle";
        if(kind=="maven"&&!fs::is_regular_file(resolve_(pathUtf8(directory/descriptor))))throw std::runtime_error("Maven pom.xml is missing");
        result["scripts"]=kind=="maven"?Json::array({{{"name","clean"}},{{"name","verify"}},{{"name","install"}}}):Json::array({{{"name","clean"}},{{"name","assemble"}},{{"name","check"}}});
        result["message"]="Maven/Gradle execute project rules only on Configure/Build/Test/Run. Runtime classpaths come from actual build models; project JVM options are owned by the selected Java runtime.";
    }
    if(kind=="shader") {
        for(const auto &file:shaderSources(directory))result["sources"].push_back(pathUtf8(resolve_(pathUtf8(file)).lexically_relative(root_)));
        result["message"]="Shader tasks compile one selected HLSL/GLSL source with installed DXC/glslang. Artifacts describe this compilation only; includes require rebuilding. GPU preview is not connected.";
    }
    if (kind.empty()) result["message"]="Select a Cargo, npm, Java, Maven, Gradle or shader project directory.";
    return result;
}
Json ProjectTasks::inspect(const Json &params) { std::lock_guard lock(operationMutex_);return model(profile(params)); }
void ProjectTasks::save(const Json &value) const { atomic(resolve_(".cycode/project-tasks.json"),value); }
Json ProjectTasks::cancel(const std::string &id) {
    std::lock_guard lock(stateMutex_);const bool active=!runningId_.empty() && (id.empty() || id==runningId_);
    if (active) {cancellation_=true;output_.cancelling();}return {{"cancelled",active}};
}
Json ProjectTasks::status() const {
    std::lock_guard lock(stateMutex_);
    return {{"running",!runningId_.empty()},{"operationId",runningId_},{"method",runningMethod_},
            {"cancelling",!runningId_.empty()&&cancellation_.load()}};
}
Json ProjectTasks::output(const Json &parameters) const {return output_.read(parameters);}
Json ProjectTasks::execute(const std::string &method,const Json &params) {
    static const std::set<std::string> methods={"project.configure","project.build","project.test","project.install","project.script","project.launch","project.debug"};
    if (!methods.contains(method)) throw std::runtime_error("Unknown project operation");
    auto id=params.value("operationId",std::string("local"));
    if (id.empty() || id.size()>128 || id.find('\0')!=std::string::npos) throw std::runtime_error("Invalid project operation ID");
    std::unique_lock operation(operationMutex_,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("A project task is already running; wait or cancel it first");
    auto runningMethod=method;
    { std::lock_guard state(stateMutex_);cancellation_=false;
      outputStream_=output_.begin(method,id,{{"system",""},{"directory",""}});
      runningId_.swap(id);runningMethod_.swap(runningMethod); }
    auto finish=[&] {std::lock_guard state(stateMutex_);runningId_.clear();runningMethod_.clear();output_.finish();};
    try {
        ProcessOutputScope outputScope(&cancellation_,output_.observer(outputStream_));
        auto result=perform(method,params);finish();return result;
    } catch (...) {finish();throw;}
}
Json ProjectTasks::perform(const std::string &method,const Json &params) {
    const auto value=profile(params);auto project=model(value);const auto kind=project.at("kind").get<std::string>();
    if (kind.empty()) throw std::runtime_error("No Cargo, npm, Java, Maven, Gradle or shader project is selected");
    output_.context(outputStream_,{{"system",kind},{"directory",value.at("directory")}});
    if (kind!="npm" && kind!="maven" && kind!="gradle" && (method=="project.install" || method=="project.script")) throw std::runtime_error("This operation requires an npm, Maven or Gradle project");
    const auto directory=resolve_(value.at("directory"));const auto tools=project.at("tools");
    ProcessResult combined;combined.exitCode=0;Json diagnostics=Json::array(),artifacts=Json::array(),targets=Json::array(),inputs=Json::object();
    Json cargoModel={{"packages",Json::array()},{"targets",Json::array()},{"workspaceRoot",""}};
    const auto started=std::chrono::steady_clock::now();
    auto execute=[&](const std::vector<std::string> &argv,const std::map<std::string,std::string> &environment=std::map<std::string,std::string>{},bool publish=true,bool cargoMessages=false) {
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(buildTimeout(value))-std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started);
        TaskOutputText display(publish?processOutputObserver(&cancellation_):ProcessOutputObserver{},cargoMessages);
        auto result=cancellation_.load()?ProcessResult{-1,"Project task cancelled",false,false,true}:remaining.count()<=0?ProcessResult{-1,"Project task deadline expired",true}:runProcess(argv,directory,remaining,4*1024*1024,&cancellation_,environment,display.observer());
        display.flush();
        combined.exitCode=result.exitCode;combined.timedOut=result.timedOut;combined.truncated=result.truncated;combined.cancelled=result.cancelled;
        if (combined.output.size()+result.output.size()>4*1024*1024) { result.output.resize(4*1024*1024-combined.output.size());combined.truncated=true; }
        combined.output+=result.output;return result;
    };
    auto requireTool=[&](const char *name) { auto path=tools.at(name).get<std::string>();if (path.empty()) throw std::runtime_error(std::string("Required project tool is missing: ")+name);return path; };
    auto input=[&](const fs::path &path) { const auto key=pathUtf8(resolve_(pathUtf8(path)).lexically_relative(root_));inputs[key]=stamp(path); };
    const bool launch=method=="project.launch",debug=method=="project.debug";
    if(kind=="shader") {
        if(method!="project.configure"&&method!="project.build")throw std::runtime_error("Shader tasks support Configure and Build; no standalone shader run, GPU preview or debugger is connected");
        auto sourceName=value.at("shaderSource").get<std::string>();
        if(sourceName.empty()&&project.at("sources").size()==1)sourceName=project.at("sources").front();
        auto configured=value;configured["shaderSource"]=sourceName;
        if(sourceName.empty()) {
            if(method!="project.configure")throw std::runtime_error("Select one shader source in Project tasks");
            save(configured);auto result=process(combined);result["project"]=model(configured);result["project"]["configured"]=true;result["diagnostics"]=diagnostics;return result;
        }
        const auto source=resolve_(sourceName);
        if(!fs::is_regular_file(source))throw std::runtime_error("Shader source does not exist");
        const auto build=resolve_(".cycode/shader-build/"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto plan=shaderBuildPlan(configured,source,build/"shader.bin",resolve_);
        const auto output=resolve_(pathUtf8(build/utf8Path("shader"+plan.extension)));
        plan=shaderBuildPlan(configured,source,output,resolve_);
        if(method=="project.build") {
            const auto compiler=requireTool(plan.tool.c_str());
            fs::create_directories(build);
            auto argv=plan.arguments;argv.insert(argv.begin(),compiler);execute(argv);
            diagnostics=shaderBuildDiagnostics(combined.output,source,root_,directory,resolve_);
            if(combined.exitCode==0&&!combined.cancelled&&!combined.timedOut&&!combined.truncated&&!cancellation_.load()) {
                validateShaderArtifact(output,plan.format);
                artifacts.push_back({{"name",pathUtf8(source.filename())},{"path",pathUtf8(output.lexically_relative(root_))},{"format",plan.format},{"language",plan.language},{"stage",plan.stage}});
            }
        }
        combined.cancelled=combined.cancelled||cancellation_.load();
        if(combined.exitCode==0&&!combined.cancelled&&!combined.timedOut&&!combined.truncated)save(configured);
        auto result=process(combined);result["project"]=model(configured);result["project"]["configured"]=combined.exitCode==0&&!combined.cancelled&&!combined.timedOut&&!combined.truncated;
        result["project"]["artifacts"]=artifacts;result["diagnostics"]=diagnostics;return result;
    }
    if (kind!="npm" && (method!="project.configure"||kind=="cargo"||kind=="maven"||kind=="gradle")) atomic(resolve_(".cycode/project-build.json"),{{"profile",value},{"complete",false},{"inputs",Json::object()},{"targets",Json::array()},{"artifacts",Json::array()}});
    if (kind=="maven" || kind=="gradle") {
        if(method=="project.install")throw std::runtime_error("Use an explicit Maven/Gradle build task; dependency resolution follows the project's build model");
        const auto launcher=jvmBuildLauncher(root_,directory,kind,requireTool("java"),resolve_);
        const auto id=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto work=resolve_(".cycode/jvm-build/"+id);fs::create_directories(work);
        Json runtimeModels=Json::array();
        auto tool=[&](std::vector<std::string> arguments) {
            auto argv=launcher;
            if(kind=="maven")argv.insert(argv.end(),{"--batch-mode","--no-transfer-progress","-Dstyle.color=never"});
            else argv.insert(argv.end(),{"--no-daemon","--console=plain"});
            if(value.at("offline").get<bool>())argv.push_back(kind=="maven"?"--offline":"--offline");
            argv.insert(argv.end(),arguments.begin(),arguments.end());return execute(argv);
        };
        for(const auto *name:{"pom.xml","build.gradle","build.gradle.kts","settings.gradle","settings.gradle.kts","gradle.properties","gradle/libs.versions.toml","gradle/wrapper/gradle-wrapper.properties",".mvn/wrapper/maven-wrapper.properties",".mvn/maven.config",".mvn/jvm.config"})input(directory/name);
        const auto module=limited(value,"package"),task=limited(value,"script");
        auto taskName=[&](const std::string &name) {
            if(name.empty()||name.size()>256||name.starts_with('-')||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.:")!=std::string::npos||name.find("..")!=std::string::npos)throw std::runtime_error("Select one simple Maven goal or Gradle task");
            return name;
        };
        if(method!="project.configure") {
            const auto goal=method=="project.test"?"test":method=="project.script"?taskName(task):kind=="maven"?"package":"build";
            std::vector<std::string> arguments;
            if(kind=="maven") {if(!module.empty())throw std::runtime_error("Select the Maven module using Project directory; the package field is for Gradle modules");arguments={goal};if(method=="project.build"||launch||debug)arguments.push_back("-DskipTests");}
            else {const auto prefix=module.empty()?std::string{}:taskName(module)+(module.ends_with(':')?"":":");arguments={prefix+goal};}
            (void)tool(arguments);
        }
        if(!combined.exitCode&&!combined.timedOut&&!combined.truncated&&!combined.cancelled) {
            if(kind=="maven") {
                const auto effective=work/"effective-pom.xml";
                const auto result=tool({"--non-recursive","org.apache.maven.plugins:maven-help-plugin:3.5.2:effective-pom","-Doutput="+pathUtf8(effective)});
                if(!result.exitCode&&!result.timedOut&&!result.truncated&&!result.cancelled) {
                    auto model=mavenRuntimeModel(read(resolve_(pathUtf8(effective))),directory,resolve_);
                    if(launch||debug) {
                        const auto dependencies=work/"classpath.txt";
                        const auto resolved=tool({"--non-recursive","org.apache.maven.plugins:maven-dependency-plugin:3.11.0:build-classpath","-DincludeScope=runtime","-Dmdep.outputFile="+pathUtf8(dependencies)});
                        if(!resolved.exitCode&&!resolved.timedOut&&!resolved.truncated&&!resolved.cancelled) {
                            if (!fs::is_regular_file(dependencies)) throw std::runtime_error("Maven did not write the resolved runtime classpath");
                            auto content=read(resolve_(pathUtf8(dependencies)));while(!content.empty()&&(content.back()=='\r'||content.back()=='\n'))content.pop_back();
#ifdef _WIN32
                            constexpr char separator=';';
#else
                            constexpr char separator=':';
#endif
                            std::istringstream stream(content);std::string part;while(std::getline(stream,part,separator))if(!part.empty())model["classpath"].push_back(part);
                        }
                    }
                    runtimeModels.push_back(std::move(model));
                }
            } else {
                const auto script=work/"model.gradle",output=work/"model.json";
                writeTextFileAtomic(script,gradleModelScript(),std::nullopt);
                const auto result=tool({"--init-script",pathUtf8(script),"-Dcycode.modelTask=cycodeExport"+id,"-Dcycode.modelOutput="+pathUtf8(output),"cycodeExport"+id});
                if(!result.exitCode&&!result.timedOut&&!result.truncated&&!result.cancelled)runtimeModels=gradleRuntimeModels(read(resolve_(pathUtf8(output))),resolve_);
            }
        }
        size_t inspectedClasses=0;
        if(!combined.exitCode&&!combined.timedOut&&!combined.truncated&&!combined.cancelled)for(const auto &model:runtimeModels) {
            if(kind=="gradle"&&!module.empty()&&model.at("module")!=module)continue;
            targets.push_back({{"name",model.at("name")},{"package",model.at("module")},{"kind",Json::array({"java"})},{"path",pathUtf8(resolve_(model.at("directory")).lexically_relative(root_))}});
            if(model.contains("buildFile"))input(utf8Path(model.at("buildFile")));
            for(const auto &directory:model.at("outputs"))if(fs::is_directory(resolve_(directory)))for(const auto &file:sources(resolve_(directory),".class",5000,&cancellation_)) {
                if(cancellation_.load())break;
                if(++inspectedClasses>20000||artifacts.size()>=2000)throw std::runtime_error("JVM class/artifact inspection limit exceeded");
                const auto main=javaMain(file);if(!main.empty())artifacts.push_back({{"name",main},{"path",pathUtf8(resolve_(pathUtf8(file)).lexically_relative(root_))},{"kind","java"},{"mainClass",main},{"package",model.at("module")},{"classPathEntries",model.at("classpath")}});
            }
        }
    } else if (kind=="cargo") {
        const auto toolchain=value.at("cargoToolchain").get<std::string>();
        const auto tool=toolchain.empty()?requireTool("cargo"):std::string("cargo");
        const auto rustup=toolchain.empty()?std::string{}:requireTool("rustup");
        const auto environment=cargoEnvironment(directory,toolchain);const auto manifest=resolve_(pathUtf8(directory/"Cargo.toml"));
        // Validate Run/Debug kinds before invoking Cargo or evaluating metadata.
        const auto taskArguments=cargoArguments(value,method,manifest);
        auto metadata=cargoArguments(value,"project.configure",manifest);metadata.insert(metadata.begin(),tool);metadata=rustToolchainCommand(toolchain,rustup,metadata);
        auto result=execute(metadata,environment,false);
        if (!result.exitCode&&!result.timedOut&&!result.truncated&&!result.cancelled) {
            Json data;std::istringstream lines(result.output);std::string line;while (std::getline(lines,line)) if (line.starts_with('{')) { auto parsed=Json::parse(line,nullptr,false);if (parsed.is_object() && parsed.contains("packages")) data=std::move(parsed); }
            if (data.is_null()) throw std::runtime_error("Cargo did not return format-version 1 metadata");
            combined.output.clear();input(manifest);cargoModel=cargoWorkspaceModel(data,root_,resolve_);targets=cargoModel.at("targets");
            for(const auto &package:cargoModel.at("packages"))input(resolve_(package.at("manifest")));
            const auto workspaceRoot=resolve_(cargoModel.at("workspaceRoot"));input(workspaceRoot/"Cargo.toml");input(workspaceRoot/"Cargo.lock");
            // Cargo configuration/toolchain lookup includes ancestors of the working directory.
            for(auto parent=directory;;parent=parent.parent_path()) {
                for(auto name:{".cargo/config",".cargo/config.toml","rust-toolchain","rust-toolchain.toml"})input(parent/name);
                if(parent==root_)break;
                if(parent.parent_path()==parent)throw std::runtime_error("Cargo directory is outside workspace");
            }
            if (method!="project.configure") {
                auto argv=taskArguments;argv.insert(argv.begin(),tool);argv=rustToolchainCommand(toolchain,rustup,argv);
                result=execute(argv,environment,true,true);combined.output.clear();std::istringstream messages(result.output);
                while (std::getline(messages,line)) {
                    auto item=Json::parse(line,nullptr,false);
                    if (item.is_object() && item.value("reason","")=="compiler-message") {
                        const auto message=item.at("message");combined.output+=message.value("rendered",message.value("message",""))+"\n";
                        for (const auto &span : message.value("spans",Json::array())) if (span.value("is_primary",false) && diagnostics.size()<5000) {
                            std::string path;try { path=pathUtf8(resolve_(pathUtf8(directory/utf8Path(span.at("file_name")))).lexically_relative(root_)); } catch (...) { continue; }
                            diagnostics.push_back({{"path",path},{"line",span.at("line_start")},{"column",span.at("column_start")},{"message",message.value("message","")},{"severity",message.value("level","")},{"code",message.value("code",Json()).is_object() ? message.at("code").value("code","") : ""}});
                        }
                    } else if (item.is_object() && item.value("reason","")=="compiler-artifact") {
                        const auto artifact=cargoExecutable(item,cargoModel,value,root_,resolve_);
                        if(!artifact.is_null()&&std::find(artifacts.begin(),artifacts.end(),artifact)==artifacts.end())artifacts.push_back(artifact);
                    } else if (!item.is_object()) combined.output+=line+"\n";
                }
            }
            input(workspaceRoot/"Cargo.lock");
        }
    } else if (kind=="npm") {
        const auto node=requireTool("node"),npm=requireTool("npm");input(directory/"package.json");input(directory/"package-lock.json");
        if (debug) throw std::runtime_error("JavaScript debugging is not connected; use Run in terminal");
        if (method!="project.configure") {
            std::vector<std::string> argv={node,npm};
            if (method=="project.install") argv.push_back(fs::is_regular_file(directory/"package-lock.json") ? "ci" : "install");
            else {
                auto script=method=="project.build" ? std::string("build") : method=="project.test" ? std::string("test") : limited(value,"script");
                if (script.empty()) for (auto preferred : {"start","dev"}) for (const auto &entry : project.at("scripts")) if (entry.at("name")==preferred && script.empty()) script=preferred;
                bool found=false;for (const auto &entry : project.at("scripts")) found=found || entry.at("name")==script;
                if (!found) throw std::runtime_error("Select an existing npm script in Project tasks");
                argv.insert(argv.end(),{"run-script",script,"--"});for (const auto &arg : value.at("args")) argv.push_back(arg);
            }
            if (launch) { Json output=process(combined);output["terminal"]=terminals_.create(directory,argv);save(value);output["project"]=model(value);output["diagnostics"]=diagnostics;return output; }
            (void)execute(argv);input(directory/"package-lock.json");
        }
    } else if (kind=="java") {
        if (method=="project.test" || method=="project.install") throw std::runtime_error("Java test/build dependency providers are not connected for standalone projects");
        if (method!="project.configure") {
            const auto compiler=requireTool("javac");if (project.at("sources").empty()) throw std::runtime_error("No Java source files in the selected source directory");
            const auto build=resolve_(".cycode/java-build/"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(build/"classes");
            const auto argumentFile=build/"javac.args";std::ostringstream arguments;
            arguments<<(value.at("configuration")=="Release" ? "-g:none\n" : "-g\n")<<"-encoding\nUTF-8\n-proc:none\n-d\n"<<std::quoted(pathUtf8(build/"classes"))<<'\n';
            for (const auto &source : project.at("sources")) { const auto path=resolve_(source);arguments<<std::quoted(pathUtf8(path))<<'\n';input(path); }
            writeTextFileAtomic(argumentFile,arguments.str(),std::nullopt);
            const auto result=execute({compiler,"-J-Duser.language=en","-J-Dstdout.encoding=UTF-8","-J-Dstderr.encoding=UTF-8","@"+pathUtf8(argumentFile)});
            std::istringstream errors(result.output);std::string line;
            while (std::getline(errors,line)) {
                const auto suffix=line.find(".java:");if (suffix==std::string::npos) continue;
                const auto end=line.find(':',suffix+6);if (end==std::string::npos) continue;
                const auto number=line.substr(suffix+6,end-(suffix+6));if (number.empty() || number.find_first_not_of("0123456789")!=std::string::npos) continue;
                try { const auto file=resolve_(pathUtf8(directory/utf8Path(line.substr(0,suffix+5))));
                    diagnostics.push_back({{"path",pathUtf8(file.lexically_relative(root_))},{"line",std::stoi(number)},{"column",1},{"message",line.substr(end+1)},{"code","javac"},{"severity","error"}}); } catch (...) {}
            }
            if (!result.exitCode) for (const auto &file : sources(build/"classes",".class")) {
                const auto main=javaMain(file);if (!main.empty()) artifacts.push_back({{"name",main},{"path",pathUtf8(file.lexically_relative(root_))},{"kind","java"},{"mainClass",main},{"classPath",pathUtf8((build/"classes").lexically_relative(root_))}});
            }
        }
    }
    combined.cancelled=combined.cancelled || cancellation_.load();
    if (combined.exitCode==0 && !combined.cancelled && !combined.truncated && !combined.timedOut) {
        atomic(resolve_(".cycode/project-build.json"),{{"profile",value},{"complete",true},{"inputs",inputs},{"targets",targets},{"artifacts",artifacts},
            {"packages",cargoModel.at("packages")},{"workspaceRoot",cargoModel.at("workspaceRoot")}});save(value);
        if (launch || debug) {
            const auto target=limited(value,"target"),main=limited(value,"mainClass");Json selected;
            for (const auto &artifact : artifacts) if ((kind=="java" || kind=="maven" || kind=="gradle" ? main.empty() || artifact.at("mainClass")==main : target.empty() || artifact.at("name")==target)) {
                if (!selected.is_null()) throw std::runtime_error("Select a runnable target in Project tasks");selected=artifact; }
            if (selected.is_null()) throw std::runtime_error("No runnable build artifact; select a binary target or Java main class");
            Json output=process(combined);output["project"]=model(value);output["diagnostics"]=diagnostics;
            if (debug) {
                if (kind == "java" || kind == "maven" || kind == "gradle") {
                    output["debugProfile"] = {{"adapterId", "builtin.java"}, {"adapterOptions", javaDebugTarget(selected, root_, requireTool("java"))},
                                               {"program", ""}, {"cwd", pathUtf8(directory.lexically_relative(root_))},
                                               {"args", value.at("args")}, {"runtime", "automatic"}};
                    if (kind == "java") output["debugProfile"]["adapterOptions"]["sourcePaths"] = Json::array({pathUtf8(resolve_(pathUtf8(directory / utf8Path(value.at("sourceDirectory")))))});
                }
                else output["program"]=selected.at("path");
                return output;
            }
            std::vector<std::string> argv;
            // The interactive JVM uses the attached console's encoding. Forcing
            // UTF-8 bytes through a Windows OEM ConPTY would double-convert them.
            if (kind=="maven" || kind=="gradle") argv={requireTool("java"),"--disable-@files","-cp",jvmRuntimeClasspath(selected.at("classPathEntries")),selected.at("mainClass")};
            else if (kind=="java") argv={requireTool("java"),"--disable-@files","-cp",pathUtf8(resolve_(selected.at("classPath"))),selected.at("mainClass")};
            else argv={pathUtf8(resolve_(selected.at("path")))};
            for (const auto &arg : value.at("args")) argv.push_back(arg);
            if(kind=="maven"||kind=="gradle") {
                const auto arguments=resolve_(".cycode/jvm-launch/"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".args");fs::create_directories(arguments.parent_path());
                const auto content=jvmArgumentFile(std::vector<std::string>(argv.begin()+1,argv.end()));
                writeTextFileAtomic(arguments,content,std::nullopt);argv={argv[0],"@"+pathUtf8(arguments)};
            }
            output["terminal"]=terminals_.create(directory,argv);return output;
        }
    }
    Json result=process(combined);result["project"]=model(value);result["diagnostics"]=diagnostics;return result;
}
}
