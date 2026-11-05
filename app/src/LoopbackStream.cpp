#include "cycode/LoopbackStream.h"
#include <algorithm>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace cycode {
struct LoopbackStream::Impl {
#ifdef _WIN32
    SOCKET socket = INVALID_SOCKET;
    bool winsock = false;
    Impl() {
        WSADATA data{};
        winsock = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    ~Impl() {
        if (socket != INVALID_SOCKET)
            closesocket(socket);
        if (winsock)
            WSACleanup();
    }
    bool valid() const {
        return socket != INVALID_SOCKET;
    }
    static bool pending() {
        const auto e = WSAGetLastError();
        return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
    }
#else
    int socket = -1;
    ~Impl() {
        if (socket >= 0)
            close(socket);
    }
    bool valid() const {
        return socket >= 0;
    }
    static bool pending() {
        return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EINTR;
    }
#endif
};
LoopbackStream::LoopbackStream() : impl_(std::make_unique<Impl>()) {}
LoopbackStream::~LoopbackStream() = default;
bool LoopbackStream::connect(std::uint16_t port) {
    if (!port || impl_->valid())
        return false;
#ifdef _WIN32
    if (!impl_->winsock)
        return false;
#endif
    impl_->socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!impl_->valid())
        return false;
#ifdef _WIN32
    u_long nonblocking = 1;
    if (ioctlsocket(impl_->socket, FIONBIO, &nonblocking))
        return false;
#else
    const auto flags = fcntl(impl_->socket, F_GETFL, 0);
    if (flags < 0 || fcntl(impl_->socket, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;
    if (impl_->socket >= FD_SETSIZE)
        return false;
#ifdef SO_NOSIGPIPE
    int enabled = 1;
    if (setsockopt(impl_->socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)))
        return false;
#endif
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::connect(impl_->socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0)
        return true;
    if (!Impl::pending())
        return false;
    fd_set writes, errors;
    FD_ZERO(&writes);
    FD_ZERO(&errors);
    FD_SET(impl_->socket, &writes);
    FD_SET(impl_->socket, &errors);
    timeval timeout{2, 0};
    if (select(static_cast<int>(impl_->socket + 1), nullptr, &writes, &errors, &timeout) <= 0)
        return false;
    int error = 0;
#ifdef _WIN32
    int size = sizeof(error);
    if (getsockopt(impl_->socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &size))
        return false;
#else
    socklen_t size = sizeof(error);
    if (getsockopt(impl_->socket, SOL_SOCKET, SO_ERROR, &error, &size))
        return false;
#endif
    return error == 0 && FD_ISSET(impl_->socket, &writes) && !FD_ISSET(impl_->socket, &errors);
}
std::ptrdiff_t LoopbackStream::read(char *data, std::size_t size) {
    const auto count = recv(impl_->socket, data, static_cast<int>(std::min<std::size_t>(size, 65536)), 0);
    if (count > 0)
        return count;
    return count < 0 && Impl::pending() ? 0 : -1;
}
std::ptrdiff_t LoopbackStream::write(const char *data, std::size_t size) {
#ifdef MSG_NOSIGNAL
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    const auto count = send(impl_->socket, data, static_cast<int>(std::min<std::size_t>(size, 65536)), flags);
    if (count > 0)
        return count;
    return count < 0 && Impl::pending() ? 0 : -1;
}
void LoopbackStream::shutdown() {
    if (!impl_->valid())
        return;
#ifdef _WIN32
    ::shutdown(impl_->socket, SD_BOTH);
#else
    ::shutdown(impl_->socket, SHUT_RDWR);
#endif
}
} // namespace cycode
