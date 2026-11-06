#include "diagnose.hpp"
#include "features.hpp"
#include "merged.hpp"
#include "positions.hpp"
#include "server.hpp"

#include <baker/parse.hpp>

#include <doctest.h>

#include <vector>

using namespace baker_lsp;
using nlohmann::json;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kProjectFile = kProject + "/baker-project.dta";
const std::string kDefault = kProject + "/data/default.dta";
const std::string kLocal = kProject + "/data/local.dta";  // optional, not on disk

struct Stack {
    baker::Context ctx;
    Buffers buffers;
    std::shared_ptr<Loaded> loaded;
    // `below` is added to the end of default.dta, as an editor's text.
    explicit Stack(const std::string& local = {}, const std::string& below = {}) {
        if (!local.empty()) buffers.set(kLocal, local);
        if (!below.empty()) {
            std::string text;
            REQUIRE(buffers.read(kDefault, text));
            buffers.set(kDefault, text + below);
        }
        Setup s;
        s.project = kProjectFile;
        const Diagnosis d = diagnose(s, ctx, buffers, false);
        loaded = d.loaded;
        REQUIRE(loaded);
        REQUIRE(loaded->config);
    }
};

// The view's line holding `text`, 0-based, or -1.
int line_of(const View& v, const std::string& text) {
    int line = 0;
    for (size_t at = 0; at < v.text.size(); ++line) {
        const size_t end = v.text.find('\n', at);
        if (v.text.substr(at, end - at).find(text) != std::string::npos) return line;
        at = end + 1;
    }
    return -1;
}

}  // namespace

TEST_CASE("the merged view reads back as the merged tree, each value naming its source") {
    Stack stack("(units (knight (health 50)))\n");
    const View v = merged_view(*stack.loaded->config, kProject);
    const auto read = baker::read_string(v.text, "merged");
    REQUIRE(read);
    CHECK(baker::structurally_equal(**read, *stack.loaded->config->root()));

    const int health = line_of(v, "(health 50)");
    REQUIRE(health >= 0);
    CHECK(v.text.find("(health 50)  ; local data/local.dta:1") != std::string::npos);
    const int speed = line_of(v, "(speed 2.5)");
    bool found_health = false, found_speed = false;
    for (const ViewSource& s : v.sources) {
        if (s.line == health) found_health = s.file == kLocal && s.file_line == 1;
        if (s.line == speed) found_speed = s.file == kDefault && s.file_line == 5;
    }
    CHECK(found_health);
    CHECK(found_speed);
}

TEST_CASE("a layer's view lists what it changes, as a game's diff prints it") {
    Stack stack("; tuned\n(units (knight (health 50)) (mage (health 40)))\n");
    std::string error;
    const auto v = layer_view(*stack.loaded->config, "local", error);
    REQUIRE(v);
    CHECK(v->text ==
          "layer 'local' changes:\n"
          "  units.knight.health  100 -> 50  (" +
              kLocal +
              ":2)\n"
              "  units.mage.health  (new) -> 40  (" +
              kLocal + ":2)\n");
    REQUIRE(v->sources.size() == 2);
    CHECK(v->sources[0].line == 1);
    CHECK(v->sources[0].file == kLocal);
    CHECK(v->sources[0].file_line == 2);

    CHECK(layer_view(*stack.loaded->config, "default", error)->text.find("(new) -> 2.5") !=
          std::string::npos);
    CHECK(!layer_view(*stack.loaded->config, "nowhere", error));
    CHECK(!error.empty());
}

TEST_CASE("over LSP: the views, and a lens on each layer of the project file") {
    std::vector<json> sent;
    Server server(Setup{}, [&sent](const json& m) { sent.push_back(m); });
    const auto request = [&](const std::string& method, const json& params) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"}, {"id", 9}, {"method", method}, {"params", params}}, {});
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == 9) return m;
        return json();
    };
    request("initialize", {{"rootUri", uri_from_path(kProject)}});
    const json merged = request("baker/merged", {{"uri", uri_from_path(kDefault)}})["result"];
    CHECK(merged["text"].get<std::string>().find(
              "(knight (health 100) (speed 2.5))  ; default data/default.dta:5") !=
          std::string::npos);
    CHECK(!merged["sources"].empty());
    CHECK(merged["sources"][0]["uri"].get<std::string>().rfind("file://", 0) == 0);
    CHECK(request("baker/layers", {{"uri", uri_from_path(kDefault)}})["result"] ==
          json::array({"default", "local"}));  // local is optional: empty, its file missing
    CHECK(request("baker/layerEffect", {{"uri", uri_from_path(kDefault)}, {"layer", "nowhere"}})
              .contains("error"));

    std::string text;
    REQUIRE(Buffers().read(kProjectFile, text));
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kProjectFile)}, {"text", text}}}}}},
        {});
    const json lenses =
        request("textDocument/codeLens",
                {{"textDocument", {{"uri", uri_from_path(kProjectFile)}}}})["result"];
    REQUIRE(lenses.size() == 3);
    CHECK(lenses[0]["command"]["command"] == "baker.showMerged");
    CHECK(lenses[1]["command"]["arguments"][1] == "default");
    CHECK(lenses[2]["command"]["arguments"][1] == "local");
    CHECK(lenses[2]["range"]["start"]["line"] == 6);
    CHECK(request("textDocument/codeLens",
                  {{"textDocument", {{"uri", uri_from_path(kDefault)}}}})["result"]
              .empty());
}

TEST_CASE("a verb row: its hint shows the list below and what it makes, nothing faded") {
    Stack stack("; tuned\n(world (spawns #append 5))\n", "(world (spawns 3 4))\n");
    const Loaded& l = *stack.loaded;
    REQUIRE(l.rows.size() == 1);
    const LayerRow& c = l.rows[0];
    CHECK(c.path == "world/spawns");
    CHECK(c.op == "#append");
    CHECK(c.operands == "5");
    CHECK(c.below_values == std::vector<std::string>{"3", "4"});
    CHECK(c.below == "(3 4)");
    CHECK(c.below_file == kDefault);
    CHECK(c.below_line == 12);
    CHECK(c.result == "(3 4 5)");
    std::vector<std::string> labels;
    for (const LayerHint& h : l.hints) {
        labels.push_back(h.label);
        CHECK(!h.shadowed);
    }
    CHECK(labels == std::vector<std::string>{"\u2190 default.dta:12 (3 4) #append 5 \u2192 (3 4 5)",
                                             "composed by local.dta:2 (#append 5)"});
    CHECK(l.shadowed.empty());  // the list below stands: not faded

    Stack drop("(world (spawns #drop 3))\n", "(world (spawns 3 4))\n");
    REQUIRE(drop.loaded->rows.size() == 1);
    CHECK(drop.loaded->rows[0].result == "4");
}

TEST_CASE("the views name a verb: beside the merged value, and as diff prints it") {
    Stack stack("(world (spawns #append 5))\n(units (knight (health 50 (@was 30))))\n",
                "(world (spawns 3 4))\n");
    const View merged = merged_view(*stack.loaded->config, kProject, stack.loaded->rows);
    CHECK(merged.text.find("(world (spawns 3 4 5))  ; default data/default.dta:12, local #append 5 "
                           "data/local.dta:1") != std::string::npos);
    std::string error;
    const auto v = layer_view(*stack.loaded->config, "local", error);
    REQUIRE(v);
    CHECK(v->text.find("world.spawns  (3 4) -> #append 5  (") != std::string::npos);
    CHECK(v->text.find("units.knight.health  100 -> 50  (" + kLocal + ":2)  stale") !=
          std::string::npos);
}

TEST_CASE("hover on a verb row: the list below, the verb and what it makes") {
    const std::string local = "(world (spawns #append 5))\n";
    Stack stack(local, "(world (spawns 3 4))\n");
    const auto h = hover(FileRead(local), local.find("spawns") + 1, *stack.loaded, Reference{});
    REQUIRE(h);
    CHECK(h->markdown.find("local (local.dta:1): `(3 4) #append 5`, the list from default.dta:12 "
                           "\u2192 `(3 4 5)`") != std::string::npos);
    CHECK(h->markdown.find("| local | `5` | local.dta:1 | composes `#append` |") !=
          std::string::npos);
}

TEST_CASE("a verb row on a list of records: by their keys, the one operand read as one") {
    Stack stack("(world (crowd #append (c (x 3) (y 4))))\n",
                "(world (crowd (a (x 1)) (b (x 2))))\n");
    REQUIRE(stack.loaded->rows.size() == 1);
    const LayerRow& r = stack.loaded->rows[0];
    CHECK(r.values == std::vector<std::string>{"(c (x 3) (y 4))"});  // one operand, not three
    CHECK(r.operands == "c");
    CHECK(r.below == "(a b)");
    CHECK(r.below_file == kDefault);  // Baker 0.8.0 gives the list a verb changes, and its row
    CHECK(r.below_line == 12);
    CHECK(r.result == "(a b c)");
    CHECK(r.result_values.size() == 3);
    bool hinted = false;
    for (const LayerHint& h : stack.loaded->hints)
        hinted = hinted || h.label == "← default.dta:12 (a b) #append c → (a b c)";
    CHECK(hinted);
}

TEST_CASE("a #drop on a list of records, and a value that is one array: as Baker gives them") {
    Stack drop("(world (crowd #drop (a (x 1))))\n", "(world (crowd (a (x 1)) (b (x 2))))\n");
    REQUIRE(drop.loaded->rows.size() == 1);
    const LayerRow& d = drop.loaded->rows[0];
    CHECK(d.below == "(a b)");
    CHECK(d.below_line == 12);
    CHECK(d.result == "b");
    // (aim (0 0)) is one value, an array: a pin would write (@was (0 0)), not (@was 0 0).
    Stack aim("(world (aim (1 1)))\n", "(world (aim (0 0)))\n");
    REQUIRE(aim.loaded->rows.size() == 1);
    CHECK(aim.loaded->rows[0].values == std::vector<std::string>{"(1 1)"});
    CHECK(aim.loaded->rows[0].below_values == std::vector<std::string>{"(0 0)"});
}
