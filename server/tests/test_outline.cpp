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
