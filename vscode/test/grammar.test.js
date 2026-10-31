// Tokenizes Baker text with the extension's grammar, as VS Code does, and checks the scopes
// of the tokens that matter. Run with `npm test`.
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const oniguruma = require("vscode-oniguruma");
const textmate = require("vscode-textmate");

const wasm = fs.readFileSync(require.resolve("vscode-oniguruma/release/onig.wasm")).buffer;
const onig = oniguruma.loadWASM(wasm).then(() => ({
    createOnigScanner: (patterns) => new oniguruma.OnigScanner(patterns),
    createOnigString: (s) => new oniguruma.OnigString(s),
}));
const registry = new textmate.Registry({
    onigLib: onig,
    loadGrammar: async () =>
        textmate.parseRawGrammar(
            fs.readFileSync(path.join(__dirname, "..", "syntaxes", "baker.tmLanguage.json"), "utf8"),
            "baker.tmLanguage.json"),
});

// The scopes of each token of `text`, as [token, innermost scope].
function tokens(grammar, text) {
    const out = [];
    let state = textmate.INITIAL;
    for (const line of text.split("\n")) {
        const r = grammar.tokenizeLine(line, state);
        for (const t of r.tokens) {
            const s = line.slice(t.startIndex, t.endIndex);
            if (s.trim()) out.push([s.trim(), t.scopes[t.scopes.length - 1]]);
        }
        state = r.ruleStack;
    }
    return out;
}

function scopeOf(list, token) {
    const found = list.find(([t]) => t === token);
    assert.ok(found, `no token ${token} in ${JSON.stringify(list)}`);
    return found[1];
}

(async () => {
    const g = await registry.loadGrammar("source.baker");
    const t = tokens(g, [
        "; a comment (with brackets)",
        '#require "../packs/game/core.dta"',
        "(fighters",
        "   (kestrel (health 1000) (width 0.6) (guard low) (desc \"a (string)\")))",
        "[SPEEDY 9]",
        "#autorun {game.spawn 'bacon and eggs' $count -2.5e3}",
        "(units (knight (speed #remove)))",
        "(paint #00aa00 #1 #x1)",
        "(speed 4.5 (@range 0 10) (@desc \"Run speed\")) (mail @)",
        "(/fighters.kestrel/'max_hp' 1100 (@note \"x\"))",
        "(/ 1 2)",
        "/* a block",
        "   comment */ (mask 0x1F) (say \"a \\q(quoted)\\q\")",
    ].join("\n"));
    assert.strictEqual(scopeOf(t, "; a comment (with brackets)"), "comment.line.semicolon.baker");
    assert.strictEqual(scopeOf(t, "#require"), "keyword.control.directive.baker");
    assert.strictEqual(scopeOf(t, "fighters"), "entity.name.tag.baker");
    assert.strictEqual(scopeOf(t, "health"), "entity.name.tag.baker");
    assert.strictEqual(scopeOf(t, "1000"), "constant.numeric.baker");
    assert.strictEqual(scopeOf(t, "0.6"), "constant.numeric.baker");
    assert.strictEqual(scopeOf(t, "low"), "meta.array.baker");
    assert.strictEqual(scopeOf(t, "a (string)"), "string.quoted.double.baker");
    assert.strictEqual(scopeOf(t, "SPEEDY"), "entity.name.macro.baker");
    assert.strictEqual(scopeOf(t, "#autorun"), "keyword.control.directive.baker");
    assert.strictEqual(scopeOf(t, "game.spawn"), "entity.name.function.baker");
    assert.strictEqual(scopeOf(t, "'bacon and eggs'"), "string.quoted.single.baker");
    assert.strictEqual(scopeOf(t, "$count"), "variable.other.baker");
    assert.strictEqual(scopeOf(t, "-2.5e3"), "constant.numeric.baker");
    assert.strictEqual(scopeOf(t, "#remove"), "keyword.control.directive.baker");
    // # opens a directive only before a letter (Baker 0.7.0).
    assert.strictEqual(scopeOf(t, "#x1"), "keyword.control.directive.baker");
    assert.strictEqual(scopeOf(t, "#00aa00 #1"), "meta.array.baker");  // values, as low is
    // Metadata is an array tagged @ and more (Baker 0.7.0); a lone @ is a symbol.
    assert.strictEqual(scopeOf(t, "@range"), "storage.type.annotation.baker");
    assert.strictEqual(scopeOf(t, "@desc"), "storage.type.annotation.baker");
    assert.strictEqual(scopeOf(t, "Run speed"), "string.quoted.double.baker");
    assert.strictEqual(scopeOf(t, "speed"), "entity.name.tag.baker");
    assert.strictEqual(scopeOf(t, "@"), "meta.array.baker");
    // A row's key: its segments as tags, its separators apart (Baker 0.7.0).
    const row = t.filter(([, s]) => s.endsWith(".baker")).map(([x, s]) => x + " " + s);
    for (const want of ["fighters entity.name.tag.baker", "kestrel entity.name.tag.baker",
                        "'max_hp' entity.name.tag.baker", ". punctuation.separator.key.baker"])
        assert.ok(row.includes(want), `${want} in ${JSON.stringify(row)}`);
    assert.strictEqual(scopeOf(t, "1100"), "constant.numeric.baker");
    assert.ok(row.includes("/ entity.name.tag.baker"), "a lone / is a tag");
    assert.ok(row.includes("/ punctuation.separator.key.baker"), "a key's / is a separator");
    assert.strictEqual(scopeOf(t, "@note"), "storage.type.annotation.baker");  // in a row
    assert.strictEqual(scopeOf(t, "a block"), "comment.block.baker");
    assert.strictEqual(scopeOf(t, "*/"), "comment.block.baker");
    assert.strictEqual(scopeOf(t, "0x1F"), "constant.numeric.baker");
    assert.strictEqual(scopeOf(t, "\\q"), "constant.character.escape.baker");
    console.log(`grammar: ${t.length} tokens, all scopes as expected`);
})().catch((e) => {
    console.error(e.message);
    process.exit(1);
});
