// The Baker extension: starts baker-lsp for a workspace that holds a Baker project, and
// gives .dta files under a project the language id `baker`.
//
// .dta is also another format's extension, so the extension does not claim it everywhere:
// a .dta file is Baker's when a baker-project.dta is at or above it.
import * as fs from "fs";
import * as path from "path";
import * as vscode from "vscode";
import {LanguageClient, LanguageClientOptions, ServerOptions} from "vscode-languageclient/node";

import {isBaker, PROJECT_FILE} from "./project";

let client: LanguageClient | undefined;

async function claim(doc: vscode.TextDocument): Promise<void> {
    if (doc.uri.scheme !== "file" || doc.languageId === "baker") return;
    if (!isBaker(doc.fileName)) return;
    await vscode.languages.setTextDocumentLanguage(doc, "baker");
}

function serverPath(context: vscode.ExtensionContext): string {
    const configured = vscode.workspace.getConfiguration("baker").get<string>("server.path", "");
    if (configured) return configured;
    const exe = process.platform === "win32" ? "baker-lsp.exe" : "baker-lsp";
    const bundled = context.asAbsolutePath(path.join("server", exe));
    return fs.existsSync(bundled) ? bundled : exe;
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(claim));
    for (const doc of vscode.workspace.textDocuments) await claim(doc);

    const settings = vscode.workspace.getConfiguration("baker");
    const args = settings.get<string[]>("server.args", []);
    const port = settings.get<number>("console.port", 4555);
    const run = {
        command: serverPath(context),
        args: [...args, "--console-port", String(port), "--stdio"],
    };
    const serverOptions: ServerOptions = {run, debug: run};
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{scheme: "file", language: "baker"}],
        synchronize: {
            fileEvents: [
                vscode.workspace.createFileSystemWatcher(`**/${PROJECT_FILE}`),
                vscode.workspace.createFileSystemWatcher("**/reference.json"),
            ],
        },
    };
    client = new LanguageClient("baker", "Baker", serverOptions, clientOptions);
    context.subscriptions.push(
        vscode.commands.registerCommand("baker.restartServer", () => client?.restart()));
    await client.start();
}

export async function deactivate(): Promise<void> {
    await client?.stop();
}
