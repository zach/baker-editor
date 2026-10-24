#include "live.hpp"
#include "positions.hpp"
#include "server.hpp"

#include <doctest.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

using namespace baker_lsp;
using nlohmann::json;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kDefault = kProject + "/data/default.dta";

// A running game's console: answers each request line with the reply `answer` gives for its
// command, as one line of baker.reply/1.1 and a blank line.
class MockConsole {
public:
    using Answer = std::function<json(const std::string& command)>;
    explicit MockConsole(Answer answer) : answer_(std::move(answer)) {
        thread_ = std::thread([this] { serve(); });
    }
    ~MockConsole() {
        stop_ = true;
        thread_.join();
    }
    [[nodiscard]] uint16_t port() const { return listener_.port(); }
    // What was asked, in order.
    std::vector<std::string> commands() {
        std::lock_guard lock(mutex_);
        return commands_;
    }

private:
    void serve() {
        while (!stop_) {
            auto c = listener_.accept(20);
            if (!c) continue;
            std::string buffer;
            while (!stop_ && c->open()) {
                const size_t newline = buffer.find('\n');
                if (newline == std::string::npos) {
                    if (!c->read_until(buffer, "\n", 20) && !c->open()) break;
                    continue;
                }
                const std::string line = buffer.substr(0, newline);  // requests may come at once
                buffer.erase(0, newline + 1);
                // (ID {command})
                const size_t space = line.find(' ');
                const int id = std::stoi(line.substr(1, space - 1));
                const std::string command = line.substr(space + 1, line.size() - space - 2);
                {
                    std::lock_guard lock(mutex_);
                    commands_.push_back(command);
                }
                json reply = answer_(command);
                reply["schema"] = "baker.reply/1.1";
                reply["id"] = id;
                c->send(reply.dump() + "\n\n");
            }
        }
    }
    Answer answer_;
    std::mutex mutex_;
    std::vector<std::string> commands_;
    net::Listener listener_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

json explained(const std::string& file, bool live_edit, int line = 5) {
    json chain = json::array({{{"layer", "default"},
                               {"value", 100},
                               {"standing", live_edit ? "shadowed" : "wins"},
                               {"file", file},
                               {"line", line}}});
    if (live_edit) chain.push_back({{"layer", "scratch"}, {"value", 140}, {"standing", "wins"}});
    return {{"ok", true}, {"text", "(...)"}, {"value", chain}};
}

// What the console answers a command nothing binds: {name ...} or name.
json not_a_function(std::string name) {
    if (name.starts_with('{')) name = name.substr(1, name.find_first_of(" }") - 1);
    return {{"ok", false},
            {"error", {{"code", "not found"}, {"message", "'" + name + "' is not a function"}}}};
}

std::vector<std::string> titles(const json& actions) {
    std::vector<std::string> out;
    for (const json& a : actions) out.push_back(a["title"]);
    return out;
}

const std::string kText = "#require \"../packs/game/core.dta\"\n(units (knight (health 100)))\n";

struct Client {
    std::vector<json> sent;
    Server server;
    explicit Client(uint16_t port, bool live_inlays = false, const std::string& text = kText)
        : server(setup(port, live_inlays), [this](const json& m) { sent.push_back(m); }) {
        server.handle(
            {{"jsonrpc", "2.0"},
             {"id", 1},
             {"method", "initialize"},
             {"params",
              {{"capabilities", {{"workspace", {{"inlayHint", {{"refreshSupport", true}}}}}}}}}},
            {});
        server.handle({{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", json::object()}},
                      {});
        server.handle(
            {{"jsonrpc", "2.0"},
             {"method", "textDocument/didOpen"},
             {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}, {"text", text}}}}}},
            {});
        server.check_now();
    }
    static Setup setup(uint16_t port, bool live_inlays) {
        Setup s;
        s.live_inlays = live_inlays;
        s.project = kProject + "/baker-project.dta";
        s.reference = kProject + "/reference.json";
        s.console_port = port;
        return s;
    }
    json request(int id, const std::string& method, json params) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}, {});
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == id) return m["result"];
        return nullptr;
    }
    json at_health() const {
        const json pos = {{"line", 1}, {"character", 23}};
        return {{"textDocument", {{"uri", uri_from_path(kDefault)}}},
                {"position", pos},
                {"range", {{"start", pos}, {"end", pos}}}};
    }
    json at(int line, int character) const {
        const json pos = {{"line", line}, {"character", character}};
        return {{"textDocument", {{"uri", uri_from_path(kDefault)}}},
                {"position", pos},
                {"range", {{"start", pos}, {"end", pos}}},
                {"context", {{"diagnostics", json::array()}}}};
    }
    // Runs the code action titled `title` at `where`; false if it is not offered.
    bool run(const json& where, const std::string& title) {
        for (const json& a : request(3, "textDocument/codeAction", where))
            if (a["title"] == title) {
                request(4, "workspace/executeCommand",
                        {{"command", a["command"]["command"]},
                         {"arguments", a["command"]["arguments"]}});
                return true;
            }
        return false;
    }
    json inlays() {
        return request(5, "textDocument/inlayHint",
                       {{"textDocument", {{"uri", uri_from_path(kDefault)}}},
                        {"range",
                         {{"start", {{"line", 0}, {"character", 0}}},
                          {"end", {{"line", 9}, {"character", 0}}}}}});
    }
    bool told(const std::string& start) const {
        for (const json& m : sent)
            if (m.value("method", "") == "window/showMessage" &&
                m["params"]["message"].get<std::string>().rfind(start, 0) == 0)
                return true;
        return false;
    }
    std::string hover() {
        return request(2, "textDocument/hover", at_health())["contents"]["value"];
    }
};

}  // namespace

TEST_CASE("no game: the console is not waited for, and nothing is live") {
    uint16_t closed = 0;
    {
        net::Listener l;  // a port nothing listens on once it closes
        closed = l.port();
    }
    Live live(closed);
    const auto start = std::chrono::steady_clock::now();
    CHECK(!live.ask("{config.get a}"));
    CHECK(!live.ask("{config.get a}"));  // within kRetry: not tried again
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(200));
    Client c(closed);
    const std::string h = c.hover();
    CHECK(h.find("from the files") != std::string::npos);
    CHECK(h.find("Live") == std::string::npos);
}

TEST_CASE("a running game: hover shows its live chain") {
    MockConsole game([](const std::string& cmd) {
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, true);
        return not_a_function(cmd);
    });
    Client c(game.port());
    const std::string h = c.hover();
    CHECK(h.find("**Live**") != std::string::npos);
    CHECK(h.find("| scratch | `140` |  | wins |") != std::string::npos);
    CHECK(game.commands().front() == "{config.explain units/knight/health}");
}

TEST_CASE("another project's game is not this project's live value") {
    MockConsole game([](const std::string&) { return explained("/elsewhere/default.dta", false); });
    Client c(game.port());
    CHECK(c.hover().find("Live") == std::string::npos);
}

TEST_CASE("write the live value: offered for a live edit when the game can write it back") {
    std::atomic<bool> binds{false};
    MockConsole game([&binds](const std::string& cmd) -> json {
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, true);
        if (cmd == "{config.write_back}")
            return binds ? json{{"ok", false}, {"error", {{"message", "no overload takes 0"}}}}
                         : not_a_function("config.write_back");
        if (cmd.rfind("{config.write_back ", 0) == 0)
            return {{"ok", true}, {"text", "(written (default.dta 2))"}, {"value", nullptr}};
        return not_a_function(cmd);
    });
    {
        Client c(game.port());
        CHECK(titles(c.request(3, "textDocument/codeAction", c.at_health())) ==
              std::vector<std::string>{"Try this value in the game"});
    }
    binds = true;
    Client c(game.port());
    const json actions = c.request(3, "textDocument/codeAction", c.at_health());
    REQUIRE(titles(actions) ==
            std::vector<std::string>{"Try this value in the game", "Write the live value here"});
    const json command = actions[1]["command"];
    const json result =
        c.request(4, "workspace/executeCommand",
                  {{"command", command["command"]}, {"arguments", command["arguments"]}});
    CHECK(result == "(written (default.dta 2))");
    CHECK(game.commands().back() == "{config.write_back units/knight/health}");
    bool told = false;
    for (const json& m : c.sent)
        told = told || (m.value("method", "") == "window/showMessage" &&
                        m["params"]["message"].get<std::string>().find("the game wrote") == 0);
    CHECK(told);
}

TEST_CASE("pipelined requests: every reply, in the commands' order") {
    MockConsole game([](const std::string& cmd) { return json{{"ok", true}, {"value", cmd}}; });
    Live live(game.port());
    const auto replies = live.ask_many({"{a}", "{b}", "{c}"});
    REQUIRE(replies);
    REQUIRE(replies->size() == 3);
    CHECK((*replies)[0]["value"] == "{a}");
    CHECK((*replies)[2]["value"] == "{c}");
    CHECK(live.ask("{d}").value()["value"] == "{d}");  // still in step
}

TEST_CASE("live inlays: the game's value beside the file value a live edit overrides") {
    MockConsole game([](const std::string& cmd) {
        if (cmd == "{config.explain units/knight/health}") return explained(kDefault, true, 2);
        return json{{"ok", false}, {"error", {{"message", "no such value"}}}};
    });
    const json range = {
        {"textDocument", {{"uri", uri_from_path(kDefault)}}},
        {"range",
         {{"start", {{"line", 0}, {"character", 0}}}, {"end", {{"line", 3}, {"character", 0}}}}}};
    Client off(game.port());
    CHECK(off.request(2, "textDocument/inlayHint", range).empty());
    Client on(game.port(), true);
    const json hints = on.request(2, "textDocument/inlayHint", range);
    REQUIRE(hints.size() == 1);
    CHECK(hints[0]["label"] == "live 140");
    CHECK(hints[0]["position"] == json{{"line", 1}, {"character", 27}});  // after (health 100)
}

TEST_CASE("evaluate in the game: the reply after the command until the next edit") {
    MockConsole game([](const std::string& cmd) -> json {
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, false);
        if (cmd == "{game.spawn knight 2}")
            return {{"ok", true}, {"text", "(spawned knight_1 knight_2)\n(more)"}};
        if (cmd == "{game.report}") return {{"ok", true}, {"text", "\"a\\nb\""}, {"value", "a\nb"}};
        if (cmd == "{game.wait 0}")
            return {{"ok", false},
                    {"error",
                     {{"message", "wait needs a positive time"},
                      {"stack",
                       json::array(
                           {{{"function", "game.wait"}, {"file", "default.dta"}, {"line", 3}}})}}}};
        return not_a_function(cmd);
    });
    const std::string text = kText + "{game.spawn knight 2}\n{game.wait 0}\n{game.report}\n";
    Client c(game.port(), false, text);
    CHECK(titles(c.request(3, "textDocument/codeAction", c.at(2, 3))) ==
          std::vector<std::string>{"Evaluate in the game"});
    REQUIRE(c.run(c.at(2, 3), "Evaluate in the game"));
    CHECK(game.commands().back() == "{game.spawn knight 2}");
    bool refreshed = false;
    for (const json& m : c.sent)
        refreshed = refreshed || m.value("method", "") == "workspace/inlayHint/refresh";
    CHECK(refreshed);
    json hints = c.inlays();
    REQUIRE(hints.size() == 1);
    CHECK(hints[0]["label"] == "\u21d2 (spawned knight_1 knight_2) \u2026");
    CHECK(hints[0]["position"] == json{{"line", 2}, {"character", 21}});

    REQUIRE(c.run(c.at(3, 1), "Evaluate in the game"));
    CHECK(c.told("wait needs a positive time\n  in game.wait (default.dta:3)"));
    hints = c.inlays();
    REQUIRE(hints.size() == 2);
    CHECK(hints[1]["label"] == "\u2717 wait needs a positive time");
    REQUIRE(c.run(c.at(4, 1), "Evaluate in the game"));
    CHECK(c.inlays()[2]["label"] == "\u21d2 a \u2026");  // a string's content, not its escapes

    c.server.handle({{"jsonrpc", "2.0"},
                     {"method", "textDocument/didChange"},
                     {"params",
                      {{"textDocument", {{"uri", uri_from_path(kDefault)}}},
                       {"contentChanges", json::array({{{"text", text + "\n"}}})}}}},
                    {});
    CHECK(c.inlays().empty());
}

TEST_CASE("try this value: set live, without saving; revert only where the game can") {
    std::atomic<bool> clears{false};
    MockConsole game([&clears](const std::string& cmd) -> json {
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, true, 2);
        if (cmd.rfind("{config.set ", 0) == 0) return {{"ok", true}, {"text", "120"}};
        if (cmd == "{config.clear_scratch}") return json{{"ok", true}, {"text", "cleared all"}};
        if (cmd == "{config.clear_scratch baker_editor.no_such_value}")
            return clears ? json{{"ok", true}, {"text", "0"}}
                          : not_a_function("config.clear_scratch");
        if (cmd.rfind("{config.clear_scratch ", 0) == 0) return {{"ok", true}, {"text", "1"}};
        return not_a_function(cmd);
    });
    {
        const std::string tuned =
            "#require \"../packs/game/core.dta\"\n(units (knight (health  120 )))\n";
        Client c(game.port(), false, tuned);
        REQUIRE(c.run(c.at(1, 23), "Try this value in the game"));
        CHECK(game.commands().back() == "{config.set units/knight/health 120}");
        const json hints = c.inlays();
        REQUIRE(hints.size() == 1);
        CHECK(hints[0]["label"] == "\u21d2 120");
        CHECK(!c.run(c.at(1, 23), "Revert the live edit"));
    }
    clears = true;
    Client c(game.port());
    REQUIRE(c.run(c.at(1, 23), "Revert the live edit"));
    CHECK(game.commands().back() == "{config.clear_scratch units/knight/health}");
    for (const std::string& cmd : game.commands())
        CHECK(cmd != "{config.clear_scratch}");  // clears all
    CHECK(!c.told("the game did not revert"));
}

TEST_CASE("no game, or another project's: nothing to evaluate or try") {
    uint16_t closed = 0;
    {
        net::Listener l;
        closed = l.port();
    }
    const std::string text = kText + "{game.spawn knight 2}\n";
    {
        Client c(closed, false, text);
        CHECK(c.request(3, "textDocument/codeAction", c.at(2, 3)).empty());
        CHECK(c.request(3, "textDocument/codeAction", c.at_health()).empty());
    }
    MockConsole game([](const std::string&) { return explained("/elsewhere/default.dta", false); });
    Client c(game.port(), false, text);
    CHECK(c.request(3, "textDocument/codeAction", c.at(2, 3)).empty());
    CHECK(c.request(3, "textDocument/codeAction", c.at_health()).empty());
}

namespace {

// The refreshes the server asked the client for since `sent` was cleared.
int refreshes(const std::vector<json>& sent) {
    int n = 0;
    for (const json& m : sent) n += m.value("method", "") == "workspace/inlayHint/refresh";
    return n;
}

}  // namespace

TEST_CASE("live inlays follow the game: refreshed when its epoch moves, and only then") {
    std::atomic<int> epoch{1};
    MockConsole game([&epoch](const std::string& cmd) -> json {
        if (cmd == "{config.epoch}") return {{"ok", true}, {"text", std::to_string(epoch.load())}};
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, false);
        return not_a_function(cmd);
    });
    Client c(game.port(), true);
    const auto now = Server::Clock::now();
    REQUIRE(c.server.due());
    CHECK(*c.server.due() <= now);  // a poll is due, as well as nothing else
    c.sent.clear();
    c.server.tick(now);
    CHECK(refreshes(c.sent) == 1);  // a game came up
    c.sent.clear();
    c.server.tick(now + Server::kPoll);
    CHECK(refreshes(c.sent) == 0);  // nothing changed
    epoch = 2;
    c.server.tick(now + 2 * Server::kPoll);
    CHECK(refreshes(c.sent) == 1);
    c.sent.clear();
    c.server.tick(now + 2 * Server::kPoll + std::chrono::milliseconds(10));
    CHECK(refreshes(c.sent) == 0);  // asked at most once a poll
    bool asked_epoch = false;
    for (const std::string& cmd : game.commands())
        asked_epoch = asked_epoch || cmd == "{config.epoch}";
    CHECK(asked_epoch);
}

TEST_CASE("a game with no epoch is followed by its unsaved edits") {
    std::mutex mutex;
    std::string unsaved = "()";
    MockConsole game([&](const std::string& cmd) -> json {
        std::lock_guard lock(mutex);
        if (cmd == "{config.unsaved}") return {{"ok", true}, {"text", unsaved}};
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, false);
        return not_a_function(cmd);
    });
    Client c(game.port(), true);
    const auto now = Server::Clock::now();
    c.server.tick(now);
    c.sent.clear();
    c.server.tick(now + Server::kPoll);
    CHECK(refreshes(c.sent) == 0);
    {
        std::lock_guard lock(mutex);
        unsaved = "(fighters.kestrel.health)";
    }
    c.server.tick(now + 2 * Server::kPoll);
    CHECK(refreshes(c.sent) == 1);
}

TEST_CASE("each project's game on its own port") {
    MockConsole game([](const std::string& cmd) -> json {
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, true);
        return not_a_function(cmd);
    });
    uint16_t closed = 0;
    {
        net::Listener l;
        closed = l.port();
    }
    std::vector<json> sent;
    Setup s = Client::setup(closed, false);  // the default port: no game
    s.console_ports = {{kProject, game.port()}};
    Server server(s, [&sent](const json& m) { sent.push_back(m); });
    server.handle(
        {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}, {"params", json::object()}}, {});
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}, {"text", kText}}}}}},
        {});
    server.check_now();
    sent.clear();
    const json pos = {{"line", 1}, {"character", 23}};
    server.handle(
        {{"jsonrpc", "2.0"},
         {"id", 2},
         {"method", "textDocument/hover"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}}}, {"position", pos}}}},
        {});
    std::string shown;
    for (const json& m : sent)
        if (m.contains("id") && m["id"] == 2) shown = m["result"]["contents"]["value"];
    CHECK(shown.find("**Live**, the game on 127.0.0.1:" + std::to_string(game.port())) !=
          std::string::npos);
}

TEST_CASE("a game with no epoch is followed by its journal: each live edit, the same path too") {
    std::atomic<int64_t> last{0};  // the newest edit's time; 0: none
    std::vector<std::string> asked;
    std::mutex mutex;
    MockConsole game([&](const std::string& cmd) -> json {
        std::lock_guard lock(mutex);
        if (cmd.rfind("{config.journal", 0) == 0) {
            asked.push_back(cmd);
            const int64_t since = cmd == "{config.journal}" ? 0 : std::stoll(cmd.substr(16));
            json entries = json::array();
            if (last > 0 && last >= since)
                entries.push_back({{"path", "fighters.kestrel.health"}, {"time_ns", last.load()}});
            return {{"ok", true}, {"text", "(...)"}, {"value", entries}};
        }
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, false);
        return not_a_function(cmd);
    });
    Client c(game.port(), true);
    const auto now = Server::Clock::now();
    c.server.tick(now);
    c.sent.clear();
    c.server.tick(now + Server::kPoll);
    CHECK(refreshes(c.sent) == 0);
    last = 100;  // an edit
    c.server.tick(now + 2 * Server::kPoll);
    CHECK(refreshes(c.sent) == 1);
    c.sent.clear();
    c.server.tick(now + 3 * Server::kPoll);
    CHECK(refreshes(c.sent) == 0);
    last = 200;  // the same path, edited again
    c.server.tick(now + 4 * Server::kPoll);
    CHECK(refreshes(c.sent) == 1);
    std::lock_guard lock(mutex);
    CHECK(asked.back() == "{config.journal 101}");  // only what is newer
}

TEST_CASE("the project file's console port, unless --console names one") {
    MockConsole game([](const std::string& cmd) -> json {
        if (cmd.rfind("{config.explain", 0) == 0) return explained(kDefault, true);
        return not_a_function(cmd);
    });
    std::string project;
    REQUIRE(Buffers().read(kProject + "/baker-project.dta", project));
    project.insert(project.find("(layers"), "(console " + std::to_string(game.port()) + ")\n   ");
    uint16_t closed = 0;
    {
        net::Listener l;
        closed = l.port();
    }
    std::vector<json> sent;
    Setup none;  // no command-line setup: the project is found, and read with its buffer
    none.console_port = closed;
    Server server(none, [&sent](const json& m) { sent.push_back(m); });
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params",
          {{"textDocument",
            {{"uri", uri_from_path(kProject + "/baker-project.dta")}, {"text", project}}}}}},
        {});
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}, {"text", kText}}}}}},
        {});
    server.check_now();
    sent.clear();
    const json pos = {{"line", 1}, {"character", 23}};
    server.handle(
        {{"jsonrpc", "2.0"},
         {"id", 2},
         {"method", "textDocument/hover"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}}}, {"position", pos}}}},
        {});
    std::string shown;
    for (const json& m : sent)
        if (m.contains("id") && m["id"] == 2) shown = m["result"]["contents"]["value"];
    CHECK(shown.find("127.0.0.1:" + std::to_string(game.port())) != std::string::npos);
}
