#include "features.hpp"

#include <doctest.h>

#include <algorithm>
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
    const auto h = hover(FileRead(text), at, *f.loaded, f.reference);
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
    const auto h = hover(FileRead(text), at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "{game.spawn symbol [int]}"));
}

TEST_CASE("hover on a macro, and on nothing") {
    Fixture f;
    auto [text, at] = cursor("(units (knight (speed SPE|EDY)))");
    auto h = hover(FileRead(text), at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "macro `SPEEDY`"));
    std::tie(text, at) = cursor("(units  |  (knight))");
    CHECK(!hover(FileRead(text), at, *f.loaded, f.reference));
}

TEST_CASE("completion offers every field of the enclosing shape") {
    Fixture f;
    auto [text, at] = cursor("(units (archer (|");
    CHECK(labels(complete(FileRead(text), at, *f.loaded, f.reference)) ==
          std::vector<std::string>{"health", "speed"});
    std::tie(text, at) = cursor("(units (archer (he|");
    const Completions c = complete(FileRead(text), at, *f.loaded, f.reference);
    CHECK(labels(c) == std::vector<std::string>{"health"});
    CHECK(c.begin == text.size() - 2);  // replaces "he"
    CHECK(c.items[0].detail == "int");
}

TEST_CASE("completion offers functions in a command, macros as values, packs in requires") {
    Fixture f;
    auto [text, at] = cursor("(start {game.|");
    const std::vector<std::string> functions =
        labels(complete(FileRead(text), at, *f.loaded, f.reference));
    CHECK(functions == std::vector<std::string>{"game.spawn", "game.wait", "game.log", "game.later",
                                                "game.aim", "game.twice"});
    std::tie(text, at) = cursor("(units (archer (speed S|");
    CHECK(labels(complete(FileRead(text), at, *f.loaded, f.reference)) ==
          std::vector<std::string>{"SPEEDY"});
    std::tie(text, at) = cursor("(pack extra (level task) (requires |");
    CHECK(labels(complete(FileRead(text), at, *f.loaded, f.reference)) ==
          std::vector<std::string>{"core"});
    std::tie(text, at) = cursor("; (units (archer (|");
    CHECK(complete(FileRead(text), at, *f.loaded, f.reference).items.empty());
}

namespace {
const auto on_disk = [](const std::string& f) {
    std::ifstream in(f);
    return static_cast<bool>(in);
};
}  // namespace

TEST_CASE("definition: from local.dta's override to the default.dta line it shadows") {
    Fixture f("(units (knight (health 50)))\n");
    const std::string local = "(units (knight (health 50)))\n";
    const auto defs =
        definition(FileRead(local), local.find("50"), kLocal, *f.loaded, f.reference, on_disk);
    REQUIRE(defs.size() == 1);
    CHECK(defs[0].file == kDefault);
    CHECK(defs[0].line == 5);

    // And from default.dta, to the override above it.
    const std::string text = f.text_of(kDefault);
    const auto up =
        definition(FileRead(text), text.find("100"), kDefault, *f.loaded, f.reference, on_disk);
    REQUIRE(up.size() == 1);
    CHECK(up[0].file == kLocal);
    CHECK(up[0].line == 1);
}

TEST_CASE("definition: macros, script functions, packs, and files") {
    Fixture f;
    auto [text, at] = cursor("(units (knight (speed SPE|EDY)))");
    auto defs = definition(FileRead(text), at, kLocal, *f.loaded, f.reference, on_disk);
    REQUIRE(defs.size() == 1);
    CHECK(defs[0].file == kDefault);
    CHECK(defs[0].line == 10);

    std::tie(text, at) = cursor("(start {game.tw|ice 3})");
    defs = definition(FileRead(text), at, kLocal, *f.loaded, f.reference, on_disk);
    REQUIRE(defs.size() == 1);
    CHECK(defs[0].line == 11);

    std::tie(text, at) = cursor("(pack extra (level task) (requires co|re))");
    defs = definition(FileRead(text), at, kLocal, *f.loaded, f.reference, on_disk);
    REQUIRE(defs.size() == 1);
    CHECK(defs[0].file == kProject + "/packs/game/core.dta");
    CHECK(defs[0].line == 1);

    const std::string text2 = f.text_of(kDefault);
    defs = definition(FileRead(text2), text2.find("core.dta"), kDefault, *f.loaded, f.reference,
                      on_disk);
    REQUIRE(defs.size() == 1);
    CHECK(defs[0].file == kProject + "/packs/game/core.dta");

    const std::string project = f.text_of(kProject + "/baker-project.dta");
    defs = definition(FileRead(project), project.find("data/default.dta") + 2,
                      kProject + "/baker-project.dta", *f.loaded, f.reference, on_disk);
    REQUIRE(defs.size() == 1);
    CHECK(defs[0].file == kDefault);
    CHECK(definition(FileRead(project), project.find("data/local.dta") + 2,
                     kProject + "/baker-project.dta", *f.loaded, f.reference, on_disk)
              .empty());  // not on disk
}

TEST_CASE("a call on one of the game's objects: its type's methods, and its bases'") {
    Fixture f;
    const auto [text, at] = cursor("#autorun {tower |}\n");
    const Completions c = complete(FileRead(text), at, *f.loaded, f.reference);
    CHECK(labels(c) == std::vector<std::string>{"delay_task", "cancel", "tick"});
    CHECK(c.items[0].kind == Kind::Method);
    CHECK(c.items[0].detail == "{Turret delay_task float values...}");
    const auto [typed, at2] = cursor("#autorun {tower de|}\n");
    CHECK(labels(complete(FileRead(typed), at2, *f.loaded, f.reference)) ==
          std::vector<std::string>{"delay_task"});

    // Turret's cancel, not its base's.
    const auto [call, at3] = cursor("#autorun {tower can|cel 3}\n");
    const auto h = hover(FileRead(call), at3, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "{Turret cancel int}"));
    CHECK(has(h->markdown, "Ends a task."));
    CHECK(has(h->markdown, "method of `Turret`"));
    const auto [object, at4] = cursor("#autorun {tow|er tick}\n");
    const auto o = hover(FileRead(object), at4, *f.loaded, f.reference);
    REQUIRE(o);
    CHECK(has(o->markdown, "object `tower`, which the game names: a `Turret`"));
    CHECK(has(o->markdown, "methods: `delay_task`, `cancel`, `tick`"));
    // Not an object: no methods.
    const auto [other, at5] = cursor("#autorun {game.spawn |}\n");
    const std::vector<std::string> offered =
        labels(complete(FileRead(other), at5, *f.loaded, f.reference));
    CHECK(std::find(offered.begin(), offered.end(), "delay_task") == offered.end());
}

TEST_CASE("hover names a key field's table and a curve field") {
    Fixture f(
        "(shapes (tower (at towers) (keyed) (aim curve) (target (key_of units))))\n"
        "(towers (t1 (target knight)))\n");
    auto [text, at] = cursor("(towers (t1 (tar|get knight)))");
    auto h = hover(FileRead(text), at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "(key_of units)"));
    std::tie(text, at) = cursor("(towers (t1 (ai|m 0 0)))");
    h = hover(FileRead(text), at, *f.loaded, f.reference);
    REQUIRE(h);
    CHECK(has(h->markdown, "curve"));
}
