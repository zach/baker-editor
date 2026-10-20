#pragma once
// A running game, through its loopback console (baker.reply/1.1): the values it holds now,
// for hover, and its own write-back. The server must not hang or claim to be live without
// a game, so every request waits at most kTimeout, and while no game answers, connecting is
// tried at most once every kRetry.
#include "net.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace baker_lsp {

// One contributor to a value in the running game (config.explain).
struct LiveEntry {
    std::string layer;  // a layer's name, or "scratch" for a live edit not yet written
    std::string value;  // as JSON, for display
    std::string standing;
    std::string file;
    int line = 0;
};

class Live {
public:
    static constexpr std::chrono::milliseconds kTimeout{50};
    static constexpr std::chrono::milliseconds kRetry{1000};

    explicit Live(uint16_t port) : port_(port) {}

    // The reply to {command}, or empty if no game answers in time. On a reply the game is
    // connected; on none it is not.
    std::optional<nlohmann::json> ask(const std::string& command);
    // The replies to many commands, sent at once and waited for together (kTimeout in all),
    // in the commands' order; empty if no game answers them all in time.
    std::optional<std::vector<nlohmann::json>> ask_many(const std::vector<std::string>& commands);
    [[nodiscard]] bool connected() const { return connection_.open(); }
    [[nodiscard]] uint16_t port() const { return port_; }

    // The running game's layer chain for `path`, bottom to top, or empty if no game answers
    // or it has no such value.
    std::optional<std::vector<LiveEntry>> explain(const std::string& path);
    // The chains of many paths at once; an empty chain where the game has no such value.
    std::optional<std::vector<std::vector<LiveEntry>>> explain_many(
        const std::vector<std::string>& paths);
    // Whether the game binds config.write_back (asked once per connection).
    bool can_write_back();

private:
    // Connected, or connecting if a try is due.
    bool ready();

    uint16_t port_;
    net::Connection connection_;
    std::chrono::steady_clock::time_point next_try_{};
    int next_id_ = 1;
    std::optional<bool> write_back_;
};

}  // namespace baker_lsp
