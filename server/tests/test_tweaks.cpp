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

TEST_CASE("the game's tweaks, made from their descriptions, a game's own types too") {
    TweakInfo gravity;
    gravity.path = "world.gravity";
    gravity.type = "float32";
    gravity.default_text = "9.8";
    TweakInfo aim = gravity;
    aim.path = "world.aim";
    aim.type = "vec2";  // a type of the game's own, with its values
    aim.values = {"float32", "float32"};
    aim.default_text = "(0 0)";
    TweakInfo old = aim;
    old.path = "world.old";
    old.values.clear();  // as a reference before 1.2 writes it
    TweakInfo team = gravity;
    team.path = "world.team";
    team.type = "symbol";
    team.default_text = "red";
    team.one_of = {"red", "blue"};
    const GameTweaks tweaks({gravity, aim, old, team});
    CHECK(tweaks.checks("world.gravity"));
    CHECK(tweaks.checks("world.aim"));
    CHECK(tweaks.checks("world.team"));
    CHECK(!tweaks.checks("world.old"));
    REQUIRE(tweaks.left_out().size() == 1);
    CHECK(tweaks.left_out()[0].starts_with("world.old: "));
}

TEST_CASE("list tweaks, made from their elements' descriptions (baker.tweaks/1.5)") {
    baker::Context ctx;
    Reference reference;
    std::string error;
    REQUIRE(load_reference(ctx, kProject + "/reference.json", reference, error));
    const GameTweaks tweaks(reference.tweaks);
    CHECK(tweaks.checks("world.path"));    // two vec2 points
    CHECK(tweaks.checks("world.spawns"));  // any number of int32
    CHECK(!tweaks.checks("world.old"));    // written before 1.5: no element said
    CHECK(findings("(world (path (0 0) (4 4)) (spawns 3 4 5))\n").empty());
    // A point given one number; three points where two are wanted.
    const std::vector<std::string> point = findings("(world (path (0 0) (4)))\n");
    REQUIRE(point.size() == 1);
    CHECK(point[0].starts_with("error path: "));
    CHECK(findings("(world (path (0 0) (4 4) (8 8)))\n").size() == 1);
    // Each element held to its type, and to the tweak's range.
    const std::vector<std::string> spawn = findings("(world (spawns 3 four))\n");
    REQUIRE(spawn.size() == 1);
    CHECK(spawn[0].starts_with("error spawns: "));
    const std::vector<std::string> ranged = findings("(world (spawns 3 40))\n");
    REQUIRE(ranged.size() == 1);
    CHECK(ranged[0].find("40") != std::string::npos);
}

TEST_CASE("Baker holds the data to the game's tweaks: type, range, and a key none reads") {
    CHECK(findings("(world (gravity 9.8))\n").empty());
    CHECK(findings("(world (gravity 30))\n") ==
          std::vector<std::string>{"error gravity: 'world.gravity' is 30, outside its tweak's "
                                   "range 0.0 to 20.0"});
    CHECK(findings("(world (gravity heavy))\n") ==
          std::vector<std::string>{"error gravity: 'gravity' is symbol, not float"});
    CHECK(findings("(world (gravity 9.8) (gravty 2))\n") ==
          std::vector<std::string>{"warning gravty: 'gravty' in 'world' is read by no tweak; its "
                                   "tweaks read aim, gravity, path, spawns"});
    // A tweak Baker could not make (a fixed-size list's values are not described) is no stray.
    CHECK(findings("(world (gravity 9.8) (old 1 2))\n").empty());
    // A tweak of the game's own type, by the values its reference gives.
    const std::vector<std::string> aim = findings("(world (aim 1))\n");
    REQUIRE(aim.size() == 1);
    CHECK(aim[0].starts_with("error aim: "));
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
