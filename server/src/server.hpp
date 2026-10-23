#pragma once
// The language server: documents in, diagnostics out. It holds no thread or stream; main
// feeds it messages and calls check_now() when due(), so tests drive it directly.
//
// It checks one project per project file it finds: each open file belongs to the nearest
// baker-project.dta above it (Baker's find_project), so a workspace can hold several games.
// A setup given on the command line (--project, or --layer and the rest) is the only one.
#include "diagnose.hpp"
#include "live.hpp"
#include "reference.hpp"
#include "setup.hpp"
#include "structure.hpp"
#include "tweaks.hpp"

#include <baker/script.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <deque>
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
    // The most outline symbols, and folds, one reply carries.
    static constexpr size_t kMaxSymbols = 5000;
    // How long an evaluation in the game may take: a person asked for it.
    static constexpr std::chrono::milliseconds kEvaluateTimeout{2000};
    // The most values one inlay request asks the running game about.
    static constexpr size_t kMaxLiveInlays = 200;

    Server(Setup setup, Send send);

    // Handles one message from the client at `now`.
    void handle(const nlohmann::json& message, Clock::time_point now);

    // Messages as they arrive, answered by work() in the order that keeps a person waiting
    // least: a $/cancelRequest answers its queued request as cancelled at once; edits and the
    // requests a person waits on (completion, hover, ...) go first, in order; the requests
    // for a file's views (outline, folds, highlighting, colors, inlay hints, lenses) after
    // them, each answered ContentModified instead if an edit of its file came after it. A
    // check that is due runs before the views, so diagnostics settle while views wait.
    void receive(nlohmann::json message);
    // Handles the next queued message (or a due check before views); false if none waits.
    bool work(Clock::time_point now);
    [[nodiscard]] bool busy() const { return !queue_.empty(); }
    // When a check is due; empty when none is.
    [[nodiscard]] std::optional<Clock::time_point> check_due() const { return due_; }
    // When anything is next due: a check, or asking a running game whether it changed.
    [[nodiscard]] std::optional<Clock::time_point> due() const;
    // Does what is due at `now`.
    void tick(Clock::time_point now);
    // How often a game is asked whether its stack changed, while live inlays are on.
    static constexpr std::chrono::milliseconds kPoll{1000};
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
        std::unique_ptr<GameTweaks> tweaks;  // the reference's, for Baker's check_tweaks
        std::shared_ptr<Loaded> loaded;      // the last check's, for hover and completion
        Kept kept;                           // the stack, reloaded by the next check
        uint16_t port = 0;                   // its game's console
        int game_generation = 0;             // the console connection game_is() last asked
        bool game_ours = false;
    };

    // The project an open file belongs to: the one whose directory is nearest above it.
    Checked* project_of(const std::string& path);
    // The same, with a stack to answer from: the last whole one, even while a check is due,
    // so that hover, completion and the hints do not wait for one (a check runs first only
    // when there is none yet). Null if there is no project, or no stack.
    Checked* loaded_for(const std::string& path);
    nlohmann::json hover(const nlohmann::json& params);
    nlohmann::json complete(const nlohmann::json& params);
    nlohmann::json definition(const nlohmann::json& params);
    nlohmann::json code_actions(const nlohmann::json& params);
    nlohmann::json document_symbols(const nlohmann::json& params);
    nlohmann::json folding_ranges(const nlohmann::json& params);
    nlohmann::json references(const nlohmann::json& params);
    nlohmann::json prepare_rename(const nlohmann::json& params);
    // The rename's workspace edit, or an error message in `error`.
    nlohmann::json rename(const nlohmann::json& params, std::string& error);
    nlohmann::json execute(const nlohmann::json& params);
    nlohmann::json inlay_hints(const nlohmann::json& params);
    nlohmann::json code_lenses(const nlohmann::json& params);
    // textDocument/semanticTokens/full and /range (highlight.hpp), from the kept read.
    nlohmann::json semantic_tokens(const nlohmann::json& params, bool ranged);
    // baker/scenarios: the project's scenarios and objectives, and the binary that runs them.
    nlohmann::json scenarios(const nlohmann::json& params, std::string& error);
    // baker/simResults: a run's results (baker.sim/1), whose failures are published with the
    // project's problems until those scenarios run again.
    void sim_results(const nlohmann::json& params);
    // A swatch on each value Baker calls a color, and the text a picked color writes.
    nlohmann::json document_colors(const nlohmann::json& params);
    nlohmann::json color_presentations(const nlohmann::json& params);
    // baker/merged, baker/layerEffect and baker/layers: views of a document's project.
    nlohmann::json view(const std::string& method, const nlohmann::json& params,
                        std::string& error);
    // Whether the game on the console is this project's: one of its values comes from the
    // project's files (asked once per connection).
    bool game_is(Checked& c);
    // Runs a command in the game for baker.evaluate and baker.try, and keeps its result to
    // show after `at` in `path` until the next edit.
    void evaluate(const std::string& path, size_t at, const std::string& command);
    void refresh_inlays();
    // The running game's chain for `path`, if it answers and the value is the project's.
    std::optional<std::vector<LiveEntry>> live_chain(const Checked& c, const std::string& path);

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

    // The running games' consoles, by port: each project's is its setup's.
    std::map<uint16_t, std::unique_ptr<Live>> lives_;
    Live& live_of(const Checked& c);
    Live& live_at(const std::string& file);  // the project's that holds `file`, else the default
    // While live inlays are on: when each game is next asked whether its stack changed, and
    // what it answered last.
    Clock::time_point next_poll_{};
    std::map<uint16_t, std::string> stamps_;
    void poll(Clock::time_point now);
    Send send_;
    std::deque<nlohmann::json> queue_;  // received, not yet handled (receive, work)
    Buffers buffers_;
    // An open file's text, read (structure.hpp): read again only when its text changed.
    std::shared_ptr<const FileRead> read_of(const std::string& path);
    std::map<std::string, std::shared_ptr<const FileRead>> reads_;
    std::map<std::string, std::string> uris_;  // canonical path -> the client's URI
    std::set<std::string> published_;          // URIs that last got a non-empty list
    // Results of evaluating in the game, by file, shown until its next edit.
    struct Evaluated {
        size_t at = 0;  // the byte the result follows
        std::string label, tooltip;
    };
    std::map<std::string, std::vector<Evaluated>> evaluated_;
    // What the last run of each scenario found: project file -> scenario -> problems.
    std::map<std::string, std::map<std::string, std::vector<Problem>>> sim_;
    bool refresh_inlays_ = false;  // the client takes workspace/inlayHint/refresh
    int next_request_ = 1;         // the id of our next request to the client
    std::optional<Clock::time_point> due_;
    bool shutdown_ = false;
    std::optional<int> exit_code_;
};

}  // namespace baker_lsp
