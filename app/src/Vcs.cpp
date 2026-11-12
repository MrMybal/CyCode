#include "cycode/ProcessText.h"
#include "cycode/Engine.h"
#include "cycode/GitSafety.h"
#include "cycode/TextEdits.h"
#include <algorithm>
#include <fstream>
#include <sstream>
namespace cycode {
static Json processJson(const ProcessResult &result) {
    return {{"exitCode",result.exitCode},{"output",processOutputText(result.output, true)},{"timedOut",result.timedOut},{"truncated",result.truncated}};
}
static std::string trim(std::string text) { while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back(); return text; }
static std::vector<std::string> fields(const std::string &text, char separator) {
    std::vector<std::string> result; size_t start = 0;
    for (;;) { const auto end = text.find(separator,start); result.push_back(text.substr(start,end-start)); if (end == std::string::npos) break; start = end+1; }
    return result;
}
void Engine::refreshGitDocuments(Json &result, const std::filesystem::path &scope) {
    const auto documents = dispatch("documents.list").at("documents");
    Json reloaded = Json::array(), retained = Json::array();
    for (const auto &doc : documents) {
        const auto path = doc.at("path").get<std::string>();
        if(isUntitledDocument(path))continue;
        try { if(!scope.empty()&&!containsPath(scope,resolve(path)))continue; }
        catch(const std::exception &){retained.push_back(path);continue;}
        if (doc.at("dirty").get<bool>()) { retained.push_back(path); continue; }
        try {
            auto live = dispatch("documents.read",{{"path",path}}); std::ifstream input(resolve(path),std::ios::binary);
            if (!input) { retained.push_back(path); continue; }
            // Avoid needless new revisions/undo entries for untouched files.
            const auto size = std::filesystem::file_size(resolve(path));
            if (size > 4*1024*1024) { retained.push_back(path); continue; }
            std::string raw((std::istreambuf_iterator<char>(input)),{}), baseline;
            { std::lock_guard lock(mutex_); auto found = documents_.find(path); if (found != documents_.end()) baseline = found->second.disk; }
            if (raw == baseline) continue;
            dispatch("documents.reload",{{"path",path},{"expectedRevision",live.at("revision")}}); reloaded.push_back(path);
        } catch (const std::exception &) { retained.push_back(path); }
    }
    result["reloaded"] = reloaded; result["retained"] = retained;
    if (!retained.empty()) result["output"] = result.value("output","")+"\nSome open buffers were retained because they changed or disappeared on disk.\n";
}
Json Engine::gitAdvanced(const std::string &method, const Json &params) {
    const auto selected=gitSelection(params);
    const auto &gitRoot=selected.directory;
    auto git = [&](std::vector<std::string> arguments, int seconds = 15) {
        arguments.insert(arguments.begin(),{"git","--no-pager","--literal-pathspecs","-C",pathUtf8(gitRoot),"-c","core.quotepath=false"});
        hardenGitArguments(arguments);
        return runProcess(arguments,gitRoot,std::chrono::seconds(seconds),8*1024*1024);
    };
    auto repository = git({"rev-parse","--show-toplevel"});
    if (method == "vcs.init") {
        if(selected.marker)throw std::runtime_error("This directory already has a Git marker");
        const auto bare = git({"rev-parse","--is-bare-repository"});
        if(!bare.exitCode && trim(bare.output)=="true")
            throw std::runtime_error("Cannot reinitialize a bare Git repository");
        return processJson(git({"init","--initial-branch=main"}));
    }
    if (repository.exitCode && method != "vcs.inspect") throw std::runtime_error("Open a Git repository first");
    auto repositoryRoot = repository.exitCode ? gitRoot : std::filesystem::weakly_canonical(utf8Path(trim(repository.output)));
    auto rootOnly = [&] {
        if (!containsPath(gitRoot,repositoryRoot) || !containsPath(repositoryRoot,gitRoot)) throw std::runtime_error("Open the repository root before changing repository-wide state");
    };
    auto cleanBuffers = [&] {
        auto documents = dispatch("documents.list").at("documents");
        for (const auto &doc : documents) if (!isUntitledDocument(doc.at("path")) && containsPath(gitRoot,resolve(doc.at("path"))) && doc.at("dirty").get<bool>()) throw std::runtime_error("Save modified buffers before changing Git working-tree state");
    };
    auto cleanTree = [&] {
        cleanBuffers(); auto status = git({"status","--porcelain","--untracked-files=no"});
        if (status.exitCode || !status.output.empty()) throw std::runtime_error("Commit or stash tracked working-tree changes first");
    };
    auto branch = [&](const std::string &name) {
        if (name.empty() || name.front() == '-' || name.find('\0') != std::string::npos || git({"check-ref-format","--branch",name}).exitCode) throw std::runtime_error("Invalid Git branch name");
        return name;
    };
    auto remote = [&] {
        const auto name = params.value("remote","origin");
        if (name.empty() || name.front() == '-' || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") != std::string::npos) throw std::runtime_error("Invalid remote name");
        return name;
    };
    auto reference = [&](const std::string &name) {
        if (name.empty() || name.find('\0') != std::string::npos) throw std::runtime_error("Invalid Git revision");
        auto result = git({"rev-parse","--verify","--end-of-options",name+"^{commit}"});
        if (result.exitCode) throw std::runtime_error("Unknown Git revision: "+name); return trim(result.output);
    };
    if (method == "vcs.inspect") {
        Json result = {{"repository",!repository.exitCode},{"root",repository.exitCode ? "" : pathUtf8(repositoryRoot)}, {"branch",""},{"head",""},
                       {"branches",Json::array()},{"remotes",Json::array()},{"stashes",Json::array()},{"ahead",0},{"behind",0},{"hasUpstream",false}};
        if (repository.exitCode) return result;
        result["branch"] = trim(git({"branch","--show-current"}).output);
        auto head = git({"rev-parse","--verify","HEAD"}); if (!head.exitCode) result["head"] = trim(head.output);
        auto refs = git({"for-each-ref","--format=%(refname:short)%00%(objectname)%00%(upstream:short)%00%(HEAD)%00%(refname)","refs/heads","refs/remotes"});
        std::istringstream lines(refs.output); std::string line;
        while (std::getline(lines,line)) { auto parts = fields(trim(line),'\0'); if (parts.size() == 5) result["branches"].push_back({{"name",parts[0]},{"hash",parts[1]},{"upstream",parts[2]},{"current",parts[3] == "*"},{"remote",parts[4].starts_with("refs/remotes/")}}); }
        lines = std::istringstream(git({"remote"}).output);
        while (std::getline(lines,line)) { const auto name = trim(line); if (name.empty() || name.front() == '-') continue;
            auto url = git({"remote","get-url","--",name}); result["remotes"].push_back({{"name",name},{"url",url.exitCode ? "" : trim(url.output)}}); }
        lines = std::istringstream(git({"stash","list","--format=%gd%x00%H%x00%gs"}).output);
        while (std::getline(lines,line)) { auto parts = fields(trim(line),'\0'); if (parts.size() == 3) result["stashes"].push_back({{"ref",parts[0]},{"hash",parts[1]},{"message",parts[2]}}); }
        auto tracking = git({"rev-list","--left-right","--count","HEAD...@{upstream}"});
        if (!tracking.exitCode) { std::istringstream counts(tracking.output); int ahead = 0, behind = 0; counts >> ahead >> behind; result.update({{"ahead",ahead},{"behind",behind},{"hasUpstream",true}}); }
        return result;
    }
    if (method == "vcs.history") {
        auto result = git({"log","-"+std::to_string(std::clamp(params.value("limit",100),1,500)),"--format=%H%x00%P%x00%an%x00%aI%x00%s","--","."});
        Json commits = Json::array(); std::istringstream lines(result.output); std::string line;
        while (std::getline(lines,line)) { auto parts = fields(trim(line),'\0'); if (parts.size() == 5) commits.push_back({{"hash",parts[0]},{"parents",parts[1]},{"author",parts[2]},{"date",parts[3]},{"subject",parts[4]}}); }
        return {{"commits",commits},{"truncated",result.truncated},{"message",result.exitCode ? result.output : ""}};
    }
    if (method == "vcs.show") return processJson(git({"show","--no-ext-diff","--no-textconv","--no-color",reference(params.at("revision")),"--","."}));
    if (method == "vcs.blame") return processJson(git({"blame","--date=short","--",gitFileArgument(root_,selected,resolve(params.at("path")))}));
    if (method == "vcs.conflicts") {
        rootOnly();
        const auto path = gitFileArgument(root_,selected,resolve(params.at("path")));
        auto unresolved = git({"ls-files","-u","--",path}); if (unresolved.output.empty()) throw std::runtime_error("This file is not an unresolved merge conflict");
        Json versions = Json::object();
        for (const auto &[stage,name] : std::vector<std::pair<int,std::string>>{{1,"base"},{2,"ours"},{3,"theirs"}}) {
            auto content = git({"show",":"+std::to_string(stage)+":"+path});
            if (content.truncated || content.output.size() > 4*1024*1024 || content.output.find('\0') != std::string::npos) throw std::runtime_error("Conflict editor supports UTF-8 text up to 4 MiB per version");
            if (content.exitCode) versions[name] = nullptr; else { (void)Json(content.output).dump(); versions[name] = content.output; }
        }
        auto doc = dispatch("documents.read",{{"path",params.at("path")}});
        return {{"path",pathUtf8(resolve(params.at("path")).lexically_relative(root_))},{"revision",doc.at("revision")},{"working",doc.at("text")},{"versions",versions}};
    }
    if (method == "vcs.resolve") {
        rootOnly(); const auto path = gitFileArgument(root_,selected,resolve(params.at("path")));
        if (git({"ls-files","-u","--",path}).output.empty()) throw std::runtime_error("This file is not an unresolved merge conflict");
        auto doc = dispatch("documents.read",{{"path",params.at("path")}});
        if (params.at("expectedRevision") != doc.at("revision")) throw std::runtime_error("RevisionConflict: conflict buffer changed");
        const auto text = params.value("text",doc.at("text").get<std::string>());
        std::istringstream lines(text); std::string line;
        while (std::getline(lines,line)) if (!params.value("allowMarkers",false) && (line.starts_with("<<<<<<< ") || line.starts_with("=======") || line.starts_with(">>>>>>> "))) throw std::runtime_error("Remove conflict markers before marking the file resolved");
        if (params.contains("text")) doc = dispatch("documents.replace",{{"path",params.at("path")},{"expectedRevision",doc.at("revision")},{"text",text}});
        dispatch("documents.save",{{"path",params.at("path")},{"expectedRevision",doc.at("revision")}});
        return processJson(git({"add","--",path}));
    }
    rootOnly();
    std::vector<std::string> args; bool changesFiles = false;
    if (method == "vcs.branch.create") {
        args = {"branch","--",branch(params.at("name")),reference(params.value("start","HEAD"))};
    } else if (method == "vcs.branch.switch") {
        cleanTree(); args = {"switch"}; if (params.value("track",false)) args.push_back("--track");
        args.insert(args.end(),{"--",branch(params.at("name"))}); changesFiles = true;
    } else if (method == "vcs.branch.rename") args = {"branch","--move","--",branch(params.at("name")),branch(params.at("newName"))};
    else if (method == "vcs.branch.delete") args = {"branch","--delete","--",branch(params.at("name"))};
    else if (method == "vcs.remote.add" || method == "vcs.remote.set") {
        auto url = params.at("url").get<std::string>(); if (url.empty() || url.front() == '-' || url.find('\0') != std::string::npos) throw std::runtime_error("Invalid Git remote URL");
        args = {"remote",method == "vcs.remote.add" ? "add" : "set-url","--",remote(),url};
    } else if (method == "vcs.remote.remove") args = {"remote","remove","--",remote()};
    else if (method == "vcs.fetch") args = {"fetch","--prune","--",remote()};
    else if (method == "vcs.push") {
        auto name = branch(params.value("branch",trim(git({"branch","--show-current"}).output)));
        args = {"push","--porcelain"}; if (params.value("setUpstream",false)) args.push_back("--set-upstream"); args.insert(args.end(),{"--",remote(),name});
    } else if (method == "vcs.pull") {
        cleanTree(); const auto name = branch(params.value("branch",trim(git({"branch","--show-current"}).output)));
        auto fetch = git({"fetch","--",remote(),name},180); if (fetch.exitCode) return processJson(fetch);
        cleanTree(); args = {"merge","--ff-only","FETCH_HEAD"}; changesFiles = true;
    } else if (method == "vcs.merge") { cleanTree(); args = {"merge","--no-edit",reference(params.at("revision"))}; changesFiles = true; }
    else if (method == "vcs.stash.save") {
        cleanBuffers(); args = {"stash","push"}; if (params.value("includeUntracked",false)) args.push_back("--include-untracked");
        args.insert(args.end(),{"-m",params.value("message","CyCode stash")}); changesFiles = true;
    } else if (method == "vcs.stash.apply" || method == "vcs.stash.pop" || method == "vcs.stash.drop") {
        const auto index = params.value("index",0); if (index < 0 || index > 9999) throw std::runtime_error("Invalid stash index");
        if (method != "vcs.stash.drop") { cleanTree(); changesFiles = true; }
        args = {"stash",method.substr(10),"--","stash@{"+std::to_string(index)+"}"};
    } else throw std::runtime_error("Unsupported Git operation");
    auto result = processJson(git(args,method == "vcs.fetch" || method == "vcs.push" || method == "vcs.pull" ? 180 : 30));
    if (changesFiles) refreshGitDocuments(result,gitRoot);
    return result;
}
} // namespace cycode
