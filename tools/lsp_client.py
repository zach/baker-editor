#!/usr/bin/env python3
"""A minimal LSP client for trying baker-lsp from a shell.

  tools/lsp_client.py [--open FILE] [--replace FILE OLD NEW] [--text FILE TEXT]
                     [--hover FILE LINE COL] [--complete FILE LINE COL]
                     [--definition FILE LINE COL] [--actions FILE LINE COL] -- baker-lsp ARGS...

Starts the server, opens each --open file (with its edit, if --replace names it; with TEXT
instead of the file's, if --text names it), waits for the diagnostics to settle, prints them
one a line as file:line:col: severity: message, then each hover's markdown and each
completion's labels (LINE and COL 1-based, COL in characters), and shuts the server down.
Exit status: 1 if any error was published, else 0.
"""
import json
import os
import pathlib
import subprocess
import sys
import threading
import time
import urllib.parse

SEVERITY = {1: "error", 2: "warning", 3: "info", 4: "hint"}


def uri(path):
    return "file://" + urllib.parse.quote(str(pathlib.Path(path).resolve()))


def send(proc, message):
    body = json.dumps(message).encode()
    proc.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    proc.stdin.flush()


def read_messages(proc, out):
    while True:
        length = None
        while True:
            line = proc.stdout.readline()
            if not line:
                return
            line = line.strip()
            if not line:
                break
            if line.lower().startswith(b"content-length:"):
                length = int(line.split(b":")[1])
        out.append(json.loads(proc.stdout.read(length)))


def main(argv):
    if "--" not in argv:
        sys.exit(__doc__)
    split = argv.index("--")
    opts, server = argv[:split], argv[split + 1:]
    opens, edits, texts, asks = [], {}, {}, []
    i = 0
    while i < len(opts):
        if opts[i] == "--open":
            opens.append(opts[i + 1])
            i += 2
        elif opts[i] == "--replace":
            edits[opts[i + 1]] = (opts[i + 2], opts[i + 3])
            opens.append(opts[i + 1])
            i += 4
        elif opts[i] == "--text":
            texts[opts[i + 1]] = opts[i + 2].encode().decode("unicode_escape")
            opens.append(opts[i + 1])
            i += 3
        elif opts[i] in ("--hover", "--complete", "--definition", "--actions"):
            asks.append((opts[i][2:], opts[i + 1], int(opts[i + 2]) - 1, int(opts[i + 3]) - 1))
            opens.append(opts[i + 1])
            i += 4
        else:
            sys.exit("unknown option " + opts[i])
    proc = subprocess.Popen(server, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    messages = []
    threading.Thread(target=read_messages, args=(proc, messages), daemon=True).start()
    send(proc, {"jsonrpc": "2.0", "id": 1, "method": "initialize",
                "params": {"processId": os.getpid(), "rootUri": uri("."), "capabilities": {}}})
    send(proc, {"jsonrpc": "2.0", "method": "initialized", "params": {}})
    for path in dict.fromkeys(opens):
        text = texts[path] if path in texts else pathlib.Path(path).read_text()
        send(proc, {"jsonrpc": "2.0", "method": "textDocument/didOpen",
                    "params": {"textDocument": {"uri": uri(path), "languageId": "baker",
                                                "version": 1, "text": text}}})
        if path in edits:
            old, new = edits[path]
            if old not in text:
                sys.exit(f"{path} has no {old!r}")
            send(proc, {"jsonrpc": "2.0", "method": "textDocument/didChange",
                        "params": {"textDocument": {"uri": uri(path), "version": 2},
                                   "contentChanges": [{"text": text.replace(old, new, 1)}]}})
    # Settled: no message for 500 ms after the debounce.
    seen, quiet_since = 0, time.time()
    while time.time() - quiet_since < 0.5:
        time.sleep(0.05)
        if len(messages) != seen:
            seen, quiet_since = len(messages), time.time()
    answers = {}
    for n, (what, path, line, col) in enumerate(asks):
        method = "textDocument/" + {"hover": "hover", "complete": "completion",
                                    "definition": "definition", "actions": "codeAction"}[what]
        where = {"line": line, "character": col}
        send(proc, {"jsonrpc": "2.0", "id": 100 + n, "method": method,
                    "params": {"textDocument": {"uri": uri(path)}, "position": where,
                               "range": {"start": where, "end": where}, "context": {"diagnostics": []}}})
    deadline = time.time() + 5
    while len(answers) < len(asks) and time.time() < deadline:
        time.sleep(0.02)
        for m in messages:
            if isinstance(m.get("id"), int) and m["id"] >= 100:
                answers[m["id"]] = m.get("result")
    send(proc, {"jsonrpc": "2.0", "id": 2, "method": "shutdown"})
    send(proc, {"jsonrpc": "2.0", "method": "exit"})
    proc.wait(timeout=5)

    latest = {}
    for m in messages:
        if m.get("method") == "window/logMessage":
            print("log:", m["params"]["message"], file=sys.stderr)
        elif m.get("method") == "textDocument/publishDiagnostics":
            latest[m["params"]["uri"]] = m["params"]["diagnostics"]
    errors = 0
    for u, diagnostics in sorted(latest.items()):
        path = urllib.parse.unquote(urllib.parse.urlparse(u).path)
        for d in diagnostics:
            start = d["range"]["start"]
            severity = SEVERITY.get(d["severity"], "?")
            errors += severity == "error"
            print(f"{path}:{start['line'] + 1}:{start['character'] + 1}: {severity}: "
                  f"{d['message']}")
    for n, (what, path, line, col) in enumerate(asks):
        result = answers.get(100 + n)
        print(f"--- {what} {path}:{line + 1}:{col + 1}")
        if what == "hover":
            print(result["contents"]["value"] if result else "(nothing)")
        elif what == "actions":
            print(", ".join(a["title"] for a in result or []) or "(none)")
        elif what == "definition":
            for loc in result or []:
                p = urllib.parse.unquote(urllib.parse.urlparse(loc["uri"]).path)
                print(f"{p}:{loc['range']['start']['line'] + 1}")
            if not result:
                print("(nothing)")
        else:
            items = result["items"] if result else []
            print(" ".join(item["label"] for item in items) or "(nothing)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
