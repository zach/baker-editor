// Which .dta files the extension calls Baker's. Run with `npm test`, after `npm run compile`.
const assert = require("assert");
const path = require("path");
const {isBaker, projectOf} = require("../out/project.js");

const files = new Set([
    path.join("/w", "stacks", "duel", "baker-project.dta"),
    path.join("/w", "stacks", "hearth", "baker-project.dta"),
]);
const exists = (p) => files.has(p);

assert.strictEqual(projectOf(path.join("/w", "stacks", "duel", "data", "default.dta"), exists),
                   path.join("/w", "stacks", "duel"));
assert.strictEqual(projectOf(path.join("/w", "stacks", "hearth", "sim", "x.dta"), exists),
                   path.join("/w", "stacks", "hearth"));
assert.ok(isBaker(path.join("/w", "stacks", "duel", "data", "local.dta"), exists));
assert.ok(isBaker(path.join("/w", "stacks", "duel", "data", "UPPER.DTA"), exists));
assert.ok(!isBaker(path.join("/w", "stats", "survey.dta"), exists));  // another format's .dta
assert.ok(!isBaker(path.join("/w", "stacks", "duel", "notes.txt"), exists));
console.log("project: which files are Baker's, as expected");
