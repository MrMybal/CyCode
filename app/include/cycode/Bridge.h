#pragma once
#include "cycode/CoreService.h"
#include "cycode/LocalEndpoint.h"
namespace cycode {
// Core protocol adapter. LocalEndpoint owns sockets and joins handlers before
// the borrowed Engine or this adapter's CoreService can be destroyed.
class Bridge {
  public:
    Bridge(Engine &,const std::filesystem::path &descriptor);
    ~Bridge();
    Bridge(const Bridge &)=delete;
    Bridge &operator=(const Bridge &)=delete;
    const std::filesystem::path &descriptor() const {return descriptor_;}
    // Read the same service instance exposed by this bridge's endpoint.
    // The owner must keep the bridge alive for the duration of the call.
    Json serviceStatus() const;
  private:
    CoreService service_;
    std::filesystem::path descriptor_;
    std::unique_ptr<LocalEndpoint> endpoint_;
};
} // namespace cycode
