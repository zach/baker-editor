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

Plan C (`dev/features-plan.md`), one step at a time: outline and rename, which layer wins in
the text, evaluating in the running game, the merged stack, value hints, and scenarios as
tests are done (first passes, below).

Plan D (`dev/next-plan.md`) is next: running the extension inside VS Code in CI, the game's
tweaks checked by Baker, method calls and game types, live values that follow the game,
scenarios run on unsaved text, and publishing the extension and the grammar.

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

The extension's unit tests (`npm test`) tokenize Baker text with the grammar, check which
files it calls Baker's, and read views' and scenarios' results. Its integration tests run it
inside the editor (`@vscode/test-electron`) on a copy of the server's test project, through
the provider commands the editor itself uses: diagnostics, hover, completion, definition,
rename, inlay hints and the faded shadowed value, the color picker, the merged and layer
views, and the test view against a fake game binary. CI runs them in VSCodium 1.90.2; locally:

```sh
cmake --build build                                   # the server
cd vscode && npm ci && npm run compile
VSCODE_EXECUTABLE=/path/to/codium xvfb-run -a npm run integration   # or no VSCODE_EXECUTABLE:
                                                                    # VS Code 1.90.2 is downloaded
```

They pass in about 4 seconds once the editor is up.

### A manual pass

What CI cannot see, done once per round by a person, on Windows, with Baker's stacks:

1. Install the `.vsix`, open baker-dev's `stacks/`: every game's files are Baker's, highlighted,
   and checked; a syntax error shows within about 200 ms of the keystroke.
2. In Duel's `default.dta`: hover a fighter's `health` and a move's `button`; complete `(button `;
   rename a macro; fold and outline the file.
3. Write a `local.dta` override: both lines show the hint, and the shadowed value is faded.
4. Open the 2 MB Duel (a copy grown as in the measurements): typing, hover, outline and inlay
   hints still feel immediate.
5. **Baker: Show the merged stack** and **Show what a layer changes** from the project file's
   code lenses; definition from a line of each.
6. With `stacks_duel --console-write` running: hover's **Live** table; Evaluate
   (Ctrl+Enter) on a command; Try a value, Write it back, Revert another.
7. Set `baker.sim.tool` to the stacks' binaries (`.../stacks_${project}.exe`): the test view
   lists Duel's scenarios, runs them, and a broken value fails one with its objective marked.

Findings go in this section, fixed or recorded.

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
range, step, kind, `desc` and `note`, and where the shape is declared), the metadata written
beside the value in the data (`(buffer 5 (range 1 20) (desc ...))`), the game's tweak at its
path (its type, code default, range, whether the data sets it, and whether it is dev only), and
every layer that contributes to it from the files, with its value, file and line, and which
one wins (`Config::explain`). On a command's head it shows the function's signatures and doc
from the game's reference; on a macro or a script function, where it is defined.

**Completion** offers, where the cursor is:

- naming an array (after `(`): the fields of the shape that governs the enclosing array,
  such as a fighter's in `(fighters (kestrel (`, and the game's tweaks there. Choosing one
  writes a value after it, to change: the value the stack has there now (overriding
  `(health` in `local.dta` writes `(health 1000`), else the tweak's code default, else the
  middle of its range, else its type's zero;
- naming a command (after `{`): the game's functions with their signatures, the data's
  script functions, and the objects the game names;
- a symbol field's value: the symbols its shape field or tweak allows, `(one_of ...)`, in the
  order declared (in Duel, `(button ` offers `light heavy kick special`, and `check` reports
  `(button lite)`); for a field with no list, the values the project's files give it, most
  used first; then the data's macros. In `(requires ...)`, the packs. Choosing a field with
  a list and no value yet offers the list as a choice.

**Colors:** a swatch, and VS Code's color picker, on every value Baker calls a color: `(kind
color)`, or 3 or 4 numbers ranged 0 to 1 with no kind, such as `(tint 0.9 0.5 0.2 (range 0
1))`. Picking a color rewrites the numbers in place, at most three decimals each.

**Ranges the game enforces:** a value outside the range written beside it in the data, or
outside its tweak's range, is a warning: only the running game refuses it, when it sets the
tweak. A shape's range is already an error, as `check` reports it.

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

## The merged stack, and what a layer changes

**Baker: Show the merged stack** opens, beside the editor, the stack as the game sees it
after every layer: `Config::root()`, one value a line, each followed by the layer and line it
comes from. It reads back through Baker into the same tree (every stack's does). An array
that fits on a line stays on one when all its values come from the same place.

```
(units
  (knight
    (health 50)  ; local data/local.dta:1
    (speed 2.5))  ; default data/default.dta:5
  (archer (health 60) (speed 3)))  ; default data/default.dta:6
```

**Baker: Show what a layer changes** lists every path a layer sets, with the value below it,
as a game's `diff --layer` prints it (`Config::layer_effect`); for `agent.dta`, what a person
reviews before promoting an agent's work. On Duel with an agent layer, the view and
`stacks_duel diff --layer agent` print the same lines. In the project file, a code lens on
each layer opens its view, and one on `(layers` the merged stack.

Go-to-definition from any line of either view jumps to where its value is written. A view
is the stack as it was when opened; open it again after an edit. The server answers the
custom requests `baker/merged`, `baker/layerEffect` (`{uri, layer}`) and `baker/layers`
with `{text, sources}`, for other editors to show the same way.

## Scenarios as tests

Each project's scenarios are tests in VS Code's test view: the project file's `(scenarios
...)` and `(objectives ...)` (Baker's host kit), listed by the server (`baker/scenarios`), each
with a run button on the line that declares it. A run starts the game's own binary, `sim
--scenario NAME... --json`, in the project's directory, and reads its `baker.sim/1`:

- a scenario passes when every run and every median it is judged by passed; one that failed
  lists each objective that failed, with its value, linked to where the objective is
  declared;
- the extension hands the results to the server (`baker/simResults`), which shows each
  failure as an error on the objective's name and on the scenario's, and a failed command
  where it is written, until that scenario runs again.

The binary is the project file's `(tool "build/bin/my_game")`, else the setting
`baker.sim.tool`, relative to the project's directory, where `${project}` is the directory's
name: Baker's stacks build theirs outside the project, so
`/path/to/build-stacks/bin/stacks_${project}` runs each stack's own. The game reads the
saved files.

In Duel, all eight scenarios (in `sim/cpu.dta` and `sim/frames.dta`) run in 137 ms and pass.
A jab doing 400 damage fails three: `frame_data` with `duel.combo_ceiling failed ... value
2000 (No combo does more than 35% of a fighter's health)`, on `(objective duel.combo_ceiling`
and on `(scenario frame_data`, and two of the CPU's win rates by their medians.

`tools/lsp_client.py --sim FILE TOOL` does the same from a shell.

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

### Evaluating in the game

With the project's game answering, two more code actions:

- **Evaluate in the game**, on a `{command}` (in a scenario, an `#autorun`, anywhere): sends
  the innermost command around the cursor to the console and shows the reply after it, as an
  inlay hint (`⇒ 42`, the first line; the whole reply in its tooltip), until the next edit of
  that file. An error shows as `✗ message`, and as a message with its script stack. In VS
  Code it is also **Baker: Evaluate in the game**, on Ctrl+Enter (Cmd+Enter).
- **Try this value in the game**, on a value such as `(health 1200)`: sends
  `{config.set fighters/kestrel/health 1200}`, without saving, so the game shows it at once.
  Hover then shows `scratch 1200` winning, and **Write the live value here** keeps it.
  Saving the file is still yours to do (**Baker: Try this value in the game** in VS Code).

Neither is offered without a game, or with another project's: the server asks the game,
once per connection, where one of the project's values comes from. An evaluation may take
up to 2 s, as a person asked for it; everything else keeps the 50 ms.

**Revert the live edit**, on a value the game has a live edit for: `{config.clear path}`
drops the edit, and the value is the files' again. Offered when the game binds
`config.clear` (Baker 0.2.0's development builds do; the server checks, once per connection,
by clearing a path no game has). On Duel, trying `(health 1200)` then reverting leaves hover
on `default 1000` and nothing unsaved in the game.

On Duel with the game running: evaluating `{duel.frame_table}` in a scenario shows the
table's header after it; `{duel.p1 nobody}` shows `✗ no pilot nobody: ...`; trying
`(health 1200)` on kestrel's `(health 1000)` line and writing it back leaves
`default.dta` with that one line changed.

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
- A symbol field with no `(one_of ...)` is completed from use: the first use of a value is not
  offered, and a typo used twice is.
- A color's components are taken as 0 to 1, as Baker's color rule has them.
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

Value hints (C5), Release build (Clang), on the 2 MB Duel: the range warnings take 2.6–3.0 ms
of each check; completing `(button ` (which reads every file of the project for the values in
use) answers in 20–42 ms as the shell client times it, and the file's colors 41 ms after it.
The check itself is unchanged: 108–170 ms against 109–144 ms at the previous commit (six
runs each, two checks a run).

The merged view (C4), Release build (Clang), at Baker 0.2.0: Duel's in 0.3 ms; the 2 MB
Duel's (5.7 MB of view, 99,623 lines with a source) in 186–196 ms, and 554–602 ms from the
request to the shell client holding the decoded reply, within the plan's second. The first
version took 827–883 ms for the view alone: it printed each array again to see whether it
fit on a line, and made every file name canonical once per line.

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

Baker 0.1's example console sent each reply as its own write, so Nagle's algorithm held each
one for a delayed acknowledgement: the same 113 values took about 50 ms, the console's whole
budget, and the server asked Linux for immediate acknowledgements (`TCP_QUICKACK`) to bring
them to the figures above. Baker 0.2.0's console sends a frame's replies in one write
(`requirements/0013`), so the workaround is gone: against it, 113 values take 10–26 ms
(a Debug build of the server, the connection included, five runs), and 60 or 200 pipelined
requests about one frame (6–20 ms).

## Baker

The server builds against a released Baker, `zach/baker` `v0.3.0`, through its public headers.
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
