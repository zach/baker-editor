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

TEST_CASE("which layer wins: a hint on both values, and the shadowed one faded") {
    const std::string local = kProject + "/data/local.dta";
    std::vector<json> sent;
    Server server(Setup{}, [&sent](const json& m) { sent.push_back(m); });
    const Server::Clock::time_point now{};
    server.handle(
        {{"jsonrpc", "2.0"},
         {"id", 1},
         {"method", "initialize"},
         {"params",
          {{"rootUri", uri_from_path(kProject)},
           {"capabilities", {{"workspace", {{"inlayHint", {{"refreshSupport", true}}}}}}}}}},
        now);
    server.handle({{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", json::object()}}, now);
    const auto open = [&](const std::string& path, const std::string& text) {
        server.handle(
            {{"jsonrpc", "2.0"},
             {"method", "textDocument/didOpen"},
             {"params", {{"textDocument", {{"uri", uri_from_path(path)}, {"text", text}}}}}},
            now);
    };
    const auto hints = [&](const std::string& path) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"},
                       {"id", 7},
                       {"method", "textDocument/inlayHint"},
                       {"params",
                        {{"textDocument", {{"uri", uri_from_path(path)}}},
                         {"range",
                          {{"start", {{"line", 0}, {"character", 0}}},
                           {"end", {{"line", 99}, {"character", 0}}}}}}}},
                      now);
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == 7) return m["result"];
        return json();
    };
    std::string text;
    REQUIRE(Buffers().read(kDefault, text));
    open(kDefault, text);
    open(local, "; tuned\n(units (knight (health 50)))\n");
    sent.clear();
    server.check_now();
    json shadowed;
    for (const json& m : sent)
        if (m.value("method", "") == "textDocument/publishDiagnostics" &&
            m["params"]["uri"] == uri_from_path(kDefault))
            for (const json& d : m["params"]["diagnostics"])
                if (d["code"] == "shadowed") shadowed = d;  // underlines the (health 100) array
    REQUIRE(shadowed.is_object());
    CHECK(shadowed["severity"] == 4);
    CHECK(shadowed["tags"] == json::array({1}));
    CHECK(shadowed["range"]["start"] == json{{"line", 4}, {"character", 11}});  // health
    bool refreshed = false;
    for (const json& m : sent)
        refreshed = refreshed || m.value("method", "") == "workspace/inlayHint/refresh";
    CHECK(refreshed);

    const json below = hints(kDefault);
    REQUIRE(below.size() == 1);
    CHECK(below[0]["label"] == "\u2190 local.dta:2 (50)");
    CHECK(below[0]["position"] == json{{"line", 4}, {"character", 23}});  // after (health 100)
    const json above = hints(local);
    REQUIRE(above.size() == 1);
    CHECK(above[0]["label"] == "over default.dta:5 (100)");
    CHECK(above[0]["position"] == json{{"line", 1}, {"character", 26}});

    sent.clear();
    server.check_now();  // nothing moved: no refresh
    for (const json& m : sent) CHECK(m.value("method", "") != "workspace/inlayHint/refresh");

    // While the file does not read, the hint and the faded value stay on their line: three
    // lines put in above it, and a bracket left open at the end.
    const auto change = [&](const std::string& path, const std::string& to) {
        server.handle({{"jsonrpc", "2.0"},
                       {"method", "textDocument/didChange"},
                       {"params",
                        {{"textDocument", {{"uri", uri_from_path(path)}, {"version", 2}}},
                         {"contentChanges", json::array({{{"text", to}}})}}}},
                      now);
    };
    bool broken = false;  // the last check found a syntax error in default.dta
    const auto shadowed_line = [&] {
        sent.clear();
        server.check_now();
        int line = -1;
        broken = false;
        for (const json& m : sent)
            if (m.value("method", "") == "textDocument/publishDiagnostics" &&
                m["params"]["uri"] == uri_from_path(kDefault))
                for (const json& d : m["params"]["diagnostics"]) {
                    if (d["code"] == "shadowed") line = d["range"]["start"]["line"];
                    if (d["code"] == "syntax") broken = true;
                }
        return line;
    };
    const size_t units = text.find("(units");
    REQUIRE(units != std::string::npos);
    std::string moved = text;
    moved.insert(units, "; one\n; two\n; three\n");
    moved += "\n(open\n";
    change(kDefault, moved);
    CHECK(shadowed_line() == 7);
    CHECK(broken);
    const json carried = hints(kDefault);
    REQUIRE(carried.size() == 1);
    CHECK(carried[0]["label"] == "\u2190 local.dta:2 (50)");
    CHECK(carried[0]["position"] == json{{"line", 7}, {"character", 23}});
    // The hinted line itself edited: its hint and faded value are dropped, not guessed at.
    const size_t health = moved.find("(health 100)");
    REQUIRE(health != std::string::npos);
    change(kDefault, moved.replace(health, 12, "(health 120)"));
    CHECK(shadowed_line() == -1);
    CHECK(broken);
    CHECK(hints(kDefault).empty());
    // Once it reads again, the hints come from the new check.
    change(kDefault, moved.substr(0, moved.size() - 7));
    CHECK(shadowed_line() == 7);
    CHECK(!broken);
    CHECK(hints(kDefault).size() == 1);
    // And highlighting marks the shadowed array's name, on the same line.
    sent.clear();
    server.handle({{"jsonrpc", "2.0"},
                   {"id", 8},
                   {"method", "textDocument/semanticTokens/full"},
                   {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}}}}}},
                  now);
    std::vector<int> faded;  // lines (0-based) of tokens with the shadowed modifier
    for (const json& m : sent)
        if (m.contains("id") && m["id"] == 8) {
            const json& data = m["result"]["data"];
            int line = 0;
            for (size_t i = 0; i + 4 < data.size(); i += 5) {
                line += data[i].get<int>();
                if (data[i + 4].get<int>() & 8) faded.push_back(line);
            }
        }
    CHECK(faded == std::vector<int>{7});
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

TEST_CASE("--console names a game's port for the projects under a directory") {
    Setup s;
    std::string error;
    const std::vector<std::string> args = {"--console", "stacks/duel=4556", "--console",
                                           "/w/hearth=4557"};
    REQUIRE(parse_args(args, s, error));
    REQUIRE(s.console_ports.size() == 2);
    CHECK(s.console_ports[0].first == canonical_path("stacks/duel"));
    CHECK(s.console_ports[0].second == 4556);
    CHECK(s.console_ports[1].second == 4557);
    Setup bad;
    const std::vector<std::string> no_port = {"--console", "stacks/duel"};
    CHECK(!parse_args(no_port, bad, error));
    CHECK(error.find("DIR=PORT") != std::string::npos);
}

TEST_CASE("a file's structure is read again after each edit, and dropped on close") {
    Client c;
    c.request(1, "initialize");
    const json doc = {{"textDocument", {{"uri", uri_from_path(kDefault)}}}};
    const auto names = [&](int id) {
        std::vector<std::string> out;
        const json reply = c.request(id, "textDocument/documentSymbol", doc);
        for (const json& s : reply["result"]) out.push_back(s["name"]);
        return out;
    };
    c.open("(units (knight (health 100)))\n");
    CHECK(names(2) == std::vector<std::string>{"units"});
    CHECK(names(3) == std::vector<std::string>{"units"});  // the same text: the same read
    c.change("(units (knight (health 100)))\n(world (gravity 9))\n");
    CHECK(names(4) == std::vector<std::string>{"units", "world"});
    CHECK(c.request(5, "textDocument/foldingRange", doc)["result"].empty());
    c.notify("textDocument/didClose", doc);
    CHECK(names(6).size() == 3);  // the file on disk
}

TEST_CASE("semantic tokens: what each name is, from Baker's lexer and the last check") {
    Client c;
    const json init = c.request(1, "initialize");
    const json legend = init["result"]["capabilities"]["semanticTokensProvider"]["legend"];
    const std::string text =
        "#require \"../packs/game/core.dta\"\n"
        "[SPEEDY 9]\n"
        "(units (knight (health 100) (speed SPEEDY)))\n"
        "(world (gravity 9))\n"
        "#autorun {func game.twice ($n) {game.wait $n} {game.wait $n}}\n"
        "#autorun {game.twice 2} {clock thread_task 1 {game.wait 1}}\n";
    c.open(text);
    c.server.check_now();
    const json doc = {{"textDocument", {{"uri", uri_from_path(kDefault)}}}};
    // Each token as "text type modifier...", decoded from LSP's relative encoding.
    const auto decode = [&](const json& data) {
        std::vector<std::string> out;
        std::vector<std::string_view> lines;
        for (size_t at = 0, nl; (nl = text.find('\n', at)) != std::string::npos; at = nl + 1)
            lines.push_back(std::string_view(text).substr(at, nl - at));
        int line = 0, column = 0;
        for (size_t i = 0; i + 4 < data.size(); i += 5) {
            line += data[i].get<int>();
            column = data[i].get<int>() ? data[i + 1].get<int>() : column + data[i + 1].get<int>();
            std::string t(lines[static_cast<size_t>(line)].substr(static_cast<size_t>(column),
                                                                  data[i + 2].get<size_t>()));
            t += " " + legend["tokenTypes"][data[i + 3].get<size_t>()].get<std::string>();
            for (size_t m = 0; m < legend["tokenModifiers"].size(); ++m)
                if (data[i + 4].get<int>() & (1 << m))
                    t += " " + legend["tokenModifiers"][m].get<std::string>();
            out.push_back(std::move(t));
        }
        return out;
    };
    const std::vector<std::string> full =
        decode(c.request(2, "textDocument/semanticTokens/full", doc)["result"]["data"]);
    const auto has = [&](const std::string& t) {
        return std::find(full.begin(), full.end(), t) != full.end();
    };
    CHECK(has("#require keyword"));
    CHECK(has("SPEEDY macro declaration"));
    CHECK(has("SPEEDY macro"));
    CHECK(has("gravity property tweak"));
    CHECK(has("game.twice function declaration"));
    CHECK(has("game.twice function"));
    CHECK(has("game.wait function defaultLibrary"));
    CHECK(has("$n variable"));
    CHECK(has("clock variable defaultLibrary"));
    CHECK(has("thread_task method defaultLibrary"));
    CHECK(!has("health property"));  // a field, not a tweak: the grammar's color
    // A range: only its lines.
    const json range = {
        {"textDocument", {{"uri", uri_from_path(kDefault)}}},
        {"range",
         {{"start", {{"line", 3}, {"character", 0}}}, {"end", {{"line", 3}, {"character", 99}}}}}};
    const json ranged = c.request(3, "textDocument/semanticTokens/range", range)["result"]["data"];
    REQUIRE(ranged.size() == 5);
    CHECK(ranged[0] == 3);  // the first token's line, from the start of the file
}
