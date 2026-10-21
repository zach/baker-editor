#include "positions.hpp"
#include "scenarios.hpp"
#include "server.hpp"

#include <doctest.h>

#include <vector>

using namespace baker_lsp;
using nlohmann::json;

namespace {

const std::string kProject = canonical_path(BAKER_LSP_TEST_DATA "/project");
const std::string kProjectFile = kProject + "/baker-project.dta";
const std::string kScenarios = kProject + "/scenarios/basic.dta";
const std::string kObjectives = kProject + "/metrics/units.dta";

// A sim's results, as baker.sim/1 has them: `opening` failed units.alive in seed 2, and
// units.fair failed by its median.
json results() {
    const json alive_fails = {
        {"name", "units.alive"}, {"file", kObjectives}, {"line", 2},       {"value", 0},
        {"pass", false},         {"over", "seed"},      {"error", nullptr}};
    json alive_passes = alive_fails;
    alive_passes["value"] = 2;
    alive_passes["pass"] = true;
    return {{"schema", "baker.sim/1"},
            {"status", "failed"},
            {"runs",
             {{{"scenario", "opening"},
               {"file", kScenarios},
               {"line", 2},
               {"seed", 1},
               {"passed", true},
               {"errors", json::array()},
               {"objectives", {alive_passes}}},
              {{"scenario", "opening"},
               {"file", kScenarios},
               {"line", 2},
               {"seed", 2},
               {"passed", false},
               {"errors",
                {{{"code", "Undefined"},
                  {"file", kScenarios},
                  {"line", 4},
                  {"column", 12},
                  {"message", "'game.spawn' failed"}}}},
               {"objectives", {alive_fails}}}}},
            {"medians",
             {{{"scenario", "long"},
               {"objective", "units.fair"},
               {"file", kObjectives},
               {"line", 5},
               {"value", 12},
               {"seeds", 2},
               {"pass", false},
               {"error", nullptr}}}}};
}

}  // namespace

TEST_CASE("the scenarios and objectives the project file names, where each is declared") {
    Suite suite;
    std::string error;
    REQUIRE(read_suite(kProjectFile, Buffers(), suite, error));
    CHECK(suite.tool.empty());  // the project names no binary
    REQUIRE(suite.scenarios.size() == 2);
    CHECK(suite.scenarios[0].name == "opening");
    CHECK(suite.scenarios[0].file == kScenarios);
    CHECK(suite.scenarios[0].line == 2);
    CHECK(suite.scenarios[0].column == 11);  // (scenario opening
    CHECK(suite.scenarios[0].desc == "Two archers take the field");
    CHECK(suite.scenarios[0].objectives == std::vector<std::string>{"units.alive"});
    CHECK(suite.scenarios[1].name == "long");
    REQUIRE(suite.objectives.size() == 2);
    CHECK(!suite.objectives[0].median);
    CHECK(suite.objectives[1].median);

    // An open editor's text, and a new file not yet saved, are what is read.
    Buffers buffers;
    buffers.set(kScenarios, "(scenario renamed (seconds 1))\n");
    buffers.set(kProject + "/scenarios/new.dta", "(scenario fresh (seconds 1))\n");
    buffers.set(kProjectFile,
                "(project (layers (default \"data/default.dta\"))\n"
                "   (tool \"build/game\") (scenarios \"scenarios\"))\n");
    Suite open;
    REQUIRE(read_suite(kProjectFile, buffers, open, error));
    CHECK(open.tool == kProject + "/build/game");
    REQUIRE(open.scenarios.size() == 2);
    CHECK(open.scenarios[0].name == "renamed");
    CHECK(open.scenarios[1].name == "fresh");
    CHECK(open.objectives.empty());
}

TEST_CASE("a failed objective is shown on its declaration and on its scenario") {
    Suite suite;
    std::string error;
    REQUIRE(read_suite(kProjectFile, Buffers(), suite, error));
    std::vector<std::string> found;
    for (const Problem& p : sim_problems(results(), suite))
        found.push_back(p.file.substr(kProject.size() + 1) + ":" + std::to_string(p.line) + " " +
                        p.message);
    for (const Problem& p : sim_problems(results(), suite))
        if (p.message.starts_with("units.alive")) CHECK(p.column > 1);  // on the name
    CHECK(found ==
          std::vector<std::string>{
              "metrics/units.dta:2 units.alive failed in scenario opening (seed 2): value 0 "
              "(Someone is left standing)",
              "metrics/units.dta:5 units.fair failed in scenario long (median over 2 seeds): "
              "value 12",
              "scenarios/basic.dta:2 units.alive failed in scenario opening (seed 2): value 0 "
              "(Someone is left standing)",
              "scenarios/basic.dta:4 in scenario opening (seed 2): 'game.spawn' failed",
              "scenarios/basic.dta:6 units.fair failed in scenario long (median over 2 seeds): "
              "value 12"});
}

TEST_CASE("over LSP: the suite, and a run's failures published until it runs again") {
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
    const json suite = request("baker/scenarios", {{"uri", uri_from_path(kProjectFile)}})["result"];
    CHECK(suite["project"] == uri_from_path(kProjectFile));
    CHECK(suite["directory"] == kProject);
    CHECK(suite["tool"] == "");
    REQUIRE(suite["scenarios"].size() == 2);
    CHECK(suite["scenarios"][0]["uri"] == uri_from_path(kScenarios));
    CHECK(suite["scenarios"][0]["line"] == 1);  // 0-based, as LSP's

    const auto sim_diagnostics = [&](const std::string& file) {
        sent.clear();
        server.check_now();
        std::vector<std::string> out;
        for (const json& m : sent)
            if (m.value("method", "") == "textDocument/publishDiagnostics" &&
                m["params"]["uri"] == uri_from_path(file))
                for (const json& d : m["params"]["diagnostics"])
                    if (d["source"] == "baker sim") out.push_back(d["message"]);
        return out;
    };
    server.handle({{"jsonrpc", "2.0"},
                   {"method", "baker/simResults"},
                   {"params", {{"uri", uri_from_path(kProjectFile)}, {"results", results()}}}},
                  {});
    CHECK(server.due());
    CHECK(sim_diagnostics(kObjectives).size() == 2);
    // `opening` runs again and passes: its failures go, `long`'s stay.
    json passed = results();
    passed["runs"].erase(1);
    passed["medians"] = json::array();
    server.handle({{"jsonrpc", "2.0"},
                   {"method", "baker/simResults"},
                   {"params", {{"uri", uri_from_path(kProjectFile)}, {"results", passed}}}},
                  {});
    const std::vector<std::string> left = sim_diagnostics(kObjectives);
    REQUIRE(left.size() == 1);
    CHECK(left[0].starts_with("units.fair failed in scenario long"));
}
