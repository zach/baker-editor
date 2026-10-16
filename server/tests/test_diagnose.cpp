#include "diagnose.hpp"
#include "stubs.hpp"

#include <doctest.h>

#include <fstream>
#include <sstream>

using namespace baker_lsp;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kDefault = kProject + "/data/default.dta";
const std::string kLocal = kProject + "/data/local.dta";  // optional, not on disk
const std::string kScenario = kProject + "/sim/opening.dta";

Setup project() {
    Setup s;
    s.layers = {{"default", kDefault, false}, {"local", kLocal, true}};
    s.pack_levels = {"engine", "game", "task"};
    s.include_dirs = {kProject + "/data"};
    s.reference = kProject + "/reference.json";
    return s;
}

std::string file_text(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return std::move(s).str();
}

std::string replaced(const std::string& path, const std::string& from, const std::string& to) {
    std::string text = file_text(path);
    const size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

struct Fixture {
    Setup setup = project();
    baker::Context ctx;
    Buffers buffers;

    Fixture() {
        StubCount count;
        std::string error;
        REQUIRE(register_stubs(ctx, setup.reference, count, error));
        CHECK(count.functions == 4);
    }
    Diagnosis run() { return diagnose(setup, ctx, buffers, true); }
};

const Problem* find(const Diagnosis& d, const std::string& code) {
    for (const Problem& p : d.problems)
        if (p.code == code) return &p;
    return nullptr;
}

}  // namespace

TEST_CASE("the project as it is on disk has no problems") {
    Fixture f;
    const Diagnosis d = f.run();
    CHECK(d.problems.empty());
    CHECK(d.files.size() == 2);  // default.dta and the pack it requires
}

TEST_CASE("an open buffer's text is checked instead of the file's") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "(speed 2.5))", "(speed 2.5)"));
    const Diagnosis d = f.run();
    const Problem* p = find(d, "syntax");
    REQUIRE(p);
    CHECK(p->file == kDefault);
    CHECK(p->line > 0);
    CHECK(p->column > 0);
}

TEST_CASE("an unsaved optional layer is read from its buffer") {
    Fixture f;
    f.buffers.set(kLocal, "(units (knight (health 5000)))\n");
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    const Problem& p = d.problems[0];
    CHECK(p.file == kLocal);
    CHECK(p.line == 1);
    CHECK(p.severity == Severity::Error);
    CHECK(p.key == "health");
    CHECK(p.message.find("5000") != std::string::npos);
}

TEST_CASE("a field no shape declares is information") {
    Fixture f;
    f.buffers.set(kLocal, "(units (archer (armor 2)))\n");
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].severity == Severity::Information);
    CHECK(d.problems[0].key == "armor");
}

TEST_CASE("calls are counted against the reference, in #autorun too") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.spawn knight 2 3}"));
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].code == "type");
    CHECK(d.problems[0].key == "game.spawn");
    CHECK(d.problems[0].file == kDefault);
}

TEST_CASE("a call to an undefined function is an error; a stub is not") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}",
                                     "{game.spwan knight 2} {game.later} {game.log 1 2 3 4}"));
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].code == "undefined");
    CHECK(d.problems[0].key == "game.spwan");
}

TEST_CASE("without a reference, undefined calls are not reported") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.spwan knight}"));
    CHECK(diagnose(f.setup, f.ctx, f.buffers, false).problems.empty());
}

TEST_CASE("a named object's calls are not undefined functions") {
    Fixture f;
    f.setup.objects = {"clock"};
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{clock tick 1}"));
    CHECK(f.run().problems.empty());
}

TEST_CASE("an open file outside the layers is checked beside them") {
    Fixture f;
    f.buffers.set(kScenario, "(start {game.wait} {game.spawn archer 1})\n");
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].file == kScenario);
    CHECK(d.problems[0].key == "game.wait");

    f.buffers.set(kScenario, "(start {game.wait 1}\n");
    const Diagnosis broken = f.run();
    const Problem* p = find(broken, "syntax");
    REQUIRE(p);
    CHECK(p->file == kScenario);
}

TEST_CASE("a missing required layer is an error without a file") {
    Fixture f;
    f.setup.layers[0].path = kProject + "/data/nothing.dta";
    const Diagnosis d = f.run();
    REQUIRE(!d.problems.empty());
    CHECK(d.problems[0].severity == Severity::Error);
}

TEST_CASE("the reference must be one") {
    baker::Context ctx;
    StubCount count;
    std::string error;
    CHECK(!register_stubs(ctx, kDefault, count, error));
    CHECK(error.find("not a baker.reference/1") != std::string::npos);
    CHECK(!register_stubs(ctx, kProject + "/none.json", count, error));
}
