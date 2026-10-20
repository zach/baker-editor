#include "net.hpp"

#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define BAKER_LSP_INVALID INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
using socket_t = int;
#define BAKER_LSP_INVALID (-1)
#endif

namespace baker_lsp::net {
namespace {

#ifdef _WIN32
struct Startup {
    Startup() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
};
void startup() {
    static Startup s;
}
void close_socket(socket_t s) {
    closesocket(s);
}
void set_nonblocking(socket_t s, bool on) {
    u_long mode = on ? 1 : 0;
    ioctlsocket(s, FIONBIO, &mode);
}
int wait_for(socket_t s, bool write, int timeout_ms) {
    WSAPOLLFD p{s, static_cast<SHORT>(write ? POLLWRNORM : POLLRDNORM), 0};
    return WSAPoll(&p, 1, timeout_ms);
}
bool in_progress() {
    return WSAGetLastError() == WSAEWOULDBLOCK;
}
#else
void startup() {}
void close_socket(socket_t s) {
    ::close(s);
}
void set_nonblocking(socket_t s, bool on) {
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, on ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
}
int wait_for(socket_t s, bool write, int timeout_ms) {
    pollfd p{s, static_cast<short>(write ? POLLOUT : POLLIN), 0};
    return poll(&p, 1, timeout_ms);
}
bool in_progress() {
    return errno == EINPROGRESS;
}
#endif

sockaddr_in loopback(uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

socket_t as_socket(long long fd) {
    return static_cast<socket_t>(fd);
}

}  // namespace

Connection::~Connection() {
    close();
}

Connection& Connection::operator=(Connection&& o) noexcept {
    if (this != &o) {
        close();
        fd_ = o.fd_;
        o.fd_ = -1;
    }
    return *this;
}

void Connection::close() {
    if (fd_ >= 0) close_socket(as_socket(fd_));
    fd_ = -1;
}

bool Connection::connect(uint16_t port, int timeout_ms) {
    close();
    startup();
    const socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == BAKER_LSP_INVALID) return false;
    set_nonblocking(s, true);
    const sockaddr_in a = loopback(port);
    const int r = ::connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof a);
    bool ok = r == 0;
    if (!ok && in_progress() && wait_for(s, true, timeout_ms) > 0) {
        int error = 0;
        socklen_t len = sizeof error;
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len);
        ok = error == 0;
    }
    if (!ok) {
        close_socket(s);
        return false;
    }
    const int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
    fd_ = static_cast<long long>(s);
    return true;
}

bool Connection::send(std::string_view data) {
    while (open() && !data.empty()) {
        if (wait_for(as_socket(fd_), true, 50) <= 0) break;
        const auto n = ::send(as_socket(fd_), data.data(), static_cast<int>(data.size()), 0);
        if (n <= 0) break;
        data.remove_prefix(static_cast<size_t>(n));
    }
    if (!data.empty()) close();
    return data.empty();
}

bool Connection::read_until(std::string& out, std::string_view terminator, int timeout_ms,
                            size_t count) {
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    char buffer[4096];
    size_t found = 0, searched = 0;
    while (open()) {
        for (size_t at; (at = out.find(terminator, searched)) != std::string::npos;) {
            ++found;
            searched = at + terminator.size();
        }
        if (found >= count && searched == out.size()) return true;
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (left <= 0 || wait_for(as_socket(fd_), false, static_cast<int>(left)) <= 0) break;
        const auto n = ::recv(as_socket(fd_), buffer, sizeof buffer, 0);
        if (n <= 0) break;
        out.append(buffer, static_cast<size_t>(n));
    }
    close();
    return false;
}

Listener::Listener() {
    startup();
    const socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == BAKER_LSP_INVALID) return;
    sockaddr_in a = loopback(0);
    socklen_t len = sizeof a;
    if (::bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0 || ::listen(s, 4) != 0 ||
        getsockname(s, reinterpret_cast<sockaddr*>(&a), &len) != 0) {
        close_socket(s);
        return;
    }
    fd_ = static_cast<long long>(s);
    port_ = ntohs(a.sin_port);
}

Listener::~Listener() {
    if (fd_ >= 0) close_socket(as_socket(fd_));
}

std::optional<Connection> Listener::accept(int timeout_ms) {
    if (fd_ < 0 || wait_for(as_socket(fd_), false, timeout_ms) <= 0) return std::nullopt;
    const socket_t c = ::accept(as_socket(fd_), nullptr, nullptr);
    if (c == BAKER_LSP_INVALID) return std::nullopt;
    set_nonblocking(c, true);
    return Connection(static_cast<long long>(c));
}

}  // namespace baker_lsp::net
