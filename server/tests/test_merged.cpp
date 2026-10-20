#include "diagnose.hpp"
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
    explicit Stack(const std::string& local = {}) {
        if (!local.empty()) buffers.set(kLocal, local);
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
