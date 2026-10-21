#!/usr/bin/env node
// A game binary for the test view's tests: answers `sim [--scenario NAME]... --json` for the
// test project's scenarios with a fixed baker.sim/1, as Baker's host kit writes it. A file
// named `fail` in the project's directory, or BREAK in data/default.dta on disk, makes
// units.alive fail in `opening`.
const fs = require("fs");
const path = require("path");

const args = process.argv.slice(2);
if (args[0] !== "sim" || !args.includes("--json")) {
    process.stderr.write("fake-game: only `sim ... --json`\n");
    process.exit(2);
}
const asked = [];
for (let i = 1; i < args.length; ++i)
    if (args[i] === "--scenario") asked.push(args[++i]);
const dir = process.cwd();
const scenarios = path.join(dir, "scenarios", "basic.dta");
const objectives = path.join(dir, "metrics", "units.dta");
// The data on disk breaking it, as an edit that has been saved would.
const fail = fs.existsSync(path.join(dir, "fail")) ||
             fs.readFileSync(path.join(dir, "data", "default.dta"), "utf8").includes("BREAK");
const lines = {opening: 2, long: 6};
const runs = (asked.length ? asked : ["long", "opening"]).sort().map((scenario) => ({
    scenario, file: scenarios, line: lines[scenario], seed: 1, steps: 60,
    passed: !(fail && scenario === "opening"), errors: [],
    objectives: scenario === "opening" ? [{
        name: "units.alive", file: objectives, line: 2, value: fail ? 0 : 2,
        pass: !fail, over: "seed", error: null}] : [],
}));
const failed = runs.some((r) => !r.passed);
process.stdout.write(JSON.stringify({schema: "baker.sim/1", status: failed ? "failed" : "passed",
                                     runs, medians: [], summary: []}, null, 1) + "\n");
process.exit(failed ? 1 : 0);
