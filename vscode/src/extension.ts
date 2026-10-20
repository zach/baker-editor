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
import {sourceAt, View, viewPath} from "./views";

const VIEW_SCHEME = "baker-merged";

let client: LanguageClient | undefined;

// The views the server has sent, by their document's URI, as they were when opened.
class Views implements vscode.TextDocumentContentProvider, vscode.DefinitionProvider {
    private readonly views = new Map<string, View>();
    private readonly changed = new vscode.EventEmitter<vscode.Uri>();
    readonly onDidChange = this.changed.event;

    set(uri: vscode.Uri, view: View): void {
        this.views.set(uri.toString(), view);
        this.changed.fire(uri);
    }
    provideTextDocumentContent(uri: vscode.Uri): string {
        return this.views.get(uri.toString())?.text ?? "";
    }
    // From any line of a view to the line its value is written on.
    provideDefinition(doc: vscode.TextDocument, at: vscode.Position): vscode.Location | undefined {
        const view = this.views.get(doc.uri.toString());
        const source = view && sourceAt(view, at.line);
        if (!source) return undefined;
        return new vscode.Location(vscode.Uri.parse(source.uri),
                                   new vscode.Position(Math.max(0, source.fileLine - 1), 0));
    }
}

// Asks the server for a view of the project `of` (a document's URI, else the active
// editor's) belongs to, and shows it beside the editor: the merged stack, or what a layer
// changes (asking which, if no layer is given).
async function show(views: Views, kind: "merged" | "layer", of?: string,
                    layer?: string): Promise<void> {
    if (!client) return;
    const uri = of ?? vscode.window.activeTextEditor?.document.uri.toString();
    if (!uri) {
        void vscode.window.showWarningMessage("Open a file of a Baker project first.");
        return;
    }
    if (kind === "layer" && layer === undefined) {
        const layers = await client.sendRequest<string[]>("baker/layers", {uri});
        layer = await vscode.window.showQuickPick(layers, {placeHolder: "The layer to show"});
        if (layer === undefined) return;
    }
    try {
        const view = kind === "merged"
            ? await client.sendRequest<View>("baker/merged", {uri})
            : await client.sendRequest<View>("baker/layerEffect", {uri, layer});
        const dir = path.dirname(vscode.Uri.parse(uri).fsPath);
        const target = vscode.Uri.from({scheme: VIEW_SCHEME, path: viewPath(dir, layer),
                                        query: uri});
        views.set(target, view);
        const doc = await vscode.workspace.openTextDocument(target);
        if (kind === "merged") await vscode.languages.setTextDocumentLanguage(doc, "baker");
        await vscode.window.showTextDocument(doc, {viewColumn: vscode.ViewColumn.Beside,
                                                  preview: true});
    } catch (e) {
        void vscode.window.showErrorMessage(`Baker: ${e instanceof Error ? e.message : String(e)}`);
    }
}

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
    const live = settings.get<boolean>("live.inlays", false) ? ["--live-inlays"] : [];
    const run = {
        command: serverPath(context),
        args: [...args, "--console-port", String(port), ...live, "--stdio"],
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
    // The server's own commands take the document and the cursor; these run them at the
    // active editor's cursor, for a key binding.
    const atCursor = (command: string) => async () => {
        const editor = vscode.window.activeTextEditor;
        if (!client || !editor) return;
        const {line, character} = editor.selection.active;
        await client.sendRequest("workspace/executeCommand", {
            command,
            arguments: [editor.document.uri.toString(), {line, character}],
        });
    };
    const views = new Views();
    context.subscriptions.push(
        vscode.workspace.registerTextDocumentContentProvider(VIEW_SCHEME, views),
        vscode.languages.registerDefinitionProvider({scheme: VIEW_SCHEME}, views),
        vscode.commands.registerCommand("baker.showMerged",
                                        (uri?: string) => show(views, "merged", uri)),
        vscode.commands.registerCommand("baker.showLayerEffect",
                                        (uri?: string, layer?: string) =>
                                            show(views, "layer", uri, layer)),
        vscode.commands.registerCommand("baker.restartServer", () => client?.restart()),
        vscode.commands.registerCommand("baker.evaluateHere", atCursor("baker.evaluate")),
        vscode.commands.registerCommand("baker.tryHere", atCursor("baker.try")));
    await client.start();
}

export async function deactivate(): Promise<void> {
    await client?.stop();
}
