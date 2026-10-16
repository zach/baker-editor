# Baker in the editor

Editor support for [Baker](https://github.com/zach/baker) data files: a language server,
`baker-lsp`, that shows what Baker knows about the file you are editing. This is plan B of
Baker's integration plans.

| Step | What | Status |
|---|---|---|
| 1 | The project file (in Baker) | Waiting: `requirements/0001-project-file.md`; the server takes the setup on its command line meanwhile |
| 2 | Diagnostics | First pass: below |
| 3 | Hover and completion | Not started |
| 4 | Go-to-definition | Not started |
| 5 | Live values from a running game | Not started |
| 6 | tree-sitter grammar | Not started |

## Diagnostics

On every change (150 ms after the last keystroke), the server loads the project's layers as
the game would, with the text of open editors in place of the files on disk, and reports:

- **load errors:** syntax, directives, includes, a missing layer, located to the column;
- **calls:** a call with the wrong number of arguments for every overload, in the data and in
  `#autorun` commands, counted against the game's reference; a call to a function nothing
  defines (not a stub) is an error, a likely typo;
- **the one-way rule:** a harder pack calling a softer pack's script function (a warning);
- **shapes:** a value of the wrong type or out of range (an error), a field no shape declares
  (information).

An open `.dta` file the layers do not read, such as a scenario or an objective, is read on
its own and checked beside them, as a game's `check` command checks its scenarios.

Problems Baker locates by line alone (shape and call errors) underline the array the error's
path or message names on that line, else the line's first token. Columns are converted to
UTF-16 for LSP.

### Running it

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

The server takes the game's load setup as arguments, run from the game's directory:

```sh
build/server/baker-lsp \
  --layer default=data/default.dta \
  --optional-layer local=data/local.dta --optional-layer agent=data/agent.dta \
  --pack-levels engine,game,task --include data \
  --reference build/reference.json
```

`baker-lsp --help` lists the options. `--reference` is the game's reference JSON, which the
game writes in its build (for Baker's stacks, `stacks_duel reference --json`). Without it,
calls are not counted and undefined calls are not reported. `--object NAME` names an object
the game adds to its Context (`clock` in Hearth), whose calls are not undefined functions.

`tools/lsp_client.py` drives the server from a shell: it opens files, applies an edit, and
prints the diagnostics.

```sh
tools/lsp_client.py --replace data/default.dta "(health 1000)" "(health 0)" -- build/server/baker-lsp ...
```

### Limits of this pass

- Argument types are not checked, only their number: each of the game's functions is
  registered as a stand-in that takes any value (`requirements/0003`).
- Calls on objects (`{clock thread_task ...}`) need `--object`, because the reference does
  not list a game's named objects (`requirements/0002`). Method calls are not checked.
- An inline `(range ...)` on a value is tweak metadata, which only the running game
  enforces; shapes are what the server checks, as the game's own `check` does.
- No editor extension yet: point an LSP client at the binary, language id `baker`.

### Measurements

Linux, Release build (Clang), the server's own timer around one check:

| Data | Check |
|---|---|
| Hearth, the largest stack (about 10 KB) | 2–3 ms |
| Duel grown to 2 MB (1,776 fighters) | 110–160 ms clean; 50–60 ms when a syntax error stops the load |

A syntax error in the stacks shows about 155 ms after the keystroke, within the plan's 200 ms.

## Baker

Baker is fetched at a pinned commit of its development repository, because no release has
E3 and E4 yet; move to a `zach/baker` tag once one ships. `-DBAKER_EDITOR_BAKER_DIR=PATH`
builds against a local checkout instead.

## Requirements for Baker

When the server needs something Baker lacks, it is written down in `requirements/`, in
Baker's own terms, from `requirements/0000-template.md`. Only the text of a requirement goes
to Baker.

## License

MIT, as Baker is. The server uses [nlohmann/json](https://github.com/nlohmann/json) and its
tests use [doctest](https://github.com/doctest/doctest), both MIT.
