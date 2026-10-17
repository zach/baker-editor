#include "positions.hpp"
#include "server.hpp"

#include <doctest.h>

#include <set>
#include <vector>

using namespace baker_lsp;
using nlohmann::json;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kDefault = kProject + "/data/default.dta";

struct Client {
    std::vector<json> sent;
    Server server;
    Server::Clock::time_point now{};

    Client() : server(setup(), [this](const json& m) { sent.push_back(m); }) {}
    static Setup setup() {
        Setup s;
        s.layers = {{"default", kDefault, false}};
        s.pack_levels = {"engine", "game", "task"};
        s.reference = kProject + "/reference.json";
        return s;
    }
    void notify(const std::string& method, json params) {
        server.handle({{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}}, now);
    }
    json request(int id, const std::string& method, json params = json::object()) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}},
                      now);
        // The reply; a check the request ran first sends diagnostics and logs too.
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == id) return m;
        FAIL("no reply to " << method);
        return {};
    }
    void open(const std::string& text) {
        notify("textDocument/didOpen", {{"textDocument",
                                         {{"uri", uri_from_path(kDefault)},
                                          {"languageId", "baker"},
                                          {"version", 1},
                                          {"text", text}}}});
    }
    void change(const std::string& text) {
        notify("textDocument/didChange",
               {{"textDocument", {{"uri", uri_from_path(kDefault)}, {"version", 2}}},
                {"contentChanges", json::array({{{"text", text}}})}});
    }
    // The diagnostics the last check published, by URI.
    std::vector<json> published() {
        std::vector<json> out;
        for (const json& m : sent)
            if (m.value("method", "") == "textDocument/publishDiagnostics")
                out.push_back(m["params"]);
        return out;
    }
};

}  // namespace

TEST_CASE("initialize offers full text sync") {
    Client c;
    const json reply = c.request(1, "initialize");
    CHECK(reply["id"] == 1);
    CHECK(reply["result"]["capabilities"]["textDocumentSync"]["change"] == 1);
    CHECK(reply["result"]["capabilities"]["hoverProvider"] == true);
    CHECK(c.request(2, "textDocument/formatting")["error"]["code"] == -32601);
}

TEST_CASE("an edit is checked after the debounce, and a fix clears its diagnostics") {
    Client c;
    c.request(1, "initialize");
    c.notify("initialized", json::object());
    REQUIRE(c.server.due());
    c.server.check_now();
    CHECK(c.published().empty());  // nothing wrong, nothing published

    c.open("(units (knight (health 100) (speed 2.5)))\n");
    c.change("(units (knight (health 100) (speed 2.5))\n");
    REQUIRE(c.server.due());
    CHECK(*c.server.due() == c.now + Server::kDebounce);

    c.sent.clear();
    c.server.check_now();
    CHECK(!c.server.due());
    auto lists = c.published();
    REQUIRE(lists.size() == 1);
    CHECK(lists[0]["uri"] == uri_from_path(kDefault));
    REQUIRE(lists[0]["diagnostics"].size() == 1);
    const json& d = lists[0]["diagnostics"][0];
    CHECK(d["severity"] == 1);
    CHECK(d["source"] == "baker");
    CHECK(d["range"]["start"]["line"] == 0);

    c.change("(units (knight (health 100) (speed 2.5)))\n");
    c.sent.clear();
    c.server.check_now();
    lists = c.published();
    REQUIRE(lists.size() == 1);
    CHECK(lists[0]["diagnostics"].empty());
}

TEST_CASE("each edit pushes the check back; an open brings it forward") {
    Client c;
    c.open("(units)\n");
    CHECK(*c.server.due() == c.now);
    c.now += std::chrono::milliseconds(100);
    c.change("(units )\n");
    CHECK(*c.server.due() == c.now + Server::kDebounce);
    c.now += std::chrono::milliseconds(100);
    c.change("(units  )\n");
    CHECK(*c.server.due() == c.now + Server::kDebounce);
}

TEST_CASE("without a setup, the project file at the workspace's root is the setup") {
    std::vector<json> sent;
    Server server(Setup{}, [&sent](const json& m) { sent.push_back(m); });
    const Server::Clock::time_point now{};
    server.handle({{"jsonrpc", "2.0"},
                   {"id", 1},
                   {"method", "initialize"},
                   {"params", {{"rootUri", uri_from_path(kProject)}}}},
                  now);
    server.handle({{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", json::object()}}, now);
    bool found = false;
    for (const json& m : sent)
        if (m.value("method", "") == "window/logMessage")
            found = found || m["params"]["message"].get<std::string>().find("baker-project.dta") !=
                                 std::string::npos;
    CHECK(found);
    sent.clear();
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params",
          {{"textDocument",
            {{"uri", uri_from_path(kDefault)},
             {"text", "#require \"../packs/game/core.dta\"\n(units (knight (health 0)))\n"}}}}}},
        now);
    server.check_now();
    bool published = false;
    for (const json& m : sent)
        if (m.value("method", "") == "textDocument/publishDiagnostics")
            published = published || !m["params"]["diagnostics"].empty();
    CHECK(published);
}

TEST_CASE("each open file is checked in its nearest project") {
    const std::string data = canonical_path(BAKER_LSP_TEST_DATA);
    const std::string second = data + "/second/data/main.dta";
    std::vector<json> sent;
    Server server(Setup{}, [&sent](const json& m) { sent.push_back(m); });
    const Server::Clock::time_point now{};
    // A workspace holding both games: no project file at its root.
    server.handle({{"jsonrpc", "2.0"},
                   {"id", 1},
                   {"method", "initialize"},
                   {"params", {{"rootUri", uri_from_path(data)}}}},
                  now);
    CHECK(server.projects().empty());
    const auto open = [&](const std::string& path, const std::string& text) {
        server.handle(
            {{"jsonrpc", "2.0"},
             {"method", "textDocument/didOpen"},
             {"params", {{"textDocument", {{"uri", uri_from_path(path)}, {"text", text}}}}}},
            now);
    };
    open(second, "(settings (volume 3)\n");
    open(kDefault, "#require \"../packs/game/core.dta\"\n(units (knight (health 0)))\n");
    CHECK(server.projects() == std::vector<std::string>{kProject + "/baker-project.dta",
                                                        data + "/second/baker-project.dta"});
    sent.clear();
    server.check_now();
    std::set<std::string> uris;
    for (const json& m : sent)
        if (m.value("method", "") == "textDocument/publishDiagnostics" &&
            !m["params"]["diagnostics"].empty())
            uris.insert(m["params"]["uri"].get<std::string>());
    CHECK(uris == std::set<std::string>{uri_from_path(kDefault), uri_from_path(second)});
}

TEST_CASE("no project and no layers: nothing is checked") {
    std::vector<json> sent;
    Server server(Setup{}, [&sent](const json& m) { sent.push_back(m); });
    server.handle({{"jsonrpc", "2.0"},
                   {"id", 1},
                   {"method", "initialize"},
                   {"params", {{"rootUri", "file:///nowhere"}}}},
                  {});
    sent.clear();
    server.check_now();
    CHECK(sent.empty());
}

TEST_CASE("hover and completion answer at an LSP position, checking first if an edit waits") {
    Client c;
    c.request(1, "initialize");
    c.notify("initialized", json::object());
    c.open("#require \"../packs/game/core.dta\"\n(units (knight (health 100)))\n");
    REQUIRE(c.server.due());
    const json at = {{"textDocument", {{"uri", uri_from_path(kDefault)}}},
                     {"position", {{"line", 1}, {"character", 23}}}};  // on 100
    json reply = c.request(2, "textDocument/hover", at);
    CHECK(!c.server.due());
    const std::string value = reply["result"]["contents"]["value"];
    CHECK(value.find("`units/knight/health`") != std::string::npos);
    CHECK(reply["result"]["range"]["start"]["character"] == 23);
    CHECK(reply["result"]["range"]["end"]["character"] == 26);

    c.change("#require \"../packs/game/core.dta\"\n(units (knight (sp\n");
    reply = c.request(3, "textDocument/completion",
                      {{"textDocument", {{"uri", uri_from_path(kDefault)}}},
                       {"position", {{"line", 1}, {"character", 18}}}});
    const json items = reply["result"]["items"];
    REQUIRE(items.size() == 1);
    CHECK(items[0]["label"] == "speed");
    CHECK(items[0]["textEdit"]["range"]["start"]["character"] == 16);
}

TEST_CASE("a reference rebuilt on disk is read again") {
    Client c;
    c.request(1, "initialize");
    c.notify("initialized", json::object());
    c.sent.clear();
    c.notify(
        "workspace/didChangeWatchedFiles",
        {{"changes",
          json::array({{{"uri", uri_from_path(kProject + "/reference.json")}, {"type", 2}}})}});
    bool reread = false;
    for (const json& m : c.sent)
        if (m.value("method", "") == "window/logMessage")
            reread = reread || m["params"]["message"].get<std::string>().find(
                                   "the game's reference") != std::string::npos;
    CHECK(reread);
    CHECK(c.server.due());
}

TEST_CASE("exit after shutdown is a clean exit") {
    Client c;
    c.request(1, "shutdown");
    c.notify("exit", json::object());
    CHECK(c.server.exit_code() == 0);

    Client d;
    d.notify("exit", json::object());
    CHECK(d.server.exit_code() == 1);
}
