#!/usr/bin/env python3
"""A minimal LSP client for trying baker-lsp from a shell.

  tools/lsp_client.py [--open FILE] [--replace FILE OLD NEW] -- baker-lsp ARGS...

Starts the server, opens each --open file (with its edit, if --replace names it), waits for
the diagnostics to settle, prints them one a line as file:line:col: severity: message, and
shuts the server down. Exit status: 1 if any error was published, else 0.
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
    opens, edits = [], {}
    i = 0
    while i < len(opts):
        if opts[i] == "--open":
            opens.append(opts[i + 1])
            i += 2
        elif opts[i] == "--replace":
            edits[opts[i + 1]] = (opts[i + 2], opts[i + 3])
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
        text = pathlib.Path(path).read_text()
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
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
