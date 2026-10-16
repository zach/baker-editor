#include "features.hpp"

#include <doctest.h>

#include <fstream>
#include <sstream>

using namespace baker_lsp;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kDefault = kProject + "/data/default.dta";
const std::string kLocal = kProject + "/data/local.dta";

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
        setup.objects = reference.objects;
        if (!local.empty()) buffers.set(kLocal, local);
        loaded = diagnose(setup, ctx, buffers, true).loaded;
        REQUIRE(loaded);
    }
    std::string text_of(const std::string& path) {
        std::string t;
        REQUIRE(buffers.read(path, t));
        return t;
    }
};

// The cursor is at the '|', which is taken out.
std::pair<std::string, size_t> cursor(std::string text) {
    const size_t bar = text.find('|');
    text.erase(bar, 1);
    return {text, bar};
}

std::vector<std::string> labels(const Completions& c) {
    std::vector<std::string> out;
    for (const Completion& item : c.items) out.push_back(item.label);
    return out;
}

bool has(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("the shape governing a keyed table's child") {
    Fixture f;
    const baker::shape::Shape* s = governing(f.loaded->shapes, {"units", "knight"});
    REQUIRE(s);
    CHECK(s->name.str() == "unit");
    CHECK(!governing(f.loaded->shapes, {"units"}));
    CHECK(!governing(f.loaded->shapes, {"other", "knight"}));
}

TEST_CASE("hover on a value overridden in local.dta shows both layers") {
    Fixture f("(units (knight (health 50)))\n");
    const std::string text = f.text_of(kDefault);
    const size_t at = text.find("100");
    const auto h = hover(text, at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "`units/knight/health`"));
    CHECK(has(h->markdown, "`int`, 1 to 1000"));
    CHECK(has(h->markdown, "| default | `100` | default.dta:5 | shadowed |"));
    CHECK(has(h->markdown, "| local | `50` | local.dta:1 | wins |"));
    CHECK(text.substr(h->begin, h->end - h->begin) == "100");
}

TEST_CASE("hover on a command's head shows the function") {
    Fixture f;
    const auto [text, at] = cursor("(start {game.sp|awn archer 1})");
    const auto h = hover(text, at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "{game.spawn symbol [int]}"));
}

TEST_CASE("hover on a macro, and on nothing") {
    Fixture f;
    auto [text, at] = cursor("(units (knight (speed SPE|EDY)))");
    auto h = hover(text, at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "macro `SPEEDY`"));
    std::tie(text, at) = cursor("(units  |  (knight))");
    CHECK(!hover(text, at, *f.loaded, f.reference));
}

TEST_CASE("completion offers every field of the enclosing shape") {
    Fixture f;
    auto [text, at] = cursor("(units (archer (|");
    CHECK(labels(complete(text, at, *f.loaded, f.reference)) ==
          std::vector<std::string>{"health", "speed"});
    std::tie(text, at) = cursor("(units (archer (he|");
    const Completions c = complete(text, at, *f.loaded, f.reference);
    CHECK(labels(c) == std::vector<std::string>{"health"});
    CHECK(c.begin == text.size() - 2);  // replaces "he"
    CHECK(c.items[0].detail == "int");
}

TEST_CASE("completion offers functions in a command, macros as values, packs in requires") {
    Fixture f;
    auto [text, at] = cursor("(start {game.|");
    const std::vector<std::string> functions = labels(complete(text, at, *f.loaded, f.reference));
    CHECK(functions == std::vector<std::string>{"game.spawn", "game.wait", "game.log", "game.later",
                                                "game.aim"});
    std::tie(text, at) = cursor("(units (archer (speed S|");
    CHECK(labels(complete(text, at, *f.loaded, f.reference)) == std::vector<std::string>{"SPEEDY"});
    std::tie(text, at) = cursor("(pack extra (level task) (requires |");
    CHECK(labels(complete(text, at, *f.loaded, f.reference)) == std::vector<std::string>{"core"});
    std::tie(text, at) = cursor("; (units (archer (|");
    CHECK(complete(text, at, *f.loaded, f.reference).items.empty());
}
