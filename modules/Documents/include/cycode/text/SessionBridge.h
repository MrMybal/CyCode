#pragma once
#include "cycode/text/DocumentSession.h"
#include "cycode/LocalEndpoint.h"
#include <atomic>
namespace cycode::text {
// Thin text-session adapter for the common native transport and core envelope.
class SessionBridge {
  public:
    SessionBridge(DocumentSession &,const std::filesystem::path &);
    ~SessionBridge();
    SessionBridge(const SessionBridge &)=delete;
    SessionBridge &operator=(const SessionBridge &)=delete;
    const std::filesystem::path &descriptor() const{return descriptor_;}
    // Same status as service.status on this endpoint; no project worker starts.
    // The owner must keep the bridge alive for the duration of the call.
    Json serviceStatus() const;
  private:
    DocumentSession &session_;
    std::filesystem::path descriptor_;
    std::atomic<bool> stopping_{false};
    std::unique_ptr<LocalEndpoint> endpoint_;
};
} // namespace cycode::text
