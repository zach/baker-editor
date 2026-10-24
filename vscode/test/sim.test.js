// Scenarios as tests: the binary, its arguments, and each scenario's verdict from baker.sim/1.
// Run with `npm test`, after `npm run compile`.
const assert = require("assert");
const path = require("path");
const {outcomes, simArgs, toolFor} = require("../out/sim.js");

const suite = (tool) => ({project: "", directory: path.join("/w", "stacks", "duel"), tool,
                          scenarios: [], objectives: []});
const join = path.join, abs = path.isAbsolute;
assert.strictEqual(toolFor(suite("/w/build/duel"), "x", join, abs), "/w/build/duel");
assert.strictEqual(toolFor(suite(""), "/b/bin/stacks_${project}", join, abs), "/b/bin/stacks_duel");
assert.strictEqual(toolFor(suite(""), "build/game", join, abs),
                   path.join("/w", "stacks", "duel", "build", "game"));
assert.strictEqual(toolFor(suite(""), "", join, abs), "");
assert.deepStrictEqual(simArgs(["a", "b"]), ["sim", "--baker-scenario", "a", "--baker-scenario", "b", "--baker-json"]);

const results = {
    schema: "baker.sim/1", status: "failed",
    runs: [
        {scenario: "walk", seed: 1, passed: true, errors: [], objectives: [
            {name: "steps", file: "/p/m.dta", line: 2, value: 120, pass: true, over: "seed", error: null},
            {name: "scored", file: "/p/m.dta", line: 4, value: 0, pass: false, over: "median", error: null}]},
        {scenario: "run", seed: 1, passed: false, errors: [
            {code: "Undefined", file: "/p/s.dta", line: 9, column: 4, message: "no such function"}],
         objectives: [{name: "steps", file: "/p/m.dta", line: 2, value: null, pass: false,
                       over: "seed", error: {message: "measure gave no number"}}]},
    ],
    medians: [{scenario: "walk", objective: "scored", file: "/p/m.dta", line: 4, value: 10,
               seeds: 2, pass: true, error: null}],
};
const o = outcomes(results);
assert.strictEqual(o.get("walk").passed, true);  // a median objective failing in one run is not a failure
assert.strictEqual(o.get("run").passed, false);
assert.deepStrictEqual(o.get("run").messages.map((m) => m.text),
                       ["seed 1: no such function", "steps failed (seed 1): measure gave no number"]);
assert.strictEqual(o.get("run").messages[1].line, 2);
results.medians[0].pass = false;
assert.strictEqual(outcomes(results).get("walk").passed, false);
assert.strictEqual(outcomes(results).get("walk").messages[0].text,
                   "scored failed: median 10 over 2 seeds");
console.log("sim: scenarios as tests, as expected");
