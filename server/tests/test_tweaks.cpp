#include "diagnose.hpp"
#include "positions.hpp"
#include "server.hpp"
#include "tweaks.hpp"

#include <doctest.h>

#include <vector>

using namespace baker_lsp;
using nlohmann::json;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kProjectFile = kProject + "/baker-project.dta";
const std::string kLocal = kProject + "/data/local.dta";  // optional, not on disk

// check_tweaks over the test project with `local` as its local layer.
std::vector<std::string> findings(const std::string& local) {
    baker::Context ctx;
    Reference reference;
    std::string error;
    REQUIRE(load_reference(ctx, kProject + "/reference.json", reference, error));
    Buffers buffers;
    buffers.set(kLocal, local);
    Setup s;
    s.project = kProjectFile;
    const Diagnosis d = diagnose(s, ctx, buffers, true);
    REQUIRE(d.loaded);
    const GameTweaks tweaks(reference.tweaks);
    std::vector<std::string> out;
    for (const Problem& p : tweaks.check(*d.loaded)) {
        CHECK(p.file == kLocal);
        out.push_back(std::string(p.severity == Severity::Error ? "error " : "warning ") + p.key +
                      ": " + p.message);
    }
    return out;
}

}  // namespace

TEST_CASE("the game's tweaks, made from its reference, for Baker's types only") {
    TweakInfo gravity;
    gravity.path = "world.gravity";
    gravity.type = "float32";
    gravity.default_text = "9.8";
    TweakInfo aim = gravity;
    aim.path = "world.aim";
    aim.type = "vec2";  // a type of the game's own
    aim.default_text = "0 0";
    TweakInfo team = gravity;
    team.path = "world.team";
    team.type = "symbol";
    team.default_text = "red";
    team.one_of = {"red", "blue"};
    const GameTweaks tweaks({gravity, aim, team});
    CHECK(tweaks.checks("world.gravity"));
    CHECK(tweaks.checks("world.team"));
    CHECK(!tweaks.checks("world.aim"));
    CHECK(tweaks.left_out() == std::vector<std::string>{"world.aim (vec2)"});
}

TEST_CASE("Baker holds the data to the game's tweaks: type, range, and a key none reads") {
    CHECK(findings("(world (gravity 9.8))\n").empty());
    CHECK(findings("(world (gravity 30))\n") ==
          std::vector<std::string>{"error gravity: 'world.gravity' is 30, outside its tweak's "
                                   "range 0.0 to 20.0"});
    CHECK(findings("(world (gravity heavy))\n") ==
          std::vector<std::string>{"error gravity: 'gravity' is symbol, not float (at world)"});
    CHECK(findings("(world (gravity 9.8) (gravty 2))\n") ==
          std::vector<std::string>{"warning gravty: 'gravty' in 'world' is read by no tweak; its "
                                   "tweaks read gravity"});
    // A tweak the data does not set has no place in the files: not shown.
    CHECK(findings("(units (knight (health 50)))\n").empty());
}

TEST_CASE("over LSP: a tweak's error is published, and the server's own range rule stands down") {
    std::vector<json> sent;
    Setup given;
    given.reference = kProject + "/reference.json";  // as --reference gives it
    Server server(given, [&sent](const json& m) { sent.push_back(m); });
    server.handle({{"jsonrpc", "2.0"},
                   {"id", 1},
                   {"method", "initialize"},
                   {"params", {{"rootUri", uri_from_path(kProject)}}}},
                  {});
    const auto open = [&](const std::string& path, const std::string& text) {
        server.handle(
            {{"jsonrpc", "2.0"},
             {"method", "textDocument/didOpen"},
             {"params", {{"textDocument", {{"uri", uri_from_path(path)}, {"text", text}}}}}},
            {});
    };
    open(kLocal, "(world (gravity 30))\n");
    sent.clear();
    server.check_now();
    std::vector<std::string> messages;
    for (const json& m : sent)
        if (m.value("method", "") == "textDocument/publishDiagnostics" &&
            m["params"]["uri"] == uri_from_path(kLocal))
            for (const json& d : m["params"]["diagnostics"]) messages.push_back(d["message"]);
    CHECK(messages == std::vector<std::string>{"'world.gravity' is 30, outside its tweak's range "
                                               "0.0 to 20.0"});
}
