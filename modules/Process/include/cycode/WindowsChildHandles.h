#pragma once
#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <vector>

namespace cycode {
// Explicit inheritance prevents concurrent tool launches from leaking other
// children's pipes or unrelated inheritable application handles. No fallback
// to broad inheritance is allowed if the Windows attribute setup fails.
// https://learn.microsoft.com/windows/win32/procthread/inheritance
class WindowsChildHandles {
  public:
    explicit WindowsChildHandles(std::initializer_list<HANDLE> handles) {
        for(const auto handle:handles) {
            DWORD flags=0;
            if(!handle || handle==INVALID_HANDLE_VALUE || !GetHandleInformation(handle,&flags)) {
                error_=ERROR_INVALID_HANDLE;return;
            }
            if(!(flags&HANDLE_FLAG_INHERIT)){error_=ERROR_INVALID_PARAMETER;return;}
            if(std::find(handles_.begin(),handles_.end(),handle)==handles_.end())handles_.push_back(handle);
        }
        if(handles_.empty()){error_=ERROR_INVALID_PARAMETER;return;}
        SIZE_T bytes=0;
        InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
        if(!bytes){error_=GetLastError();return;}
        storage_.resize((bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes)){error_=GetLastError();return;}
        attributes_=attributes;
        if(!UpdateProcThreadAttribute(attributes_,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles_.data(),
                                      handles_.size()*sizeof(HANDLE),nullptr,nullptr)) {
            error_=GetLastError();DeleteProcThreadAttributeList(attributes_);attributes_=nullptr;
        }
    }
    ~WindowsChildHandles(){if(attributes_)DeleteProcThreadAttributeList(attributes_);}
    WindowsChildHandles(const WindowsChildHandles &)=delete;
    WindowsChildHandles &operator=(const WindowsChildHandles &)=delete;
    LPPROC_THREAD_ATTRIBUTE_LIST get() const {return attributes_;}
    DWORD error() const {return error_;}
  private:
    std::vector<HANDLE> handles_;
    std::vector<std::max_align_t> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes_=nullptr;
    DWORD error_=ERROR_SUCCESS;
};
} // namespace cycode
#endif
