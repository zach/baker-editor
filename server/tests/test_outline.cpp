#include "outline.hpp"
#include "positions.hpp"
#include "server.hpp"

#include <doctest.h>

#include <algorithm>

using namespace baker_lsp;
using nlohmann::json;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kDefault = kProject + "/data/default.dta";
const std::string kLocal = kProject + "/data/local.dta";
const std::string kCore = kProject + "/packs/game/core.dta";

struct Fixture {
    Setup setup;
    baker::Context ctx;
    Reference reference;
    Buffers buffers;
    std::shared_ptr<Loaded> loaded;

    explicit Fixture(const std::string& local = {}) {
        setup.project = kProject + "/baker-project.dta";
        setup.reference = kProject + "/reference.json";
        std::string error;
        REQUIRE(load_reference(ctx, setup.reference, reference, error));
        if (!local.empty()) buffers.set(kLocal, local);
        loaded = diagnose(setup, ctx, buffers, true).loaded;
        REQUIRE(loaded);
    }
    std::string text(const std::string& path) const {
        std::string t;
        REQUIRE(buffers.read(path, t));
        return t;
    }
    std::vector<Occurrence> find(const std::string& path, const std::string& at) {
        const std::string t = text(path);
        const auto target = target_at(read_structure(t), t.find(at), *loaded, reference);
        REQUIRE(target);
        const auto read = [this](const std::string& f, std::string& out) {
            return buffers.read(f, out);
        };
        std::vector<std::string> files = loaded->files;
        if (!buffers.all().empty())
            for (const auto& [f, ignored] : buffers.all())
                if (std::find(files.begin(), files.end(), f) == files.end()) files.push_back(f);
        return occurrences(*target, files, *loaded, read);
    }
};

}  // namespace

TEST_CASE("a file's structure: a row's key, segment by segment") {
    const std::string text = "(/units.knight/'max_hp' 9)\n(/a..b 1)\n(x (/y 1))";
    const Structure s = read_structure(text);
    REQUIRE(s.brackets.size() == 4);
    CHECK(s.is_row(0));
    const auto key = s.key(0);
    REQUIRE(key.size() == 3);
    CHECK(key[2].name == "max_hp");
    CHECK(text.substr(key[2].begin, key[2].end - key[2].begin) == "'max_hp'");
    CHECK(text.substr(key[1].begin, key[1].end - key[1].begin) == "knight");
    CHECK(s.name(0) == "max_hp");
    CHECK(s.tag(0) == "/units.knight/'max_hp'");
    CHECK(s.data_path(0) == std::vector<std::string>{"units", "knight", "max_hp"});
    CHECK(!s.is_row(1));  // an empty segment
    CHECK(s.data_path(1).empty());
    CHECK(!s.is_row(3));  // below the top level, a symbol
}

TEST_CASE("a file's structure: metadata is no child of its value") {
    const Structure s = read_structure("(a (b 1 (@range 0 9) (@note \"x\")) (@ 1))");
    REQUIRE(s.brackets.size() == 5);
    CHECK(s.brackets[0].has_child);   // (b ...) and (@ 1): a lone @ is a tag like any
    CHECK(!s.brackets[1].has_child);  // (b 1 ...) holds metadata alone
    CHECK(s.is_metadata(2));
    CHECK(!s.is_metadata(1));
    CHECK(!s.is_metadata(4));
}

TEST_CASE("a file's structure: brackets, tags and tokens") {
    const Structure s = read_structure("(a (b 1) {f 'x y'}) ; c\n/* d */ [M 2]");
    REQUIRE(s.brackets.size() == 4);
    CHECK(s.tag(0) == "a");
    CHECK(s.tag(1) == "b");
    CHECK(s.brackets[1].parent == 0);
    CHECK(s.tag(2) == "f");
    CHECK(s.tag(3) == "M");
    CHECK(s.data_path(1) == std::vector<std::string>{"a", "b"});
    CHECK(s.data_path(2).empty());  // a command is not data
    REQUIRE(s.comments.size() == 2);
    CHECK(!s.comments[0].block);
    CHECK(s.comments[1].block);
    const auto quoted = std::find_if(s.tokens.begin(), s.tokens.end(),
                                     [](const Token& t) { return t.quote == '\''; });
    REQUIRE(quoted != s.tokens.end());
    CHECK(quoted->text == "x y");
    CHECK(quoted->index == 1);
}

TEST_CASE("the outline: arrays by tag, leaves as fields, macros and script functions") {
    Fixture f;
    const std::string text = f.text(kDefault);
    const auto symbols = outline(read_structure(text), text);
    std::vector<std::string> top;
    for (const OutlineSymbol& s : symbols) top.push_back(s.name);
    CHECK(top == std::vector<std::string>{"units", "SPEEDY", "game.twice"});
    const OutlineSymbol& units = symbols[0];
    CHECK(units.kind == SymbolKind::Namespace);
    REQUIRE(units.children.size() == 2);
    CHECK(units.children[0].name == "knight");
    CHECK(units.children[0].kind == SymbolKind::Object);
    REQUIRE(units.children[0].children.size() == 2);
    CHECK(units.children[0].children[0].name == "health");
    CHECK(units.children[0].children[0].kind == SymbolKind::Field);
    CHECK(units.children[0].children[0].detail == "100");
    CHECK(symbols[1].kind == SymbolKind::Constant);
    CHECK(symbols[2].kind == SymbolKind::Function);
}

TEST_CASE("folds: brackets and block comments over more than one line") {
    const std::string_view text = "(a\n  (b 1))\n(c 2)\n/* x\n y */";
    const auto f = folds(read_structure(text), text);
    REQUIRE(f.size() == 2);
    CHECK(!f[0].comment);
    CHECK(f[1].comment);
}

TEST_CASE("references: a macro, its definition and its uses in every file") {
    Fixture f("(units (knight (speed SPEEDY)))\n");
    const auto where = f.find(kLocal, "SPEEDY");
    REQUIRE(where.size() == 2);
    CHECK(std::count_if(where.begin(), where.end(),
                        [](const Occurrence& o) { return o.file == kDefault; }) == 1);
    CHECK(std::count_if(where.begin(), where.end(),
                        [](const Occurrence& o) { return o.file == kLocal; }) == 1);
}

TEST_CASE("references: a shape field, in every array the shape governs and its declaration") {
    Fixture f;
    const auto where = f.find(kDefault, "health");
    CHECK(where.size() == 3);  // knight's, archer's, and (shapes (unit (health ...)))
    CHECK(std::count_if(where.begin(), where.end(),
                        [](const Occurrence& o) { return o.file == kCore; }) == 1);
}

TEST_CASE("references: a script function, called and defined") {
    Fixture f;
    f.buffers.set(kProject + "/sim/opening.dta", "(start {game.twice 3})\n");
    const auto where = f.find(kProject + "/sim/opening.dta", "game.twice");
    CHECK(where.size() == 2);
}

TEST_CASE("names: what a rename may write") {
    Target macro;
    Target field;
    field.kind = Target::Kind::Field;
    CHECK(valid_name("FAST", macro));
    CHECK(!valid_name("two words", macro));
    CHECK(valid_name("two words", field));  // written quoted
    CHECK(!valid_name("a(b", field));
    CHECK(!valid_name("", field));
}

TEST_CASE("rename over LSP: every file's edits; the game's functions are not renamable") {
    std::vector<json> sent;
    Setup s;
    s.project = kProject + "/baker-project.dta";
    s.reference = kProject + "/reference.json";
    Server server(s, [&sent](const json& m) { sent.push_back(m); });
    const auto request = [&](int id, const std::string& method, json params) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}, {});
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == id) return m;
        return json();
    };
    request(1, "initialize", json::object());
    server.handle({{"jsonrpc", "2.0"},
                   {"method", "textDocument/didOpen"},
                   {"params",
                    {{"textDocument",
                      {{"uri", uri_from_path(kLocal)},
                       {"text", "(units (knight (speed SPEEDY)))\n{game.spawn knight}\n"}}}}}},
                  {});
    const json at_macro = {{"textDocument", {{"uri", uri_from_path(kLocal)}}},
                           {"position", {{"line", 0}, {"character", 24}}},
                           {"newName", "QUICK"}};
    CHECK(request(2, "textDocument/prepareRename", at_macro)["result"]["placeholder"] == "SPEEDY");
    const json edit = request(3, "textDocument/rename", at_macro)["result"]["changes"];
    CHECK(edit[uri_from_path(kLocal)].size() == 1);
    CHECK(edit[uri_from_path(kDefault)].size() == 1);
    CHECK(edit[uri_from_path(kLocal)][0]["newText"] == "QUICK");

    // Applied, the project checks as clean as before: every use moved with the definition.
    std::string text;
    Buffers disk;
    REQUIRE(disk.read(kDefault, text));
    const json at = edit[uri_from_path(kDefault)][0]["range"]["start"];
    const size_t from = offset_of(text, {at["line"], at["character"]});
    text.replace(from, 6, "QUICK");
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didOpen"},
         {"params", {{"textDocument", {{"uri", uri_from_path(kDefault)}, {"text", text}}}}}},
        {});
    server.handle(
        {{"jsonrpc", "2.0"},
         {"method", "textDocument/didChange"},
         {"params",
          {{"textDocument", {{"uri", uri_from_path(kLocal)}}},
           {"contentChanges",
            json::array({{{"text", "(units (knight (speed QUICK)))\n{game.spawn knight}\n"}}})}}}},
        {});
    sent.clear();
    server.check_now();
    for (const json& m : sent)
        if (m.value("method", "") == "textDocument/publishDiagnostics")
            for (const json& d : m["params"]["diagnostics"])
                CHECK(d["code"] == "shadowed");  // local's speed shadows default's

    const json at_native = {{"textDocument", {{"uri", uri_from_path(kLocal)}}},
                            {"position", {{"line", 1}, {"character", 3}}},
                            {"newName", "game.make"}};
    CHECK(request(4, "textDocument/prepareRename", at_native)["result"].is_null());
    CHECK(request(5, "textDocument/rename", at_native)["error"]["code"] == -32602);

    const json symbols = request(6, "textDocument/documentSymbol",
                                 {{"textDocument", {{"uri", uri_from_path(kDefault)}}}})["result"];
    REQUIRE(symbols.size() == 3);
    CHECK(symbols[0]["name"] == "units");
    CHECK(symbols[0]["children"][0]["children"][0]["detail"] == "100");
}

TEST_CASE("the outline: a #shape with its fields and their types, a #pack, no metadata") {
    Fixture f;
    const std::string core = f.text(kCore);
    const auto symbols = outline(read_structure(core), core);
    REQUIRE(symbols.size() == 2);
    CHECK(symbols[0].name == "core");
    CHECK(symbols[0].kind == SymbolKind::Package);
    CHECK(symbols[0].detail == "#pack");
    const OutlineSymbol& unit = symbols[1];
    CHECK(unit.name == "unit");
    CHECK(unit.kind == SymbolKind::Struct);
    CHECK(unit.detail == "#shape");
    REQUIRE(unit.children.size() == 2);  // not (@at units) nor (@keyed)
    CHECK(unit.children[0].name == "health");
    CHECK(unit.children[0].kind == SymbolKind::Field);
    CHECK(unit.children[0].detail == "int");
    CHECK(unit.children[0].children.empty());
    const std::string text =
        "#shape (tower (aims (list (key_of units))\n  (@optional)))\n"
        "(towers (t1 (aims knight (@note \"one\")) (w 2 (@range 0 9))))";
    const auto towers = outline(read_structure(text), text);
    REQUIRE(towers.size() == 2);
    REQUIRE(towers[0].children.size() == 1);
    CHECK(towers[0].children[0].detail == "(list (key_of units))");
    REQUIRE(towers[1].children.size() == 1);
    const OutlineSymbol& t1 = towers[1].children[0];
    REQUIRE(t1.children.size() == 2);
    CHECK(t1.children[0].kind == SymbolKind::Field);  // a value with metadata is a leaf
    CHECK(t1.children[0].detail == "knight");
    CHECK(t1.children[1].detail == "2");
}

TEST_CASE("references and rename: a field in a row's key, segment by segment") {
    const std::string rows = "(/units.knight.health 50)\n(/units.archer (health 60))\n";
    Fixture f(rows);
    const std::vector<Occurrence> found = f.find(kLocal, "health");  // in the first row's key
    std::vector<std::string> where;
    for (const Occurrence& o : found) {
        std::string t;
        REQUIRE(f.buffers.read(o.file, t));
        where.push_back(o.file.substr(o.file.rfind('/') + 1) + ":" + std::to_string(o.begin) +
                        (o.in_key ? " key" : ""));
        CHECK(t.substr(o.begin, o.end - o.begin) == "health");
    }
    std::sort(where.begin(), where.end());
    const size_t nested = rows.find("(health") + 1;
    CHECK(where.size() == 5);  // the declaration, default's two, the row's key and its record's
    CHECK(std::count(where.begin(), where.end(), "local.dta:15 key") == 1);
    CHECK(std::count(where.begin(), where.end(), "local.dta:" + std::to_string(nested)) == 1);
    // Not a field of the shape: units, and a segment no shape governs.
    Fixture g("(/units.knight.health 50)\n(/world.gravity 9)\n");
    const std::string text = g.text(kLocal);
    CHECK(!target_at(read_structure(text), 3, *g.loaded, g.reference));
    CHECK(!target_at(read_structure(text), text.find("gravity"), *g.loaded, g.reference));
}

TEST_CASE("rename over LSP in a row's key: quoted when it must be, refused with a space") {
    std::vector<json> sent;
    Setup s;
    s.project = kProject + "/baker-project.dta";
    s.reference = kProject + "/reference.json";
    Server server(s, [&sent](const json& m) { sent.push_back(m); });
    const auto request = [&](int id, const std::string& method, json params) {
        sent.clear();
        server.handle({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}, {});
        for (const json& m : sent)
            if (m.contains("id") && m["id"] == id) return m;
        return json();
    };
    request(1, "initialize", json::object());
    server.handle({{"jsonrpc", "2.0"},
                   {"method", "textDocument/didOpen"},
                   {"params",
                    {{"textDocument",
                      {{"uri", uri_from_path(kLocal)}, {"text", "(/units.knight.speed 3)\n"}}}}}},
                  {});
    const auto rename = [&](const std::string& name) {
        return request(2, "textDocument/rename",
                       {{"textDocument", {{"uri", uri_from_path(kLocal)}}},
                        {"position", {{"line", 0}, {"character", 17}}},
                        {"newName", name}});
    };
    json edit = rename("pace")["result"]["changes"];
    REQUIRE(edit[uri_from_path(kLocal)].size() == 1);
    CHECK(edit[uri_from_path(kLocal)][0]["newText"] == "pace");
    CHECK(edit[uri_from_path(kLocal)][0]["range"]["start"] == json{{"line", 0}, {"character", 15}});
    CHECK(edit[uri_from_path(kDefault)].size() == 2);
    CHECK(edit[uri_from_path(kDefault)][0]["newText"] == "pace");
    edit = rename("top.speed")["result"]["changes"];
    CHECK(edit[uri_from_path(kLocal)][0]["newText"] == "'top.speed'");
    CHECK(edit[uri_from_path(kDefault)][0]["newText"] == "top.speed");  // a tag needs no quote
    const json refused = rename("top speed");
    CHECK(refused.contains("error"));
}

TEST_CASE("the outline: a row by its whole key") {
    const std::string text = "(/units.knight.health 50)\n(/units.archer (speed 3))\n";
    const auto symbols = outline(read_structure(text), text);
    REQUIRE(symbols.size() == 2);
    CHECK(symbols[0].name == "/units.knight.health");
    CHECK(symbols[0].kind == SymbolKind::Field);
    CHECK(symbols[0].detail == "50");
    CHECK(symbols[1].name == "/units.archer");
    REQUIRE(symbols[1].children.size() == 1);
    CHECK(symbols[1].children[0].name == "speed");
}
