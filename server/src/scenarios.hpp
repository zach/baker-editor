#pragma once
// A project's scenarios and objectives (Baker's host kit: the project file's `(scenarios ...)`
// and `(objectives ...)`, run by the game's `sim`), for an editor's test view: what there is
// to run, where each is declared, and what a run's results (baker.sim/1) mean for the files.
// The editor runs the game's binary itself; the server only lists and reports.
#include "diagnose.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace baker_lsp {

struct Declared {
    std::string name;
    std::string file;  // canonical
    int line = 0;      // 1-based
    int column = 0;    // 1-based, in code points: its name's
    std::string desc;
    std::vector<std::string> objectives;  // a scenario's; empty: judged by every objective
    bool median = false;                  // an objective judged over the seeds: (over median)
};

struct Suite {
    std::string project;  // the project file, canonical
    std::string tool;     // the binary that answers the commands, canonical; empty if unnamed
    std::vector<Declared> scenarios, objectives;
};

// The scenarios and objectives the project file names, read from the open editors' text or
// the disk: each file named, or each .dta file of a directory named, in name order. False,
// with `error`, if the project file does not read.
bool read_suite(const std::string& project, const Buffers& buffers, Suite& out, std::string& error);

// What a sim's results (baker.sim/1) say is wrong, located: an objective that failed, on its
// declaration and on its scenario's; a command that failed, where it is written. The runs'
// `scenario` names are the ones results cover.
std::vector<Problem> sim_problems(const nlohmann::json& results, const Suite& suite);

}  // namespace baker_lsp
