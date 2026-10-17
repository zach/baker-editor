#pragma once
// TCP on the loopback interface, with timeouts: the client side for a running game's
// console, and a listener for tests that stand in for one.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace baker_lsp::net {

class Connection {
public:
    Connection() = default;
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }
    Connection& operator=(Connection&& o) noexcept;

    // Connects to 127.0.0.1:`port`, waiting at most `timeout_ms`.
    bool connect(uint16_t port, int timeout_ms);
    [[nodiscard]] bool open() const { return fd_ >= 0; }
    void close();

    bool send(std::string_view data);
    // Reads until `out` ends with `terminator`, waiting at most `timeout_ms` in all; false
    // (and the connection closed) on a timeout or when the peer closes.
    bool read_until(std::string& out, std::string_view terminator, int timeout_ms);

private:
    friend class Listener;
    explicit Connection(long long fd) : fd_(fd) {}
    long long fd_ = -1;
};

// Listens on 127.0.0.1, on a port the system picks.
class Listener {
public:
    Listener();
    ~Listener();
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    [[nodiscard]] uint16_t port() const { return port_; }
    // A client, if one connects within `timeout_ms`.
    std::optional<Connection> accept(int timeout_ms);

private:
    long long fd_ = -1;
    uint16_t port_ = 0;
};

}  // namespace baker_lsp::net
