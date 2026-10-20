# Baker in the editor

Editor support for [Baker](https://github.com/zach/baker) data files: a language server,
`baker-lsp`, that shows what Baker knows about the file you are editing. This is plan B of
Baker's integration plans.

| Step | What | Status |
|---|---|---|
| 1 | The project file (in Baker) | Done in Baker; the server reads it |
| 2 | Diagnostics | First pass: below |
| 3 | Hover and completion | First pass: below |
| 4 | Go-to-definition | First pass: below |
| 5 | Live values from a running game | First pass: below |
| 6 | tree-sitter grammar | First pass: `tree-sitter-baker/` (registries not submitted) |
| - | VS Code extension | First pass: below |

Plan C (`dev/features-plan.md`) comes next, one step at a time: outline and rename and which
layer wins in the text are done (first passes, below); evaluating in the running game, the
merged stack, value completion, and scenarios as tests follow.

## The VS Code extension

`vscode/` is the extension: a TextMate grammar for highlighting, and a client that starts
`baker-lsp`. A `.dta` file is Baker's when a `baker-project.dta` is at or above it (`.dta` is
also another format's extension, which the extension leaves alone): the extension activates
in a workspace that holds one and gives such files the language `baker`.

```sh
cmake --build build                                 # the server
cd vscode && npm ci && npm run compile && npm test  # the extension, and its grammar's tests
npm run bundle-server -- ../build/server            # copy baker-lsp into the extension
npx vsce package --no-dependencies                  # baker-0.1.0.vsix
code --install-extension baker-0.1.0.vsix
```

Settings: `baker.server.path` (default: the bundled server, else `baker-lsp` on the PATH),
`baker.server.args`, `baker.console.port`, and `baker.live.inlays` (live values as inlay
hints, off by default). A rebuilt reference or an edited project file on disk is read
again; **Baker: Restart the language server** restarts it.

The extension's tests tokenize Baker text with the grammar and check which files it calls
Baker's; it has not been run inside VS Code from CI, which cannot download VS Code here.

## Diagnostics

On every change (150 ms after the last keystroke), the server reads the project file and loads
its layers as the game would, with the text of open editors in place of the files on disk
(the project file's too), and reports:

- **the project file's errors:** an unknown key, a malformed layer, a layer whose file is
  missing (at the entry that names it);
- **load errors:** syntax, directives, includes, located to the column. A layer that does not
  read does not stop the layers above it from being checked;
- **calls:** a call with the wrong number of arguments for every overload, or a literal
  argument of the wrong type, in the data and in `#autorun` commands, checked against the
  game's functions as its reference describes them; a call to a function nothing defines (not
  a stub) is an error, a likely typo;
- **the one-way rule:** a harder pack calling a softer pack's script function (a warning);
- **shapes:** a value of the wrong type or out of range (an error), a field no shape declares
  (information).

An open `.dta` file of the project that its layers do not read, such as a scenario or an
objective, is read on its own and checked beside them, as a game's `check` command checks
its scenarios.

Problems Baker locates by line alone (shape and call errors) underline the array the error's
path or message names on that line, else the line's first token. Columns are converted to
UTF-16 for LSP.

## Hover and completion

**Hover** on a value or an array's name shows its path, the shape field that governs it (type,
range, step, kind, `desc` and `note`, and where the shape is declared), and every layer that
contributes to it from the files, with its value, file and line, and which one wins
(`Config::explain`). On a command's head it shows the function's signatures and doc from the
game's reference; on a macro or a script function, where it is defined.

**Completion** offers, where the cursor is:

- naming an array (after `(`): the fields of the shape that governs the enclosing array,
  such as a fighter's in `(fighters (kestrel (`;
- naming a command (after `{`): the game's functions with their signatures, the data's
  script functions, and the objects the game names;
- a value: the data's macros; in `(requires ...)`, the packs.

**Go-to-definition** jumps from:

- a value or an array's name to every other layer that sets it: from `local.dta`'s override
  to the `default.dta` line it shadows, and back;
- a macro, a script function or a pack (in `(requires ...)`) to where it is defined;
- a file `#include`, `#require` or `#merge` names, or a path in the project file (a layer's
  file, the reference), to that file.

## Outline, folding, references and rename

- **Outline:** the file's tagged arrays as a tree, named by tag (`fighters > kestrel > moves >
  jab`), with leaves as fields showing their values, and macros and script functions. Past
  5,000 symbols, the outline leaves out leaf fields, then stops (as VS Code's YAML server
  caps its outline).
- **Folding:** every bracket and block comment over more than one line.
- **References:** every use of a macro, a script function, a pack in `(requires ...)`, or a
  shape field (each array the shape governs that has it, and its declaration), across the
  project's files and open buffers. In Duel, `startup` has six: five moves and the shape.
- **Rename:** the same set as one edit. Renaming a field renames its declaration too, but not
  the game's C++, which reads it by name: rename a field only with the code. The game's own
  functions are not renamable.

All four read each file's structure from its text in one pass (`structure.cpp`), so they work
on text that does not parse.

## Which layer wins, in the text

Where one layer overrides another's value, both lines say so, as inlay hints at the end of
the value's array (`Config::layer_effect`, for each layer above the bottom one):

```
default.dta    (health 1000) ← local.dta:2 (1200)       ; faded: a higher layer overrides it
local.dta      (health 1200) over default.dta:18 (1000)
```

The overridden value is also a hint-level diagnostic, `shadowed`, tagged *unnecessary*, so
the editor fades it and a stale default reads as stale. Editing one layer refreshes the
hints in the other layers' open editors (`workspace/inlayHint/refresh`).

With `--live-inlays` (`baker.live.inlays`) and a game answering, a value a live edit has
changed in the game shows it: `(health 1000) live 1234`. The server asks the game about
every value in the visible lines at once (at most 200), within the console's 50 ms; without
a game, or past the budget, nothing is shown. The hints are asked for when the editor
scrolls or the text changes, not when the game changes a value.

## Live values

While a game answers on its loopback console (`--console-port`, default 4555; the stacks'
games serve it), hover also shows the running game's layer chain for the value
(`{config.explain path}`), headed **Live**, with any value tuned in the game (`scratch`) on
top. The file chain stays headed *from the files*; without a game, nothing says live. A
game whose files are not the project's is not taken for its live value.

Every request to the console waits at most 50 ms, and while no game answers, connecting is
tried at most once a second, so hover never waits on a game that is not there.

When the game's winning value is a live edit, and the game binds `config.write_back`, the code
action **Write the live value here** asks the game to write it back to the line it came from:
the game's own write-back, so the edit is Baker's one-line diff. The stacks' games bind it
when started with `--console-write`:

```sh
build-stacks/bin/stacks_duel --headless --console-write   # from stacks/duel
python3 stacks/tools/console.py "{config.set fighters.kestrel.health 1234}"
# hover on (health 1000) in default.dta: Live shows scratch 1234 winning;
# "Write the live value here" leaves `(health 1234)` on that line, one line changed
```

All three of hover, completion and definition read the cursor's place from the text, not a parse, so they work on text being typed,
and they answer from the last check of the whole stack: while the file being edited does not
read, they keep the last check in which every layer read.

### Running it

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Each open file is checked in its project: the nearest `baker-project.dta` at or above it.
A workspace can hold several games (Baker's `stacks/` holds seven); each is checked with its
own layers and reference. `--project FILE` names one project instead, and `--skip local`
leaves a layer out. A game without a project file can give the same setup as arguments
(`--layer NAME=FILE`, `--optional-layer`, `--pack-levels`, `--include`, `--define`).

The game's reference JSON (`write_reference_json`, which the game writes in its build) tells
the server the game's functions and the objects it names. A project file names it with
`(reference "build/reference.json")`; `--reference FILE` gives one for every project the
server checks. Without one, calls are not checked and undefined calls are not reported.
Baker's stacks write theirs at build time (`stacks/<game>/build/reference.json`) and name it,
so after a build of the stacks, opening `stacks/` checks every game with no flags.

```sh
build/server/baker-lsp                                   # projects and references found
build/server/baker-lsp --reference build/reference.json  # a game whose project names none
```

`baker-lsp --help` lists the options. `tools/lsp_client.py` drives the server from a shell: it
opens files, applies an edit, and prints the diagnostics.

```sh
tools/lsp_client.py --replace data/default.dta "(health 1000)" "(health 0)" -- build/server/baker-lsp
```

### Limits of this pass

- Method calls are not checked: the reference describes the game's types and their methods,
  but `Context::declare` declares functions only. Calls on the objects the game names are not
  reported as undefined.
- A parameter of a type of the game's own takes any value: `Context::declare` knows Baker's
  types, not the game's.
- Live values come from one console port for the whole workspace, and their inlay hints
  are not refreshed when only the game changes.
- A layer's hints are from the last check in which every layer read, so while a file does
  not read, its hints can sit a line off.
- An inline `(range ...)` on a value is tweak metadata, which only the running game
  enforces; shapes are what the server checks, as the game's own `check` does.
- Other editors: point an LSP client at the binary, language id `baker`;
  `tree-sitter-baker/` highlights for Neovim, Helix and Zed, set up by hand for now.

### Measurements

Linux, Release build (Clang), the server's own timer around one check, at Baker 2b38a4d:

| Data | Check |
|---|---|
| Hearth, the largest stack (about 10 KB) | 0.4 ms (2–3 ms in a GCC Debug build) |
| Duel grown to 2 MB (1,776 fighters) | 100–120 ms clean, the first run 326 ms (cold); 49–54 ms when a syntax error stops the load |

The outline of the 2 MB file: 26 to 31 ms to build (52 ms the first time), 7 to 8 ms for its
folds; the request, capped at 5,000 symbols, answers in 82 to 120 ms as the shell client times
it (which polls every 20 ms). Built uncapped, its 112,025 symbols took 1.5 s to send. The
stacks' files, about 10 KB, take under a millisecond.

Finding the cursor's place scans the file's text up to the cursor: about 5 ms at the end of
the 2 MB file, before `explain`.

At Baker fb613b4 and before, the same 2 MB check took 110–160 ms clean and 50–60 ms with a
syntax error (one run each of three to six).

A syntax error in the stacks shows about 155 ms after the keystroke, within the plan's 200 ms.

Which layer wins (C2), Release build (Clang), against the previous commit built the same way:

| | Before | After |
|---|---|---|
| Check of the 2 MB Duel with a three-value `local.dta` (six runs, two checks each) | 157–207 ms | 150–216 ms |
| Inlay hints for a screen of the 2 MB file, top and bottom | - | 20–21 ms, the shell client's polling floor |
| Live inlays: 113 values of Duel's `default.dta`, one batch to the game | - | 18–22 ms, about one frame |

The check is no slower: only the layers above the bottom one are asked for their effect
(0.02 ms for a small `local.dta`), and the bottom layer's hints come from what they
override; asking the 2 MB bottom layer itself would cost about 100 ms. These checks are
slower than the table above's 100–120 ms on this machine today, before and after alike.

The game sends each reply as its own write, so without the server asking for immediate
acknowledgements (`TCP_QUICKACK`, Linux), the same 113 values took about 50 ms, the console's
whole budget: Nagle's algorithm holds each reply for a delayed acknowledgement. On other
platforms it still does; `requirements/0013` asks Baker's console to send them together.

## Baker

The server builds against a released Baker, `zach/baker` `v0.1.1`, through its public headers.
`-DBAKER_EDITOR_BAKER_DIR=PATH` builds against a local checkout instead. The stacks it is
tried on come from Baker's development repository, `zach/baker-dev` (`stacks/`), which the
release leaves out.

## Requirements for Baker

When the server needs something Baker lacks, it is written down in `requirements/`, in
Baker's own terms, from `requirements/0000-template.md`. Only the text of a requirement goes
to Baker.

## License

MIT, as Baker is. The server uses [nlohmann/json](https://github.com/nlohmann/json) and its
tests use [doctest](https://github.com/doctest/doctest), both MIT.
