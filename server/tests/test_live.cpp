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

json not_a_function(const std::string& name) {
    return {{"ok", false}, {"error", {{"message", "'" + name + "' is not a function"}}}};
}

struct Client {
    std::vector<json> sent;
    Server server;
    explicit Client(uint16_t port, bool live_inlays = false)
        : server(setup(port, live_inlays), [this](const json& m) { sent.push_back(m); }) {
        server.handle(
            {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}, {"params", json::object()}},
            {});
        server.handle(
            {{"jsonrpc", "2.0"},
             {"method", "textDocument/didOpen"},
             {"params",
              {{"textDocument",
                {{"uri", uri_from_path(kDefault)},
                 {"text",
                  "#require \"../packs/game/core.dta\"\n(units (knight (health 100)))\n"}}}}}},
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
        return not_a_function("x");
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
        return not_a_function("x");
    });
    {
        Client c(game.port());
        CHECK(c.request(3, "textDocument/codeAction", c.at_health()).empty());
    }
    binds = true;
    Client c(game.port());
    const json actions = c.request(3, "textDocument/codeAction", c.at_health());
    REQUIRE(actions.size() == 1);
    CHECK(actions[0]["title"] == "Write the live value here");
    const json command = actions[0]["command"];
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
