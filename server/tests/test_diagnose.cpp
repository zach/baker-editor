#include "diagnose.hpp"
#include "reference.hpp"

#include <baker/std_interop.hpp>

#include <doctest.h>

#include <algorithm>
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
        CHECK(reference.functions == 6);
        CHECK(reference.widened == 0);  // game.aim's vec2 says its values (1.2)
        CHECK(reference.failed.empty());
        setup.objects = reference.objects;
        setup.types = reference.declared_types;
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
    CHECK(f.reference.objects == std::vector<std::string>{"clock", "tower"});
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

TEST_CASE("a parameter of the game's own type is held to its values, and counted") {
    Fixture f;
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.aim (1 2)}"));
    CHECK(f.run().problems.empty());
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.aim (1 up)}"));
    const Diagnosis wrong = f.run();
    REQUIRE(wrong.problems.size() == 1);
    CHECK(wrong.problems[0].message.find("it takes (vec2)") != std::string::npos);
    CHECK(wrong.problems[0].key == "game.aim");
    f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", "{game.aim}"));
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].key == "game.aim");
}

TEST_CASE("a list parameter's literal argument is held to its element (baker.reference/1.4)") {
    Fixture f;
    f.buffers.set(kDefault,
                  replaced(kDefault, "{game.spawn knight 2}", "{game.patrol ((0 0) (4 4))}"));
    CHECK(f.run().problems.empty());
    f.buffers.set(kDefault,
                  replaced(kDefault, "{game.spawn knight 2}", "{game.patrol ((0 0) (4))}"));
    const Diagnosis d = f.run();
    REQUIRE(d.problems.size() == 1);
    CHECK(d.problems[0].key == "game.patrol");
}

TEST_CASE("the project's scenarios and objectives are checked, open or not, as the game's check") {
    Fixture f;
    f.setup = from_file();
    const std::string scenarios = kProject + "/scenarios/basic.dta";
    // Not open: read from the disk, as the game's check reads it.
    const Diagnosis on_disk = f.run();
    CHECK(std::find(on_disk.files.begin(), on_disk.files.end(), scenarios) != on_disk.files.end());
    CHECK(std::find(on_disk.files.begin(), on_disk.files.end(), kProject + "/metrics/units.dta") !=
          on_disk.files.end());
    // Its error is located in it.
    std::string text;
    REQUIRE(Buffers().read(scenarios, text));
    f.buffers.set(scenarios, text + "(scenario broken (seconds 1) (start {game.nosuch 1}))\n");
    const Diagnosis d = f.run();
    const Problem* p = find(d, "undefined");
    REQUIRE(p);
    CHECK(p->file == scenarios);
    CHECK(p->key == "game.nosuch");
}

TEST_CASE("a call on an object the game names is held to its type's methods") {
    Fixture f;
    // Ticker and Turret, the game's own, declared; Scheduler is Baker's, known already.
    CHECK(f.reference.declared_types == std::vector<std::string>{"Ticker", "Turret"});
    CHECK(f.reference.declared_objects == 2);  // clock, of Baker's Scheduler, and tower
    const auto problems = [&f](const std::string& call) {
        f.buffers.set(kDefault, replaced(kDefault, "{game.spawn knight 2}", call));
        std::vector<std::string> out;
        for (const Problem& p : f.run().problems) out.push_back(p.code + " " + p.message);
        return out;
    };
    CHECK(problems("{tower delay_task 2 {game.wait 1}}").empty());
    CHECK(problems("{tower tick}").empty());  // its base's
    const std::vector<std::string> few = problems("{tower delay_task}");
    REQUIRE(few.size() == 1);
    CHECK(few[0].find("delay_task") != std::string::npos);
    const std::vector<std::string> typed = problems("{tower cancel soon}");
    REQUIRE(typed.size() == 1);
    CHECK(typed[0].starts_with("type "));
    CHECK(problems("{tower cancle 3}") ==
          std::vector<std::string>{
              "undefined 'Turret' has no method 'cancle' in the game's reference"});
}

TEST_CASE("the project file gives the stack the command line gives") {
    Fixture f;
    f.setup = from_file();
    Diagnosis d = f.run();
    CHECK(d.problems.empty());
    // The project file, default.dta and its pack, and its scenarios and objectives.
    CHECK(d.files.size() == 5);

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

namespace {

// A check that reloads the kept stack finds what a fresh one finds.
void same_as_fresh(Fixture& f, Kept& kept) {
    const Diagnosis again = diagnose(f.setup, f.ctx, f.buffers, true, &kept);
    const Diagnosis fresh = f.run();
    CHECK(again.problems == fresh.problems);
    CHECK(again.files == fresh.files);
    CHECK(again.every_layer_read == fresh.every_layer_read);
    REQUIRE(static_cast<bool>(again.loaded) == static_cast<bool>(fresh.loaded));
    if (!fresh.loaded) return;
    const Loaded& a = *again.loaded;
    const Loaded& b = *fresh.loaded;
    CHECK(a.hints == b.hints);
    CHECK(a.files == b.files);
    const auto names = [](const std::vector<Definition>& v) {
        std::vector<std::string> out;
        for (const Definition& d : v)
            out.push_back(d.name + "@" + d.file + ":" + std::to_string(d.line));
        return out;
    };
    CHECK(names(a.packs) == names(b.packs));
    CHECK(names(a.macros) == names(b.macros));
    CHECK(names(a.functions) == names(b.functions));
    REQUIRE(a.config->root());
    CHECK(baker::to_string(*a.config->root()) == baker::to_string(*b.config->root()));
}

}  // namespace

TEST_CASE("a kept stack is reloaded, and checks as a fresh load does") {
    Fixture f;
    f.setup = from_file();
    Kept kept;
    same_as_fresh(f, kept);
    REQUIRE(kept.config);
    const baker::Config* first = kept.config.get();

    // An edit to a layer reloads the same Config.
    f.buffers.set(kLocal, "(units (knight (health 5000)))\n");
    same_as_fresh(f, kept);
    f.buffers.set(kLocal, "(units (knight (health 7)))\n");
    same_as_fresh(f, kept);
    CHECK(kept.config.get() == first);
    f.buffers.set(kDefault, replaced(kDefault, "(speed 2.5)", "(speed fast)"));
    same_as_fresh(f, kept);
    f.buffers.erase(kDefault);

    // A layer that does not read: fresh loads, each time, and the kept stack waits.
    f.buffers.set(kLocal, "(units (knight (health 5))\n");
    same_as_fresh(f, kept);
    same_as_fresh(f, kept);
    CHECK(kept.config.get() == first);
    f.buffers.set(kLocal, "(units (knight (health 6)))\n");
    same_as_fresh(f, kept);
    CHECK(kept.config.get() == first);

    // Nothing changed in the stack: an open scenario is still checked beside it.
    f.buffers.set(kScenario, "(scenario {nope})\n");
    same_as_fresh(f, kept);
    f.buffers.erase(kScenario);

    // The optional layer's file gone again.
    f.buffers.erase(kLocal);
    same_as_fresh(f, kept);
    CHECK(kept.config.get() == first);

    // A change to the project's layers opens the stack again.
    f.buffers.set(
        kProjectPath,
        "(project (pack_levels engine game task) (include_dirs \"data\") (layers "
        "(default \"data/default.dta\") (local \"data/local.dta\" optional disabled)))\n");
    f.buffers.set(kLocal, "(units (knight (health 5000)))\n");
    same_as_fresh(f, kept);
    CHECK(kept.config.get() != first);
    f.setup.skip = {"local"};
    same_as_fresh(f, kept);
}
