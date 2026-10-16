#pragma once
// The language server: documents in, diagnostics out. It holds no thread or stream; main
// feeds it messages and calls check_now() when due(), so tests drive it directly.
#include "diagnose.hpp"
#include "setup.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace baker_lsp {

class Server {
public:
    using Clock = std::chrono::steady_clock;
    using Send = std::function<void(const nlohmann::json& message)>;

    // How long after the last edit the project is checked again (plan B7).
    static constexpr std::chrono::milliseconds kDebounce{150};

    Server(Setup setup, Send send);

    // Handles one message from the client at `now`.
    void handle(const nlohmann::json& message, Clock::time_point now);
    // When a check is due; empty when none is.
    [[nodiscard]] std::optional<Clock::time_point> due() const { return due_; }
    // Checks the project and publishes what changed.
    void check_now();
    // After `exit`: the process's status (0 if `shutdown` came first).
    [[nodiscard]] std::optional<int> exit_code() const { return exit_code_; }

private:
    void request(const nlohmann::json& message);
    // Without a project or layers, the project file at the root of the workspace.
    void find_project(const nlohmann::json& params);
    void notification(const nlohmann::json& message, Clock::time_point now);
    void schedule(Clock::time_point when, bool push_back = false);
    void log(int type, const std::string& text);
    std::string uri_for(const std::string& path) const;

    Setup setup_;
    Send send_;
    baker::Context ctx_;
    bool have_reference_ = false;
    std::vector<std::string> startup_log_;

    Buffers buffers_;
    std::map<std::string, std::string> uris_;  // canonical path -> the client's URI
    std::set<std::string> published_;          // URIs that last got a non-empty list
    std::optional<Clock::time_point> due_;
    bool shutdown_ = false;
    std::optional<int> exit_code_;
};

}  // namespace baker_lsp
