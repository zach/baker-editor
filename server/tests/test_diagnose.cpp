#include "diagnose.hpp"
#include "reference.hpp"

#include <doctest.h>

#include <fstream>
#include <sstream>

using namespace baker_lsp;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kDefault = kProject + "/data/default.dta";
const std::string kLocal = kProject + "/data/local.dta";  // optional, not on disk
const std::string kScenario = kProject + "/sim/opening.dta";
const std::string kProjectPath = kProject + "/baker-project.dta";

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

// The same stack, from the project file.
Setup from_file() {
    Setup s;
    s.project = kProjectPath;
    s.reference = kProject + "/reference.json";
    return s;
}

struct Fixture {
    Setup setup = project();
    baker::Context ctx;
    Buffers buffers;

    Reference reference;

    Fixture() {
        std::string error;
        REQUIRE(load_reference(ctx, setup.reference, reference, error));
        CHECK(reference.functions == 5);
        CHECK(reference.widened == 1);  // game.aim takes a vec2, a type of the game's own
        CHECK(reference.failed.empty());
        setup.objects = reference.objects;
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
    REQUIRE(d.problems.size() == 2);
    const Problem& p = d.problems[1];
    CHECK(p.file == kLocal);
    CHECK(p.line == 1);
    CHECK(p.severity == Severity::Error);
    CHECK(p.key == "health");
    CHECK(p.message.find("5000") != std::string::npos);
    // The value it overrides is shadowed: a hint, shown faded.
    const Problem& shadowed = d.problems[0];
    CHECK(shadowed.file == kDefault);
    CHECK(shadowed.line == 5);
    CHECK(shadowed.severity == Severity::Hint);
    CHECK(shadowed.unnecessary);
    CHECK(shadowed.message == "shadowed: local.dta:1 sets it to 5000");
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
    CHECK(f.reference.objects == std::vector<std::string>{"clock"});
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
    Reference r;
    std::string error;
    CHECK(!load_reference(ctx, kDefault, r, error));
    CHECK(error.find("not a baker.reference/1") != std::string::npos);
    CHECK(!load_reference(ctx, kProject + "/none.json", r, error));
}

TEST_CASE("a literal argument of the wrong type is an error") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.spawn 3}"));
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].code == "type");
    CHECK(d.problems[0].key == "game.spawn");
}

TEST_CASE("a parameter of the game's own type takes any value, and is still counted") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.aim (1 2)}"));
    CHECK(f.run().problems.empty());
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.aim}"));
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].key == "game.aim");
}

TEST_CASE("the project file gives the stack the command line gives") {
    Fixture f;
    f.setup = from_file();
    Diagnosis d = f.run();
    CHECK(d.problems.empty());
    CHECK(d.files.size() == 3);  // the project file, default.dta and its pack

    f.buffers.set(kLocal, "(units (knight (health 5000)))\n");
    d = f.run();
    REQUIRE(d.problems.size() == 2);  // and the shadowed (health 100) first
    CHECK(d.problems[1].file == kLocal);
    CHECK(d.problems[1].key == "health");
}

TEST_CASE("an error in the project file is located in it, from its buffer") {
    Fixture f;
    f.setup = from_file();
    f.buffers.set(kProjectPath,
                  "(project\n   (layers (default \"data/default.dta\"))\n   (colour red))\n");
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].file == kProjectPath);
    CHECK(d.problems[0].line == 3);
}

TEST_CASE("a missing layer is reported at the project entry that names it") {
    Fixture f;
    f.setup = from_file();
    f.buffers.set(kProjectPath, "(project\n   (layers (default \"data/none.dta\")))\n");
    const Diagnosis d = f.run();
    REQUIRE(!d.problems.empty());
    CHECK(d.problems[0].file == kProjectPath);
    CHECK(d.problems[0].severity == Severity::Error);
    CHECK(d.problems[0].line == 2);
    CHECK(d.problems[0].column == 21);  // the path's opening quote
}

TEST_CASE("a layer that does not read does not stop the layers above it") {
    Fixture f;
    f.setup = from_file();
    f.buffers.set(kDefault, replaced(kDefault, "(speed 2.5))", "(speed 2.5)"));
    f.buffers.set(kLocal, "(units (knight (health 5))\n");
    const Diagnosis d = f.run();
    bool in_default = false, in_local = false;
    for (const Problem& p : d.problems) {
        in_default = in_default || (p.file == kDefault && p.code == "syntax");
        in_local = in_local || (p.file == kLocal && p.code == "syntax");
    }
    CHECK(in_default);
    CHECK(in_local);
}

TEST_CASE("skipped and disabled layers are left out of the merge") {
    Fixture f;
    f.setup = from_file();
    f.setup.skip = {"local"};
    f.buffers.set(kLocal, "(units (knight (health 5000)))\n");
    CHECK(f.run().problems.empty());

    f.setup.skip.clear();
    f.buffers.set(kProjectPath,
                  "(project (pack_levels engine game task) (layers (default "
                  "\"data/default.dta\") (local \"data/local.dta\" optional disabled)))\n");
    CHECK(f.run().problems.empty());
}
