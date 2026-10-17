#pragma once
// The language server: documents in, diagnostics out. It holds no thread or stream; main
// feeds it messages and calls check_now() when due(), so tests drive it directly.
//
// It checks one project per project file it finds: each open file belongs to the nearest
// baker-project.dta above it (Baker's find_project), so a workspace can hold several games.
// A setup given on the command line (--project, or --layer and the rest) is the only one.
#include "diagnose.hpp"
#include "reference.hpp"
#include "setup.hpp"

#include <baker/script.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

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
    // Checks every project and publishes what changed.
    void check_now();
    // After `exit`: the process's status (0 if `shutdown` came first).
    [[nodiscard]] std::optional<int> exit_code() const { return exit_code_; }
    // The project files being checked, canonical ("" for a command-line setup).
    [[nodiscard]] std::vector<std::string> projects() const;

private:
    // A project and the game's vocabulary for it.
    struct Checked {
        Setup setup;
        std::unique_ptr<baker::Context> ctx = std::make_unique<baker::Context>();
        bool have_reference = false;
        Reference reference;
        std::shared_ptr<Loaded> loaded;  // the last check's, for hover and completion
    };

    // The project an open file belongs to: the one whose directory is nearest above it.
    Checked* project_of(const std::string& path);
    nlohmann::json hover(const nlohmann::json& params);
    nlohmann::json complete(const nlohmann::json& params);
    nlohmann::json definition(const nlohmann::json& params);

    void request(const nlohmann::json& message);
    void notification(const nlohmann::json& message, Clock::time_point now);
    // Starts checking a project: reads its reference, from --reference or the project file.
    void add(Setup setup);
    // The project governing `path` (a file or a directory), added if it is new.
    void find_project(const std::string& path);
    void schedule(Clock::time_point when, bool push_back = false);
    void log(int type, const std::string& text);
    std::string uri_for(const std::string& path) const;

    Setup given_;         // the command line's: --reference, --skip, and maybe the project
    bool fixed_ = false;  // the command line named the project or the layers
    std::map<std::string, Checked> projects_;
    bool initialized_ = false;
    std::vector<std::pair<int, std::string>> pending_log_;  // until `initialized`

    Send send_;
    Buffers buffers_;
    std::map<std::string, std::string> uris_;  // canonical path -> the client's URI
    std::set<std::string> published_;          // URIs that last got a non-empty list
    std::optional<Clock::time_point> due_;
    bool shutdown_ = false;
    std::optional<int> exit_code_;
};

}  // namespace baker_lsp
