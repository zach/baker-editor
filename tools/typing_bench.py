#!/usr/bin/env python3
"""Types into a file through baker-lsp as an editor would, and times what a person waits for.

  tools/typing_bench.py FILE [--rate N] [--chars N] [--heavy every|pause] -- baker-lsp ARGS...

Opens FILE, waits for its first check, then types `(health 1234)` on a new line before its
last line, N characters a second (--rate, default 10), the whole text sent after each one
(or, if the server takes incremental changes, the keystroke alone). After each keystroke it asks for completion at the cursor, and the
requests an editor asks for a file's view (outline, folds, semantic tokens, colors, inlay
hints), each cancelled ($/cancelRequest) if it has not been answered when the next keystroke
goes, as VS Code cancels a stale one. With --heavy pause (the default), as VS Code 1.90 sends
them: inlay hints after each keystroke (its debounce is at least 25 ms) and the rest once the
typing stops (at least 200 ms for folds, 300 for semantic tokens, 350 for the outline, 1000
for colors, each longer as the request is slower). With --heavy every, all of them after
every keystroke: a worst case no editor sends. Prints each completion's time from its keystroke, and the time from the last
keystroke to the last diagnostics published for FILE.
"""
import json
import re
import pathlib
import subprocess
import sys
import threading
import time
import urllib.parse

HEAVY = [
    "textDocument/documentSymbol",
    "textDocument/foldingRange",
    "textDocument/semanticTokens/full",
    "textDocument/documentColor",
    "textDocument/inlayHint",
]


def uri(path):
    return "file://" + urllib.parse.quote(str(pathlib.Path(path).resolve()))


class Client:
    def __init__(self, command):
        self.proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.lock = threading.Lock()
        self.replies = {}  # id -> (time, message)
        self.diagnostics = []  # (time, uri, count)
        self.checks = []  # (time, milliseconds), from the server's log of each check
        self.next_id = 1
        threading.Thread(target=self.read, daemon=True).start()

    def send(self, message):
        body = json.dumps(message).encode()
        self.proc.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.proc.stdin.flush()

    def notify(self, method, params):
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def request(self, method, params):
        rid = self.next_id
        self.next_id += 1
        self.send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
        return rid

    def read(self):
        out = self.proc.stdout
        while True:
            length = None
            while True:
                line = out.readline()
                if not line:
                    return
                line = line.strip()
                if not line:
                    break
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":")[1])
            message = json.loads(out.read(length))
            now = time.monotonic()
            with self.lock:
                if "id" in message and "method" not in message:
                    self.replies[message["id"]] = (now, message)
                elif message.get("method") == "textDocument/publishDiagnostics":
                    p = message["params"]
                    self.diagnostics.append((now, p["uri"], len(p["diagnostics"])))
                elif message.get("method") == "window/logMessage":
                    m = re.search(r"checked \d+ files in ([\d.]+) ms", message["params"]["message"])
                    if m:
                        self.checks.append((now, float(m.group(1))))

    def wait_reply(self, rid, timeout=30):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            with self.lock:
                if rid in self.replies:
                    return self.replies[rid]
            time.sleep(0.001)
        raise TimeoutError(f"no reply to {rid}")


def position(text, offset):
    line = text.count("\n", 0, offset)
    start = text.rfind("\n", 0, offset) + 1
    return {"line": line, "character": len(text[start:offset].encode("utf-16-le")) // 2}


def main(argv):
    if "--" not in argv:
        sys.exit(__doc__)
    split = argv.index("--")
    opts, command = argv[:split], argv[split + 1:]
    path, rate, chars, heavy = opts[0], 10.0, None, "pause"
    i = 1
    while i < len(opts):
        if opts[i] == "--rate":
            rate = float(opts[i + 1])
        elif opts[i] == "--chars":
            chars = int(opts[i + 1])
        elif opts[i] == "--heavy":
            heavy = opts[i + 1]
        i += 2
    text = pathlib.Path(path).read_text()
    doc = {"uri": uri(path)}
    c = Client(command)
    _, init = c.wait_reply(c.request("initialize", {"rootUri": uri(pathlib.Path(path).parent),
                                                    "capabilities": {}}))
    sync = init["result"]["capabilities"]["textDocumentSync"]
    incremental = isinstance(sync, dict) and sync.get("change") == 2
    c.notify("initialized", {})
    c.notify("textDocument/didOpen", {"textDocument": {**doc, "languageId": "baker",
                                                       "version": 1, "text": text}})
    c.wait_reply(c.request("textDocument/hover", {"textDocument": doc,
                                                  "position": {"line": 0, "character": 0}}))
    time.sleep(1.0)  # the first check, and anything it schedules

    typed = "(health 1234)"[: chars or None]
    at = text.rstrip("\n").rfind("\n") + 1  # the start of the last line
    c.notify("textDocument/didChange", {"textDocument": {**doc, "version": 2},
                                        "contentChanges": [{"text": text[:at] + "\n" + text[at:]}]})
    text = text[:at] + "\n" + text[at:]
    cursor = at
    time.sleep(1.0)
    pending = []  # heavy requests not yet answered
    latencies = []
    last_key = 0.0
    for n, ch in enumerate(typed):
        where = position(text, cursor)
        text = text[:cursor] + ch + text[cursor:]
        cursor += 1
        change = ({"range": {"start": where, "end": where}, "text": ch} if incremental
                  else {"text": text})
        with c.lock:
            for rid in pending:
                if rid not in c.replies:
                    c.notify("$/cancelRequest", {"id": rid})
        pending = []
        key = time.monotonic()
        c.notify("textDocument/didChange", {"textDocument": {**doc, "version": 3 + n},
                                            "contentChanges": [change]})
        rid = c.request("textDocument/completion", {"textDocument": doc,
                                                    "position": position(text, cursor)})
        for method in HEAVY if heavy == "every" else ["textDocument/inlayHint"]:
            params = {"textDocument": doc}
            if method == "textDocument/inlayHint":
                params["range"] = {"start": {"line": 0, "character": 0},
                                   "end": {"line": text.count("\n"), "character": 0}}
            pending.append(c.request(method, params))
        got, _ = c.wait_reply(rid)
        latencies.append((got - key) * 1000)
        last_key = key
        time.sleep(max(0.0, key + 1.0 / rate - time.monotonic()))
    if heavy == "pause":
        for method in HEAVY[:-1]:
            params = {"textDocument": doc}
            if method == "textDocument/inlayHint":
                params["range"] = {"start": {"line": 0, "character": 0},
                                   "end": {"line": text.count("\n"), "character": 0}}
            pending.append(c.request(method, params))
    for rid in pending:
        c.wait_reply(rid)
    time.sleep(1.0)
    with c.lock:
        settled = [t for t, u, _ in c.diagnostics if u == doc["uri"] and t > last_key]
    c.wait_reply(c.request("shutdown", {}))
    c.notify("exit", {})
    s = sorted(latencies)
    print(f"{len(s)} keystrokes at {rate:g}/s, {'incremental' if incremental else 'full'} "
          f"sync, heavy requests {heavy}: completion "
          f"median {s[len(s) // 2]:.1f} ms, worst {s[-1]:.1f} ms; "
          + " ".join(f"{x:.0f}" for x in latencies))
    with c.lock:
        checks = [ms for t, ms in c.checks if t > last_key]
    if settled:
        print(f"diagnostics settled {(settled[-1] - last_key) * 1000:.0f} ms after the last keystroke"
              f" (checks after it: {', '.join(f'{x:.0f} ms' for x in checks) or 'none'})")
    else:
        print("no diagnostics after the last keystroke")


if __name__ == "__main__":
    main(sys.argv[1:])
