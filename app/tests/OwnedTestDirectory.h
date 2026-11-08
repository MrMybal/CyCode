#pragma once
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

// Explicit cleanup only: failed acceptance cases retain their owned evidence.
class OwnedTestDirectory {
    std::filesystem::path parent_, path_;
public:
    explicit OwnedTestDirectory(const std::string &prefix)
        : parent_(std::filesystem::canonical(std::filesystem::temp_directory_path())),
          path_(parent_/(prefix+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        if (prefix.empty() || prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=std::string::npos ||
            path_.parent_path()!=parent_ || !std::filesystem::create_directory(path_))
            throw std::runtime_error("Cannot create a new owned acceptance directory");
    }
    OwnedTestDirectory(const OwnedTestDirectory &)=delete;
    OwnedTestDirectory &operator=(const OwnedTestDirectory &)=delete;
    const std::filesystem::path &path() const {return path_;}
    void clean() const {
        namespace fs=std::filesystem;
        if (path_==parent_ || path_.parent_path()!=parent_ || fs::canonical(parent_)!=parent_ ||
            fs::is_symlink(fs::symlink_status(path_)) || fs::canonical(path_)!=path_)
            throw std::runtime_error("Acceptance cleanup directory was redirected");
        auto target=path_;
#ifdef _WIN32
        auto native=target.make_preferred().native();
        if (!native.starts_with(L"\\\\?\\"))
            native=native.starts_with(L"\\\\")?L"\\\\?\\UNC\\"+native.substr(2):L"\\\\?\\"+native;
        target=fs::path(native);
#endif
        try {
            fs::remove_all(target);
        } catch (const fs::filesystem_error &error) {
            std::string detail=error.what();
            std::error_code ec;
            size_t count=0;
            for (fs::recursive_directory_iterator i(target,fs::directory_options::skip_permission_denied,ec),end;
                 !ec && i!=end && count<64;i.increment(ec),++count) {
                const auto name=i->path().lexically_relative(target).generic_u8string();
                detail+="\nRemaining: "+std::string(name.begin(),name.end());
            }
            if (ec) detail+="\nInventory: "+ec.message();
            throw std::runtime_error(detail);
        }
    }
};
