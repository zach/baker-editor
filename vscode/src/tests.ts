// Each project's scenarios in VS Code's test view. The server lists them (baker/scenarios);
// a run starts the game's own binary (`sim --scenario NAME --json`, Baker's host kit), shows
// each scenario's verdict with the objectives that failed, and hands the results to the
// server, which marks those objectives in the files (baker/simResults).
import {execFile} from "child_process";
import * as path from "path";
import * as vscode from "vscode";
import {LanguageClient} from "vscode-languageclient/node";

import {PROJECT_FILE} from "./project";
import {outcomes, simArgs, SimResults, Suite, toolFor} from "./sim";

export class Scenarios {
    private readonly controller = vscode.tests.createTestController("baker", "Baker scenarios");
    private readonly suites = new Map<string, Suite>();  // by the project item's id
    // Each scenario's last verdict, by its item's id: passed, failed, errored or skipped.
    readonly verdicts = new Map<string, string>();

    constructor(private readonly client: () => LanguageClient | undefined,
                context: vscode.ExtensionContext) {
        this.controller.resolveHandler = () => this.discover();
        this.controller.refreshHandler = () => this.discover();
        this.controller.createRunProfile("Run", vscode.TestRunProfileKind.Run,
                                         (request, token) => this.run(request, token), true);
        context.subscriptions.push(
            this.controller,
            vscode.workspace.onDidSaveTextDocument((doc) => {
                if (doc.languageId === "baker") void this.discover();
            }));
        // A project, scenario or objective file made or deleted on disk lists them again.
        const watcher = vscode.workspace.createFileSystemWatcher("**/*.dta", false, true, false);
        let pending: ReturnType<typeof setTimeout> | undefined;
        const soon = () => {
            if (pending) clearTimeout(pending);
            pending = setTimeout(() => void this.discover(), 300);
        };
        context.subscriptions.push(watcher, watcher.onDidCreate(soon), watcher.onDidDelete(soon));
    }

    // Lists every project's scenarios, by asking the server about each project file.
    async discover(): Promise<void> {
        const client = this.client();
        if (!client) return;
        const files = await vscode.workspace.findFiles(`**/${PROJECT_FILE}`, "**/node_modules/**");
        const items: vscode.TestItem[] = [];
        this.suites.clear();
        for (const file of files) {
            let suite: Suite;
            try {
                suite = await client.sendRequest<Suite>("baker/scenarios",
                                                       {uri: file.toString()});
            } catch {
                continue;  // a project file that does not read: its diagnostics say why
            }
            if (suite.scenarios.length === 0) continue;
            const project = this.controller.createTestItem(suite.project,
                                                           path.basename(suite.directory), file);
            this.suites.set(project.id, suite);
            for (const s of suite.scenarios) {
                const item = this.controller.createTestItem(`${suite.project}#${s.name}`, s.name,
                                                            vscode.Uri.parse(s.uri));
                item.range = new vscode.Range(s.line, 0, s.line, 0);
                item.description = s.desc;
                project.children.add(item);
            }
            items.push(project);
        }
        this.controller.items.replace(items);
    }

    // The scenarios listed, by their items' ids.
    ids(): string[] {
        const out: string[] = [];
        this.controller.items.forEach((p) => p.children.forEach((s) => out.push(s.id)));
        return out;
    }

    // Runs every scenario, as the test view's Run All does.
    async runAll(): Promise<void> {
        const cancel = new vscode.CancellationTokenSource();
        await this.run(new vscode.TestRunRequest(), cancel.token);
        cancel.dispose();
    }

    private async run(request: vscode.TestRunRequest,
                      token: vscode.CancellationToken): Promise<void> {
        const run = this.controller.createTestRun(request);
        // The scenarios asked for, by project.
        const asked = new Map<vscode.TestItem, vscode.TestItem[]>();
        const add = (item: vscode.TestItem) => {
            if (request.exclude?.includes(item)) return;
            if (this.suites.has(item.id)) {
                item.children.forEach(add);
                return;
            }
            const project = item.parent;
            if (!project) return;
            asked.set(project, [...(asked.get(project) ?? []), item]);
        };
        if (request.include) request.include.forEach(add);
        else this.controller.items.forEach(add);

        const setting = vscode.workspace.getConfiguration("baker").get<string>("sim.tool", "");
        for (const [project, items] of asked) {
            if (token.isCancellationRequested) break;
            const suite = this.suites.get(project.id)!;
            const tool = toolFor(suite, setting, path.join, path.isAbsolute);
            if (!tool) {
                const why = new vscode.TestMessage(
                    "No game binary to run: name it in the project file, (tool \"build/bin/game\"), " +
                    "or set baker.sim.tool.");
                for (const item of items) {
                    run.errored(item, why);
                    this.verdicts.set(item.id, "errored");
                }
                continue;
            }
            for (const item of items) run.started(item);
            const names = items.map((i) => i.label);
            run.appendOutput(`${tool} ${simArgs(names).join(" ")}\r\n` +
                             "(the game reads the saved files)\r\n");
            const started = Date.now();
            const {stdout, stderr, code} = await this.exec(tool, simArgs(names), suite.directory,
                                                           token);
            const ms = Date.now() - started;
            let results: SimResults | undefined;
            try {
                results = JSON.parse(stdout) as SimResults;
            } catch {
                results = undefined;
            }
            if (!results || code === 2) {
                const why = new vscode.TestMessage(
                    `${path.basename(tool)} could not run (exit ${code}): ${stderr.trim() || stdout.trim()}`);
                for (const item of items) {
                    run.errored(item, why);
                    this.verdicts.set(item.id, "errored");
                }
                continue;
            }
            const verdicts = outcomes(results);
            for (const item of items) {
                const o = verdicts.get(item.label);
                this.verdicts.set(item.id, !o ? "skipped" : o.passed ? "passed" : "failed");
                if (!o) {
                    run.skipped(item);
                } else if (o.passed) {
                    run.passed(item, ms);
                } else {
                    run.failed(item, o.messages.map((m) => {
                        const message = new vscode.TestMessage(m.text);
                        if (m.file && m.line)
                            message.location = new vscode.Location(
                                vscode.Uri.file(m.file), new vscode.Position(m.line - 1, 0));
                        return message;
                    }), ms);
                }
            }
            await this.client()?.sendNotification("baker/simResults",
                                                  {uri: suite.project, results});
        }
        run.end();
    }

    private exec(tool: string, args: string[], cwd: string, token: vscode.CancellationToken):
        Promise<{stdout: string; stderr: string; code: number}> {
        return new Promise((resolve) => {
            const child = execFile(tool, args, {cwd, maxBuffer: 256 * 1024 * 1024},
                                   (error, stdout, stderr) => {
                                       const code = error && typeof error.code === "number"
                                           ? error.code
                                           : error ? 2 : 0;
                                       resolve({stdout, stderr: stderr || (error && !stdout
                                           ? error.message : ""), code});
                                   });
            token.onCancellationRequested(() => child.kill());
        });
    }
}
