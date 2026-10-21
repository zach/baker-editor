// The extension inside VS Code, against a copy of the server's test project (run.js): each
// feature through the command VS Code itself uses to ask the providers, so these check the
// whole path from the editor through the client to baker-lsp and back.
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vscode = require("vscode");

const work = process.env.BAKER_INTEGRATION_WORKSPACE;
const project = path.join(work, "project");
const file = (rel) => vscode.Uri.file(path.join(project, rel));
const defaultUri = file("data/default.dta");
const localUri = file("data/local.dta");

// Polls `get` until it gives something truthy, or fails after `ms`.
async function until(what, get, ms = 20000) {
    const end = Date.now() + ms;
    for (;;) {
        const v = await get();
        if (v) return v;
        if (Date.now() > end) throw new Error(`timed out waiting for ${what}`);
        await new Promise((r) => setTimeout(r, 100));
    }
}

// The open document at `uri`, once the extension has called it Baker's.
async function baker(uri) {
    await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(uri));
    return until(`${uri.fsPath} to be Baker's`, () => vscode.workspace.textDocuments.find(
        (d) => d.uri.fsPath === uri.fsPath && d.languageId === "baker"));
}

// Replaces the whole text of `doc` (unsaved), and gives back a function that restores it.
async function setText(doc, text) {
    const before = doc.getText();
    const whole = (d) => new vscode.Range(0, 0, d.lineCount, 0);
    const edit = new vscode.WorkspaceEdit();
    edit.replace(doc.uri, whole(doc), text);
    assert.ok(await vscode.workspace.applyEdit(edit));
    return async () => {
        const back = new vscode.WorkspaceEdit();
        back.replace(doc.uri, whole(doc), before);
        await vscode.workspace.applyEdit(back);
    };
}

const diagnostics = (uri) => vscode.languages.getDiagnostics(uri);
const text = (contents) => contents.map((c) => (typeof c === "string" ? c : c.value)).join("\n");
const label = (l) => (typeof l === "string" ? l : l.label);
const labelOf = (hint) => (typeof hint.label === "string" ? hint.label
                                                         : hint.label.map((p) => p.value).join(""));

suite("Baker in VS Code", function () {
    let api;
    suiteSetup(async () => {
        const extension = vscode.extensions.getExtension("baker.baker");
        assert.ok(extension, "the extension is installed");
        api = await extension.activate();
        await baker(defaultUri);
        await baker(localUri);
    });

    test("a .dta under a project is Baker's; one elsewhere is left alone", async () => {
        const other = await vscode.workspace.openTextDocument(
            vscode.Uri.file(path.join(work, "other", "survey.dta")));
        await new Promise((r) => setTimeout(r, 1000));  // time to have claimed it, were it Baker's
        assert.notStrictEqual(other.languageId, "baker");
    });

    test("a shape error and a syntax error are diagnostics", async () => {
        const doc = await baker(defaultUri);
        // The archer: local.dta overrides the knight's health, so the merged knight stays valid.
        const restore = await setText(doc, doc.getText().replace("(health 60)", "(health 0)"));
        try {
            await until("the shape error", () => diagnostics(defaultUri).find(
                (d) => d.severity === vscode.DiagnosticSeverity.Error && /health/.test(d.message)));
            await setText(doc, doc.getText() + "\n(units (broken\n");
            await until("the syntax error", () => diagnostics(defaultUri).some(
                (d) => d.severity === vscode.DiagnosticSeverity.Error &&
                       d.range.start.line >= doc.lineCount - 3));
        } finally {
            await restore();
        }
        await until("the errors to clear", () => !diagnostics(defaultUri).some(
            (d) => d.severity === vscode.DiagnosticSeverity.Error));
    });

    test("hover shows the path, the shape and the layers", async () => {
        const hovers = await vscode.commands.executeCommand(
            "vscode.executeHoverProvider", defaultUri, new vscode.Position(4, 13));
        const shown = hovers.map((h) => text(h.contents)).join("\n");
        assert.match(shown, /units\/knight\/health/);
        assert.match(shown, /\| local \| `50` \| local\.dta:2 \| wins \|/);
    });

    test("completion writes a value, and offers a field's allowed symbols", async () => {
        const doc = await baker(localUri);
        const original = doc.getText();
        const restore = await setText(doc, original + "(units (mage (");
        try {
            const end = doc.lineAt(doc.lineCount - 1).range.end;
            const fields = await vscode.commands.executeCommand(
                "vscode.executeCompletionItemProvider", localUri, end);
            const health = fields.items.find((i) => label(i.label) === "health");
            assert.ok(health, "health is offered");
            assert.strictEqual(health.insertText.value, "health ${1:500}");
            await setText(doc, original + "(units (mage (role ");
            const roles = await vscode.commands.executeCommand(
                "vscode.executeCompletionItemProvider", localUri,
                doc.lineAt(doc.lineCount - 1).range.end);
            const offered = roles.items.map((i) => label(i.label));
            assert.deepStrictEqual(offered.slice(0, 2), ["tank", "ranged"]);
        } finally {
            await restore();
        }
    });

    test("definition jumps from an override to the line it shadows", async () => {
        const found = await vscode.commands.executeCommand(
            "vscode.executeDefinitionProvider", localUri, new vscode.Position(1, 17));
        const at = found.map((l) => ({uri: l.uri || l.targetUri, range: l.range || l.targetRange}));
        assert.ok(at.some((l) => l.uri.fsPath === defaultUri.fsPath && l.range.start.line === 4),
                  JSON.stringify(at));
    });

    test("rename changes every use of a macro", async () => {
        const edit = await vscode.commands.executeCommand(
            "vscode.executeDocumentRenameProvider", defaultUri, new vscode.Position(9, 2), "QUICK");
        const changes = edit.entries().flatMap(([, edits]) => edits);
        assert.ok(changes.length >= 1);
        assert.ok(changes.every((e) => e.newText === "QUICK"));
    });

    test("an override has an inlay hint, and the value it shadows is faded", async () => {
        const hints = await vscode.commands.executeCommand(
            "vscode.executeInlayHintProvider", defaultUri, new vscode.Range(0, 0, 20, 0));
        assert.ok(hints.some((h) => labelOf(h) === "← local.dta:2 (50)"),
                  hints.map(labelOf).join(", "));
        await until("the shadowed value to fade", () => diagnostics(defaultUri).some(
            (d) => d.tags && d.tags.includes(vscode.DiagnosticTag.Unnecessary)));
    });

    test("a color swatch, and the picker rewrites the numbers", async () => {
        const colors = await vscode.commands.executeCommand(
            "vscode.executeDocumentColorProvider", localUri);
        assert.strictEqual(colors.length, 1);
        assert.strictEqual(colors[0].color.red, 0.9);
        const presented = await vscode.commands.executeCommand(
            "vscode.executeColorPresentationProvider", new vscode.Color(0.25, 1, 0.3333333, 1),
            {uri: localUri, range: colors[0].range});
        assert.strictEqual(presented[0].label, "0.25 1 0.333");
    });

    test("the merged stack and a layer's changes open, and lead back to the files", async () => {
        await vscode.commands.executeCommand("baker.showMerged", defaultUri.toString());
        const merged = await until("the merged view", () => {
            const d = vscode.window.activeTextEditor && vscode.window.activeTextEditor.document;
            return d && d.uri.scheme === "baker-merged" && d;
        });
        const line = merged.getText().split("\n").findIndex(
            (l) => l.includes("(health 50)  ; local data/local.dta:2"));
        assert.ok(line >= 0, merged.getText());
        const found = await vscode.commands.executeCommand(
            "vscode.executeDefinitionProvider", merged.uri, new vscode.Position(line, 6));
        assert.strictEqual(found[0].uri.fsPath, localUri.fsPath);
        assert.strictEqual(found[0].range.start.line, 1);

        await vscode.commands.executeCommand("baker.showLayerEffect",
                                             file("baker-project.dta").toString(), "local");
        const changes = await until("the layer's view", () => {
            const d = vscode.window.activeTextEditor && vscode.window.activeTextEditor.document;
            return d && d.uri.path.endsWith("local.changes") && d;
        });
        assert.match(changes.getText(), /^layer 'local' changes:\n  units\.knight\.health  100 -> 50/);
    });

    test("the scenarios are tests, and a failed objective is marked where it is declared", async () => {
        // Earlier tests leave documents unsaved, though as they were: run without asking.
        const settings = vscode.workspace.getConfiguration("baker");
        await settings.update("sim.save", "never", vscode.ConfigurationTarget.Workspace);
        await api.scenarios.discover();
        const ids = api.scenarios.ids();
        assert.strictEqual(ids.length, 2, ids.join(", "));
        await api.scenarios.runAll();
        assert.deepStrictEqual(ids.map((id) => api.scenarios.verdicts.get(id)), ["passed", "passed"]);

        fs.writeFileSync(path.join(project, "fail"), "");
        await api.scenarios.runAll();
        const opening = ids.find((id) => id.endsWith("#opening"));
        assert.strictEqual(api.scenarios.verdicts.get(opening), "failed");
        await until("the failure on its objective", () => diagnostics(file("metrics/units.dta")).some(
            (d) => /units\.alive failed in scenario opening/.test(d.message)));
        fs.unlinkSync(path.join(project, "fail"));

        // An unsaved edit that breaks it: "never" runs the saved files, "always" saves first.
        const doc = await baker(defaultUri);
        const before = doc.getText();
        const restore = await setText(doc, "; BREAK\n" + before);
        try {
            await settings.update("sim.save", "never", vscode.ConfigurationTarget.Workspace);
            await api.scenarios.runAll();
            assert.strictEqual(api.scenarios.verdicts.get(opening), "passed");
            assert.ok(doc.isDirty);
            await settings.update("sim.save", "always", vscode.ConfigurationTarget.Workspace);
            await api.scenarios.runAll();
            assert.strictEqual(api.scenarios.verdicts.get(opening), "failed");
            assert.ok(!doc.isDirty);
        } finally {
            await restore();
            await doc.save();
            await settings.update("sim.save", "never", vscode.ConfigurationTarget.Workspace);
        }
        await api.scenarios.runAll();
        assert.strictEqual(api.scenarios.verdicts.get(opening), "passed");

        // A scenario file made on disk is listed, with no save in the editor.
        const extra = path.join(project, "scenarios", "extra.dta");
        fs.writeFileSync(extra, "(scenario added (seconds 1))\n");
        await until("the new scenario to be listed", () =>
            api.scenarios.ids().some((id) => id.endsWith("#added")));
        fs.unlinkSync(extra);
        await until("it to go", () => !api.scenarios.ids().some((id) => id.endsWith("#added")));
        await settings.update("sim.save", undefined, vscode.ConfigurationTarget.Workspace);
    });
});
