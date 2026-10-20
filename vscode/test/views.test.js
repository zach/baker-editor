// Where a line of a view comes from. Run with `npm test`, after `npm run compile`.
const assert = require("assert");
const {sourceAt, viewPath} = require("../out/views.js");

const view = {
    text: "(a\n  (b 1)  ; default data/default.dta:3\n  (c 2))  ; local data/local.dta:1\n",
    sources: [
        {line: 1, uri: "file:///p/data/default.dta", fileLine: 3},
        {line: 2, uri: "file:///p/data/local.dta", fileLine: 1},
    ],
};
assert.strictEqual(sourceAt(view, 0), undefined);
assert.strictEqual(sourceAt(view, 1).fileLine, 3);
assert.strictEqual(sourceAt(view, 2).uri, "file:///p/data/local.dta");
assert.strictEqual(sourceAt(view, 3), undefined);
assert.strictEqual(sourceAt({text: "", sources: []}, 0), undefined);
assert.strictEqual(viewPath("/w/stacks/duel"), "/duel/merged.dta");
assert.strictEqual(viewPath("C:\\w\\duel\\", "agent"), "/duel/agent.changes");
console.log("views: each line's source, as expected");
