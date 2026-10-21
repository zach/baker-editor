#include "features.hpp"
#include "hints.hpp"
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
    Reference reference;
    std::shared_ptr<Loaded> loaded;
    explicit Stack(const std::string& local = {}) {
        std::string error;
        REQUIRE(load_reference(ctx, kProject + "/reference.json", reference, error));
        if (!local.empty()) buffers.set(kLocal, local);
        Setup s;
        s.project = kProjectFile;
        s.objects = reference.objects;
        d = diagnose(s, ctx, buffers, true);
        loaded = d.loaded;
        REQUIRE(loaded);
    }
    Diagnosis d;
    // Completion at the end of `text`, written as `file`.
    std::vector<std::string> complete_at(const std::string& text, const std::string& file,
                                         std::vector<std::string>* snippets = nullptr) {
        const Completions c = complete(
            FileRead(text), text.size(), *loaded, reference, file,
            [this](const std::string& f, std::string& out) { return buffers.read(f, out); });
        std::vector<std::string> out;
        for (const Completion& i : c.items) {
            out.push_back(i.label);
            if (snippets) snippets->push_back(i.snippet);
        }
        return out;
    }
};

std::string hover_at(Stack& s, const std::string& text, const std::string& token) {
    const auto h = hover(FileRead(text), text.find(token) + 1, *s.loaded, s.reference);
    return h ? h->markdown : std::string();
}

}  // namespace

TEST_CASE("metadata beside a value is told from data that happens to be named kind") {
    const auto read = [](const char* text) {
        auto r = baker::read_string(text, "t");
        REQUIRE(r);
        return *r;
    };
    const auto meta = read("(w 2 (range 0 10) (desc \"x\"))");
    CHECK(has_meta(*(*meta)[0].try_array()));
    const auto data = read("(ball (kind ball) (pos 1 2))");
    CHECK(!has_meta(*(*data)[0].try_array()));
    const auto mixed = read("(w 2 (range 0 10) (pos 1 2))");
    CHECK(!has_meta(*(*mixed)[0].try_array()));
    CHECK(has_meta(*(*read("(w (range 0 10))"))[0].try_array()));  // Baker's rule: trailing ones
}

TEST_CASE("hints: the data's metadata over the shape's, both over the tweak's code") {
    Stack s(
        "(units (knight (speed 2.5 (range 0 4) (note \"slow on purpose\"))))\n"
        "(world (gravity 9.8 (desc \"Down\")))\n");
    const Hints speed = hints_at(*s.loaded, s.reference, {"units", "knight", "speed"});
    CHECK(speed.written);
    CHECK(speed.max == 4);  // the data's, over the shape's 10
    CHECK(speed.note == "slow on purpose");
    REQUIRE(speed.field);
    const Hints gravity = hints_at(*s.loaded, s.reference, {"world", "gravity"});
    REQUIRE(gravity.tweak);
    CHECK(gravity.max == 20);
    CHECK(gravity.desc == "Down");
    const std::string h = hover_at(s, "(world (gravity 9.8))", "gravity");
    CHECK(h.find("the game's tweak `world.gravity`: `float32`, code default `9.8`, 0 to 20") !=
          std::string::npos);
    CHECK(h.find("dev only") != std::string::npos);
    CHECK(h.find("in the data:\n\nDown") != std::string::npos);
}

TEST_CASE("choosing a field writes a value: the stack's, the tweak's, the range's middle") {
    Stack s;
    std::vector<std::string> snippets;
    // Overriding a value in local.dta: the value the stack has now.
    CHECK(s.complete_at("(units (knight (hea", kLocal, &snippets) ==
          std::vector<std::string>{"health"});
    CHECK(snippets[0] == "health ${1:100}");
    snippets.clear();
    // A new fighter: the middle of each field's range.
    CHECK(s.complete_at("(units (mage (", kLocal, &snippets) ==
          std::vector<std::string>{"health", "speed"});
    CHECK(snippets == std::vector<std::string>{"health ${1:500}", "speed ${1:5}"});
    snippets.clear();
    // A tweak no shape declares: its code default.
    CHECK(s.complete_at("(world (", kLocal, &snippets) ==
          std::vector<std::string>{"gravity", "aim", "old"});
    CHECK(snippets[0] == "gravity ${1:9.8}");
    CHECK(snippets[1] == "aim ${1:(0 0)}");
}

TEST_CASE("a symbol value is offered from the values the project gives the same field") {
    Stack s("(units (knight (role tank)) (archer (role ranged)) (squire (role ranged)))\n");
    CHECK(s.complete_at("(units (rogue (role ", kDefault) ==
          std::vector<std::string>{"ranged", "tank", "SPEEDY"});
    CHECK(s.complete_at("(units (rogue (role r", kDefault) == std::vector<std::string>{"ranged"});
    CHECK(s.complete_at("(other (rogue (role ", kDefault) == std::vector<std::string>{"SPEEDY"});
}

TEST_CASE("a range only the running game enforces is a warning") {
    Stack s("(world (gravity 30))\n(look (zoom 12 (range 0 10)))\n(units (archer (speed 3)))\n");
    std::vector<std::string> found;
    for (const Problem& p : range_problems(*s.loaded, s.reference)) {
        CHECK(p.severity == Severity::Warning);
        CHECK(p.file == kLocal);
        found.push_back(p.key + ": " + p.message);
    }
    CHECK(found == std::vector<std::string>{
                       "gravity: 'gravity' is 30, outside 0 to 20 (the game's tweak "
                       "world.gravity): the running game refuses it",
                       "zoom: 'zoom' is 12, outside 0 to 10: the running game refuses it"});
}

TEST_CASE("a color swatch where Baker calls a value a color, and the picker writes it back") {
    std::vector<json> sent;
    Server server(Setup{}, [&sent](const json& m) { sent.push_back(m); });
    const auto request = [&](const std::string& method, const json& params) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"}, {"id", 9}, {"method", method}, {"params", params}}, {});
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == 9) return m["result"];
        return json();
    };
    request("initialize", {{"rootUri", uri_from_path(kProject)}});
    const std::string text =
        "(look\n   (tint 0.9 0.5 0.2 (range 0 1))\n   (shade 0.1 0.2 0.3 1 (kind color))\n"
        "   (pos 0.1 0.2 0.3)\n   (box 0.1 0.2 0.3 (range 0 1) (kind meters)))\n";
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kLocal)}, {"text", text}}}}}},
        {});
    const json doc = {{"textDocument", {{"uri", uri_from_path(kLocal)}}}};
    const json colors = request("textDocument/documentColor", doc);
    REQUIRE(colors.size() == 2);
    CHECK(colors[0]["range"] == json{{"start", {{"line", 1}, {"character", 9}}},
                                     {"end", {{"line", 1}, {"character", 20}}}});
    CHECK(colors[0]["color"]["red"] == 0.9);
    CHECK(colors[0]["color"]["alpha"] == 1.0);
    CHECK(colors[1]["color"]["alpha"] == 1.0);
    CHECK(colors[1]["range"]["start"]["line"] == 2);

    json pick = doc;
    pick["range"] = colors[0]["range"];
    pick["color"] = {{"red", 0.25}, {"green", 1.0}, {"blue", 0.3333333}, {"alpha", 1.0}};
    const json p = request("textDocument/colorPresentation", pick);
    REQUIRE(p.size() == 1);
    CHECK(p[0]["textEdit"]["newText"] == "0.25 1 0.333");
    pick["range"] = colors[1]["range"];
    CHECK(request("textDocument/colorPresentation", pick)[0]["label"] == "0.25 1 0.333 1");
}

TEST_CASE("a field's allowed symbols: offered in their order, and as the choice it writes") {
    Stack s(
        "(shapes (unit (role symbol (one_of tank ranged healer) (optional))))\n"
        "(units (knight (role tank)) (archer (role tank)))\n");
    // The list, in its order, not the values in use.
    CHECK(s.complete_at("(units (rogue (role ", kDefault) ==
          std::vector<std::string>{"tank", "ranged", "healer"});
    CHECK(s.complete_at("(units (rogue (role h", kDefault) == std::vector<std::string>{"healer"});
    std::vector<std::string> snippets;
    const std::vector<std::string> fields = s.complete_at("(units (rogue (", kLocal, &snippets);
    REQUIRE(fields.size() == 3);
    CHECK(fields[2] == "role");
    CHECK(snippets[2] == "role ${1|tank,ranged,healer|}");
    const std::string h = hover_at(s, "(units (knight (role tank)))", "role");
    CHECK(h.find("optional: one of tank, ranged, healer") != std::string::npos);
    // A value outside it is check's error.
    Stack typo(
        "(shapes (unit (role symbol (one_of tank ranged healer) (optional))))\n"
        "(units (knight (role tnak)))\n");
    bool reported = false;
    for (const Problem& p : typo.d.problems)
        reported =
            reported || p.message.find("not one of tank, ranged, healer") != std::string::npos;
    CHECK(reported);
}

TEST_CASE("one_of beside a value is metadata") {
    Stack s("(look (mode fast (one_of fast slow) (desc \"How\")))\n");
    const Hints h = hints_at(*s.loaded, s.reference, {"look", "mode"});
    CHECK(h.written);
    CHECK(h.one_of == std::vector<std::string>{"fast", "slow"});
    CHECK(h.desc == "How");
}
