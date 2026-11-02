#include "cycode/Bridge.h"
#include <array>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
using NativeSocket = SOCKET;
static void closeSocket(NativeSocket s) {
    closesocket(s);
}
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
using NativeSocket = int;
static void closeSocket(NativeSocket s) {
    close(s);
}
#endif
namespace cycode {
Bridge::Bridge(Engine &engine, const std::filesystem::path &path) : engine_(engine), descriptor_(path) {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        throw std::runtime_error("Cannot initialize local bridge");
#endif
    auto socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == static_cast<NativeSocket>(-1))
        throw std::runtime_error("Cannot create local bridge");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(socket, 16) != 0) {
        closeSocket(socket);
        throw std::runtime_error("Cannot bind local bridge");
    }
#ifdef _WIN32
    int size = sizeof(address);
    SetHandleInformation(reinterpret_cast<HANDLE>(socket), HANDLE_FLAG_INHERIT, 0);
#else
    socklen_t size = sizeof(address);
#endif
    getsockname(socket, reinterpret_cast<sockaddr *>(&address), &size);
    listener_ = static_cast<std::intptr_t>(socket);
    std::random_device random;
    std::ostringstream token;
    for (int i = 0; i < 8; ++i)
        token << std::hex << std::setw(8) << std::setfill('0') << random();
    token_ = token.str();
    std::filesystem::create_directories(descriptor_.parent_path());
#ifndef _WIN32
    chmod(descriptor_.parent_path().c_str(), 0700);
#endif
    std::ofstream out(descriptor_);
    out << Json({{"protocol", "cycode-native/1"},
                 {"host", "127.0.0.1"},
                 {"port", ntohs(address.sin_port)},
                 {"token", token_},
                 {"workspace", pathUtf8(engine_.workspace())}})
               .dump(2);
    out.close();
#ifndef _WIN32
    chmod(descriptor_.c_str(), 0600);
#endif
    thread_ = std::thread([this] { serve(); });
}
Bridge::~Bridge() {
    stopping_ = true;
    if (thread_.joinable())
        thread_.join();
    if (listener_ != -1)
        closeSocket(static_cast<NativeSocket>(listener_));
    std::error_code ec;
    std::filesystem::remove(descriptor_, ec);
#ifdef _WIN32
    WSACleanup();
#endif
}
void Bridge::serve() {
    while (!stopping_) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(static_cast<NativeSocket>(listener_), &readable);
        timeval timeout{0, 200000};
        if (select(static_cast<int>(listener_ + 1), &readable, nullptr, nullptr, &timeout) <= 0)
            continue;
        auto client = accept(static_cast<NativeSocket>(listener_), nullptr, nullptr);
        if (client == static_cast<NativeSocket>(-1))
            continue;
#ifdef _WIN32
        DWORD millis = 1000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&millis), sizeof(millis));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&millis), sizeof(millis));
        SetHandleInformation(reinterpret_cast<HANDLE>(client), HANDLE_FLAG_INHERIT, 0);
#else
        timeval receiveTimeout{1, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &receiveTimeout, sizeof(receiveTimeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &receiveTimeout, sizeof(receiveTimeout));
#ifdef SO_NOSIGPIPE
        int yes = 1;
        setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
#endif
        Json response;
        std::string request;
        char buffer[8192];
        try {
            while (request.find('\n') == std::string::npos && !stopping_) {
                int count = static_cast<int>(recv(client, buffer, sizeof(buffer), 0));
                if (count <= 0)
                    throw std::runtime_error("Incomplete native request");
                request.append(buffer, count);
                if (request.size() > 8 * 1024 * 1024)
                    throw std::runtime_error("Native request exceeds 8 MiB");
            }
            auto input = Json::parse(request);
            if (input.value("token", std::string{}) != token_)
                throw std::runtime_error("Access denied");
            response = {
                {"result", engine_.dispatch(input.at("method"), input.value("params", Json::object()))}};
        } catch (const std::exception &e) {
            response = {{"error", {{"message", e.what()}}}};
        }
        auto payload = response.dump(-1, ' ', false, Json::error_handler_t::replace) + "\n";
        size_t at = 0;
        while (at < payload.size()) {
#if defined(MSG_NOSIGNAL)
            int sent = static_cast<int>(send(client, payload.data() + at,
                                             static_cast<int>(std::min<size_t>(65536, payload.size() - at)),
                                             MSG_NOSIGNAL));
#else
            int sent =
                static_cast<int>(send(client, payload.data() + at,
                                      static_cast<int>(std::min<size_t>(65536, payload.size() - at)), 0));
#endif
            if (sent <= 0)
                break;
            at += sent;
        }
        closeSocket(client);
    }
}
} // namespace cycode
