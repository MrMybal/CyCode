#include "cycode/LocalEndpoint.h"
#include "cycode/TextFile.h"
#include "EndpointSecurity.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
using Socket=SOCKET;
static constexpr Socket invalidSocket=INVALID_SOCKET;
static void closeSocket(Socket s){closesocket(s);}
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket=int;
static constexpr Socket invalidSocket=-1;
static void closeSocket(Socket s){::close(s);}
#endif
namespace cycode {
namespace {
struct OwnedSocket {
    Socket value=invalidSocket;
    ~OwnedSocket(){if(value!=invalidSocket)closeSocket(value);}
    Socket release(){const auto s=value;value=invalidSocket;return s;}
};
void preventInheritance(Socket socket) {
#ifdef _WIN32
    if(!SetHandleInformation(reinterpret_cast<HANDLE>(socket),HANDLE_FLAG_INHERIT,0))
        throw std::runtime_error("Cannot protect native socket inheritance");
#else
    if(socket>=FD_SETSIZE || fcntl(socket,F_SETFD,FD_CLOEXEC)<0)
        throw std::runtime_error("Cannot protect native socket inheritance");
#endif
}
}
struct LocalEndpoint::Impl {
    Options options;
    std::filesystem::path descriptor;
    std::string token,published;
    std::atomic<bool> stopping{false};
    std::thread thread;
    std::mutex stopMutex;
#ifdef _WIN32
    struct Winsock {
        Winsock(){WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Cannot initialize local endpoint");}
        ~Winsock(){WSACleanup();}
    };
    Winsock winsock;
#endif
    OwnedSocket listener;
    bool ownsDescriptor=false;
    Impl(const std::filesystem::path &path,Options input):options(std::move(input)),descriptor(path) {
        if(!options.respond || !options.descriptor.is_object())throw std::invalid_argument("Native endpoint needs a descriptor and handler");
        listener.value=::socket(AF_INET,SOCK_STREAM,0);
        if(listener.value==invalidSocket)throw std::runtime_error("Cannot create local endpoint");
        preventInheritance(listener.value);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(::bind(listener.value,reinterpret_cast<sockaddr *>(&address),sizeof(address)) || ::listen(listener.value,16))
            throw std::runtime_error("Cannot bind local endpoint");
#ifdef _WIN32
        int size=sizeof(address);
#else
        socklen_t size=sizeof(address);
#endif
        if(getsockname(listener.value,reinterpret_cast<sockaddr *>(&address),&size))throw std::runtime_error("Cannot inspect local endpoint");
        token=detail::nativeEndpointSecret();
        auto value=options.descriptor;value["host"]="127.0.0.1";value["port"]=ntohs(address.sin_port);value["token"]=token;
        published=value.dump(2)+"\n";
    }
    ~Impl(){stop();}
    void start(){
        if(!descriptor.parent_path().empty())std::filesystem::create_directories(descriptor.parent_path());
        std::optional<std::string> baseline;
        if(std::filesystem::exists(descriptor))baseline=readTextFileBytes(descriptor);
        detail::publishNativeDescriptor(descriptor,published,baseline);ownsDescriptor=true;
        thread=std::thread([this]{serve();});
    }
    void stop() noexcept {
        std::lock_guard lock(stopMutex);
        stopping=true;
        if(thread.joinable())thread.join();
        if(listener.value!=invalidSocket)closeSocket(listener.release());
        // Another instance can have published this path since our startup.
        // Only retire the descriptor that this endpoint actually published.
        try {if(ownsDescriptor && readTextFileBytes(descriptor)==published){std::error_code ec;std::filesystem::remove(descriptor,ec);}}catch(...){}
    }
    Json failure(const std::string &message) const {
        if(options.error)return options.error(message);
        return {{"error",{{"code","TransportError"},{"message",message}}}};
    }
    void serveClient(Socket socket,bool controlOnly) noexcept {
        OwnedSocket client{socket};
        try {
            preventInheritance(socket);
#ifdef _WIN32
            DWORD millis=1000;
            setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char *>(&millis),sizeof(millis));
            setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<char *>(&millis),sizeof(millis));
#else
            timeval timeout{1,0};
            setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
            setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
#ifdef SO_NOSIGPIPE
            int yes=1;setsockopt(socket,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
#endif
            Json response;
            try {
                std::string request;char buffer[8192];
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
                while(request.find('\n')==std::string::npos) {
                    if(stopping)throw std::runtime_error("Native endpoint is stopping");
                    if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Native request read timed out");
                    const auto count=recv(socket,buffer,sizeof(buffer),0);
                    if(count<=0)throw std::runtime_error("Incomplete native request");
                    request.append(buffer,static_cast<size_t>(count));
                    if(request.size()>maximumRequestBytes)throw std::runtime_error("Native request exceeds 8 MiB");
                }
                const auto input=Json::parse(request);
                if(!input.is_object())throw std::runtime_error("Expected a native request object");
                if(!detail::nativeEndpointSecretMatches(input.value("token",std::string{}),token))throw std::runtime_error("Access denied");
                const auto method=input.value("method",std::string{});
                if(controlOnly && (!options.control || !options.control(method)))throw std::runtime_error("Native connection capacity reached; retry later");
                response=options.respond(input);
            } catch(const std::exception &error){response=failure(error.what());}
            const auto payload=encodeNativeResponse(response);
            const auto sendDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            for(size_t at=0;at<payload.size() && !stopping;) {
                if(std::chrono::steady_clock::now()>sendDeadline)break;
#ifdef MSG_NOSIGNAL
                const int flags=MSG_NOSIGNAL;
#else
                const int flags=0;
#endif
                const auto sent=send(socket,payload.data()+at,static_cast<int>(std::min<size_t>(65536,payload.size()-at)),flags);
                if(sent<=0)break;
                at+=static_cast<size_t>(sent);
            }
        }catch(...){} // Client and socket owners still unwind on dispatch/allocation errors.
    }
    void serve() noexcept {
        std::vector<std::future<void>> clients;
        try {
            clients.reserve(maximumClients);
            while(!stopping) {
                std::erase_if(clients,[](auto &client){if(client.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;try{client.get();}catch(...){}return true;});
                fd_set readable;FD_ZERO(&readable);FD_SET(listener.value,&readable);timeval timeout{0,100000};
                if(select(static_cast<int>(listener.value+1),&readable,nullptr,nullptr,&timeout)<=0)continue;
                const auto client=::accept(listener.value,nullptr,nullptr);
                if(client==invalidSocket)continue;
                if(clients.size()>=maximumClients){serveClient(client,true);continue;}
                try{clients.push_back(std::async(std::launch::async,[this,client]{serveClient(client,false);}));}
                catch(...){closeSocket(client);}
            }
        }catch(...) {stopping=true;}
        for(auto &client:clients)try{client.get();}catch(...){}
    }
};
LocalEndpoint::LocalEndpoint(const std::filesystem::path &descriptor,Options options):impl_(std::make_unique<Impl>(descriptor,std::move(options))) {impl_->start();}
LocalEndpoint::~LocalEndpoint()=default;
void LocalEndpoint::stop() noexcept {if(impl_)impl_->stop();}
} // namespace cycode
