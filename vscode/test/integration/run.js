// Runs the integration tests inside VS Code (@vscode/test-electron): a pinned VS Code is
// downloaded once into .vscode-test/, opened on a copy of the server's test project, and the
// suite runs in its extension host. `npm run integration`, after `npm run compile` and a build
// of the server (BAKER_LSP, else ../build/server/baker-lsp). On Linux without a display, run it
// under xvfb-run.
const fs = require("fs");
const os = require("os");
const path = require("path");
const {runTests} = require("@vscode/test-electron");

async function main() {
    const extension = path.resolve(__dirname, "..", "..");
    const server = path.resolve(process.env.BAKER_LSP ||
                                path.join(extension, "..", "build", "server", "baker-lsp"));
    if (!fs.existsSync(server)) throw new Error(`no server at ${server}: build it, or set BAKER_LSP`);

    // A copy of the test project, which the tests may change, and a .dta of another format.
    const work = fs.mkdtempSync(path.join(os.tmpdir(), "baker-integration-"));
    const project = path.join(work, "project");
    fs.cpSync(path.join(extension, "..", "server", "tests", "data", "project"), project,
              {recursive: true});
    fs.writeFileSync(path.join(project, "data", "local.dta"),
                     "; One developer's tuning, for the integration tests.\n" +
                     "(units (knight (health 50)))\n" +
                     "(look (tint 0.9 0.5 0.2 (range 0 1)))\n" +
                     "(shapes (unit (role symbol (one_of tank ranged) (optional))))\n");
    fs.mkdirSync(path.join(work, "other"));
    fs.writeFileSync(path.join(work, "other", "survey.dta"), "another format's data\n");

    const game = path.join(__dirname, "fake-game.js");
    fs.chmodSync(game, 0o755);
    fs.mkdirSync(path.join(work, ".vscode"));
    fs.writeFileSync(path.join(work, ".vscode", "settings.json"), JSON.stringify({
        "baker.server.path": server,
        "baker.sim.tool": game,
        "baker.console.port": 0,  // no game's console: nothing waits on one
    }, null, 2));

    await runTests({
        version: process.env.VSCODE_VERSION || "1.90.2",
        extensionDevelopmentPath: extension,
        extensionTestsPath: path.join(__dirname, "suite", "index.js"),
        extensionTestsEnv: {BAKER_INTEGRATION_WORKSPACE: work},
        launchArgs: [work, "--disable-extensions", "--disable-workspace-trust",
                     "--skip-welcome", "--skip-release-notes"],
    });
}

main().catch((e) => {
    console.error(e);
    process.exit(1);
});
