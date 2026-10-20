# Plan C: what the editor does next

Plan B (baker-dev `dev/integration-plans.md` §3) is done in a first pass: diagnostics, hover,
completion, definition, live values and write-back, the VS Code extension and the tree-sitter
grammar. This plan is the next six features, each taken from a tool people already use for a
similar job, in the order we build them. Each is built, tested and pushed on its own.

Who it is for, which decides what matters:

- **designers** tune values across layers (`default.dta` under `local.dta` under
  `agent.dta`), often while the game runs;
- **agents** edit data files and need to see, and show a person, what their layer changes;
- **engineers** keep the vocabulary: shapes, macros, script functions.

Baker features a step needs that Baker lacks go to Baker as requirement notes, and the step
works around them here until a release has them (`CLAUDE.md`).

## C1. Outline, folding, references and rename

**From:** rust-analyzer, clangd, and every mature language server.

**What:**
- `textDocument/documentSymbol`: the file's arrays as a tree, named by tag, so the outline
  and breadcrumbs show `fighters > kestrel > moves > jab`. Macros (`[NAME`) and `{func}`s
  are symbols too.
- `textDocument/foldingRange`: every array, command and block comment over more than one line.
- `textDocument/references`: every use of a macro, a script function, a pack (in
  `(requires ...)`), and of a shape field (every array the shape governs that has the field),
  across the files the project reads.
- `textDocument/rename` (with `prepareRename`): the same set, as one workspace edit. Renaming
  a field renames its declaration in `(shapes ...)` too.

**How:** a structural scan of the text, which `scan.cpp` already does for the cursor, run over
whole files: one pass gives brackets with their tags and byte ranges. References and rename
run it over every file the project reads (`Diagnosis::files`), from the editor's buffers.
Fields are matched with the shapes' `governing()`.

**Acceptance:**
- the 2 MB file's outline arrives in under 50 ms;
- renaming a macro in the test project changes every use and its definition, and the project
  checks clean afterwards;
- references to a shape field find every fighter's use of it in Duel.

**Effort:** about two days.

## C2. Which layer wins, in the text

**From:** browser DevTools striking out overridden CSS rules; GitLens's line annotations;
TypeScript fading unreachable code.

**What:**
- an **inlay hint** after a value that a higher layer overrides: `← local.dta:3 (1200)`; after
  a value that overrides a lower one: `over default.dta:18 (1000)`;
- a **faded value** (diagnostic tag `Unnecessary`, severity hint) where a higher layer
  shadows it entirely, so a stale default reads as stale;
- with a game running, an inlay with the **live value** where it differs from the files
  (`live 1234`), behind a setting, since it asks the console once per visible value.

**How:** `Config::layer_effect(layer)` lists each layer's values with `shadowed`, the value
below and both sources, so one call per layer gives every hint for that layer's file; the
inlay request's range picks the visible lines. Live values are pipelined to the console in
one batch for the visible range, within the 50 ms budget, or skipped.

**Acceptance:**
- in Duel with a `local.dta` override, `default.dta`'s line shows the override and fades,
  and `local.dta`'s line shows what it overrides;
- inlays for a screen of the 2 MB file arrive in under 50 ms;
- no live inlay claims a value when no game answers.

**Effort:** about two days.

## C3. Evaluate in the running game

**From:** CIDER and Calva (Clojure): evaluate the form under the cursor in the running
process and show the result inline.

**What:**
- **Evaluate in the game** (a code action on a `{command}`, and a command bound to a key):
  sends the command under the cursor to the console and shows the reply after it, as an
  inlay that stays until the next edit, or the error with its script stack.
- **Try this value in the game** (a code action on a value): `{config.set path value}`
  without saving, so the game shows it at once; the hover then shows it live, and **Write the
  live value here** keeps it. Saving is still the person's choice.
- **Revert live edits**: `{config.restore ...}` or set back to the files' value.

**How:** the console client already exists (`live.cpp`). Results come back as text
(`baker.reply/1.1`'s `text`, the authority). A custom notification carries a result to the
extension, which decorates the line; other editors get it as a message.

**Acceptance:**
- with Duel running, evaluating `{duel.frame_table kestrel}` in a scenario shows its reply
  inline;
- "Try this value" on `(health 1000)` changed to 1200 makes hover show `scratch 1200` live
  without a save, and write-back then leaves a one-line diff;
- with no game, neither action is offered.

**Effort:** about two days, half of it the extension's decoration.

## C4. The merged stack, and what a layer changes

**From:** `kustomize build`, `helm template`, `cue eval`, Nix's evaluated output: the
configuration as the program sees it, after every overlay.

**What:**
- **Show merged** (a command): a read-only virtual document `baker-merged:` with the stack's
  merged tree, each value followed by a comment naming its layer and line. Definition from
  any value jumps to its source.
- **Show what a layer changes** (a command, and a code lens on each layer in the project
  file): every path the layer sets, with the value below it and where that came from, like
  a diff. For `agent.dta` this is what a person reviews before promoting an agent's work.

**How:** the merged tree is `Config::root()` printed with Baker's writer (`to_dta`); sources
come from `Config::source` per value, or from `layer_effect` for all layers in one pass,
whichever is faster on the 2 MB file. A layer's changes are `Config::layer_effect(layer)`
directly. LSP custom requests `baker/merged` and `baker/layerEffect` return text; the
extension shows it through a text-document content provider.

**Acceptance:**
- Duel's merged view reads back through Baker into the same tree;
- the agent layer's view of a stack lists the same changes as the stack's `diff` command;
- the merged view of the 2 MB file opens in under a second.

**Effort:** about three days.

## C5. Completion that knows the values, and Baker's hints

**From:** the VS Code JSON and YAML servers driven by JSON Schema: valid values offered,
defaults filled in by snippets, a color picker for colors.

**What Baker already gives:** metadata on a value, in the data or in code: `range`, `step`,
`kind`, `desc`, `note` (`tweak.hpp`, `docs/language.md` "Shapes"). `kind` is Baker's
presentation hint for tools (`color`, `angle`, `percent`, `path`, `frames`, `meters`, ...),
and a 3- or 4-element value ranged 0..1 with no kind counts as a color. The game's tweaks,
with their code defaults and metadata, are in the reference the build writes
(`{reference, tweaks, shapes}`, `baker.tweaks/1`), which the server reads only for its
functions today.

**What:**
- **hover and completion read every hint**, not only a shape field's: metadata written beside
  the value, the shape's, and the game's tweak at that path (its code default, `dev_only`,
  and whether the data binds it);
- choosing a field inserts it with a value: in an overriding layer, the value the stack has
  now (overriding `(health` in `local.dta` inserts `(health 1000)`); else the tweak's code
  default, or the middle of its range, or its type's zero, as a snippet placeholder;
- a **color picker** (`textDocument/documentColor`, `colorPresentation`) on every value
  Baker calls a color: `kind color`, or the 3- or 4-element 0..1 rule; it writes the
  components back in place;
- a **range hint**: a value outside its `range` is already an error for shapes; for a
  tweak's range, which only the running game enforces today, a warning;
- value completion for a symbol field from the values the project already uses for it,
  ranked by use (`(button ` offers `light heavy kick special` in Duel).

**Baker:** colors need nothing new. One requirement note: a field's allowed symbols, declared
in its shape or metadata (today `button` lists them only in its `desc`), so completion offers
exactly those and a value outside them is a diagnostic. Until then, completion offers the
values in use.

**Acceptance:**
- in Duel, completing `(button ` offers `light heavy kick special`, from use;
- completing `(health` in `local.dta` inserts `1000`, the stack's current value;
- a `(tint 0.9 0.5 0.2 (range 0 1))` shows a color swatch, and picking a color rewrites the
  three numbers;
- hover on a value with a tweak shows the tweak's code default and its `desc`.

**Effort:** about three days here, plus one Baker note.

## C6. Scenarios and objectives as tests

**From:** VS Code's Testing panel; Unity's Test Runner.

**What:** each scenario of a stack is a test in the editor's test view, with a run button and
a code lens on its file. A run calls the game's own simulation (`sim --scenario NAME --json`);
objectives that fail become diagnostics on their objective and scenario, and the run's
summary shows in the test view.

**Baker:** the editor must know how to run the game's tools. Baker 0.2.0's host kit
(`baker::host`, `dev/host-kit-plan.md`) brings the tool commands into Baker; this step waits
for it, and a requirement note asks for the project file to name the tool binary
(`(tool "build/bin/stacks_duel")`) beside `(reference ...)`, if the host kit does not.

**Acceptance:**
- in Duel, every scenario in `sim/` appears as a test, runs, and passes;
- breaking an objective's value in `default.dta` fails that test and shows the objective's
  message on the scenario.

**Effort:** about three days, after Baker 0.2.0.

## Order and status

| Step | Status |
|---|---|
| C1 Outline, folding, references, rename | Next |
| C2 Which layer wins, in the text | |
| C3 Evaluate in the running game | |
| C4 The merged stack, and what a layer changes | |
| C5 Completion that knows the values, and Baker's hints | One Baker note, for allowed symbols |
| C6 Scenarios and objectives as tests | Waits for Baker 0.2.0 |
