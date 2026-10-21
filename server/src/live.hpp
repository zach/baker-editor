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
    std::optional<nlohmann::json> ask(const std::string& command,
                                      std::chrono::milliseconds timeout = kTimeout);
    // The replies to many commands, sent at once and waited for together (kTimeout in all),
    // in the commands' order; empty if no game answers them all in time.
    std::optional<std::vector<nlohmann::json>> ask_many(const std::vector<std::string>& commands);
    [[nodiscard]] bool connected() const { return connection_.open(); }
    // Connected, or connecting now if a try is due (at most once every kRetry).
    bool reachable() { return ready(); }
    // Counts connections: what was learned about the game holds while it is unchanged.
    [[nodiscard]] int generation() const { return generation_; }
    [[nodiscard]] uint16_t port() const { return port_; }

    // The running game's layer chain for `path`, bottom to top, or empty if no game answers
    // or it has no such value.
    std::optional<std::vector<LiveEntry>> explain(const std::string& path);
    // The chains of many paths at once; an empty chain where the game has no such value.
    std::optional<std::vector<std::vector<LiveEntry>>> explain_many(
        const std::vector<std::string>& paths);
    // Whether the game binds config.write_back (asked once per connection).
    bool can_write_back();
    // Whether the game binds kClear, which drops one path's live edit (asked once per
    // connection).
    bool can_clear();
    static constexpr const char* kClear = "config.clear";

    // What the game's stack is now, to compare with the last time: its epoch, where the game
    // binds kEpoch; else the time of its last live edit, from {config.journal since}, which
    // answers only what is newer; else its unsaved paths, as text. Which, is asked once per
    // connection. Empty if no game answers.
    std::optional<std::string> stamp();
    static constexpr const char* kEpoch = "config.epoch";

private:
    bool ready();
    // Whether the game binds `function`, asked by calling it as `{function args}`, which must
    // change nothing whatever it does; empty if no game answers.
    std::optional<bool> binds(const char* function, const char* args = "");

    uint16_t port_;
    net::Connection connection_;
    std::chrono::steady_clock::time_point next_try_{};
    int next_id_ = 1;
    std::optional<bool> write_back_;
    std::optional<bool> clear_;
    std::optional<bool> epoch_, journal_;
    int64_t journal_ns_ = -1;  // the newest live edit's time seen; -1 before the first ask
    int generation_ = 0;
};

}  // namespace baker_lsp
