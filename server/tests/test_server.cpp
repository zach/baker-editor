#include "positions.hpp"
#include "server.hpp"

#include <doctest.h>

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
        REQUIRE(sent.size() == 1);
        return sent[0];
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
    CHECK(c.request(2, "textDocument/hover")["error"]["code"] == -32601);
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

TEST_CASE("exit after shutdown is a clean exit") {
    Client c;
    c.request(1, "shutdown");
    c.notify("exit", json::object());
    CHECK(c.server.exit_code() == 0);

    Client d;
    d.notify("exit", json::object());
    CHECK(d.server.exit_code() == 1);
}
