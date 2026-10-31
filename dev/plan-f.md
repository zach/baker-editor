# Plan F: Baker 0.7.0's language, in the editor

Baker 0.7.0 changed how Baker text is written: metadata with `@`, declarations with `#shape`,
`#pack` and `#origin`, rows keyed by a path, layer verbs and pins, infix braces, list counts
in shapes, and conditional layers (`dev/baker-next.md` has each change). The first step is
done: the server builds against 0.7.0, reads `@` metadata and the declarations, the grammars
know the new `#` rule, and the test data is migrated. Today the editor reads the new language
correctly where it reads at all; this plan makes each new form something the editor helps
with, as it helps with the old ones.

Each step names what it needs from Baker; a need goes as a requirement note, as before.

## F1. Metadata and declarations, helped as data is

**Why:** every file a person writes now uses `@` metadata and `#shape`, and the editor offers
neither. Completion of a field's metadata words, the shape grammar's words, and navigation by
shape were how the old forms were written quickly; the new forms have none of it, and the
editor shows the old forms' warnings with no way to fix them.

**What:**
- **Completion:** after a value or a field's type, the metadata words with their `@`
  (`@range`, `@step`, `@kind`, `@desc`, `@note`, `@one_of`, `@was`), each with a snippet for
  its arguments; inside a `#shape`, the shape words (`@at`, `@keyed`, `@optional`, `@rule`,
  `@warn`, `@desc`, `@note`) and the types, `(key_of ...)` offering the keyed tables and
  `(list ...)` the element types.
- **Hover** on a metadata word: what it does and what it takes.
- **Highlighting:** an `@` tag gets a scope of its own in the TextMate and tree-sitter
  grammars, and a semantic token (a `decorator`, the standard type for annotations).
- **Shapes by name:** the outline lists each `#shape` with its fields; go-to-definition on a
  shape's name (in `(key_of ...)`, a field's type, or `explain`'s `#shape.NAME`) reaches its
  `#shape`; hover on an array the shape governs names it and links to it.
- **The old forms:** Baker's check warns of `(shapes ...)`, `(pack ...)`, `(origin ...)` and a
  child tagged with an old metadata word; each warning gets a quick fix that rewrites it as
  Baker's migrators do.

**Baker:** one requirement note (0027): the migrators' rewrites as a library call that takes a
file's text and returns the edits (as `format_text` does), so a tool offers them as a quick fix
for one form or a whole file, without a second copy of their rules. Until then, no quick fix:
the warning and the README name the migrator to run.

**Acceptance:** in Duel's `duel_core.dta`, completion after `(health int ` offers `@range`
and the rest, and the outline shows `#shape fighter` with its fields; in the test project, a
field typed `(key_of units)` goes to the `#shape` that governs `units`; in a file with `(shapes ...)`, the quick
fix (after the note) gives the migrator's text.

**Effort:** about two days, the quick fix a half day more after the release.

## F2. Rows keyed by a path

**Why:** `(/fighters.kestrel.health 1100)` is a whole nested path in one form, the form an
agent or a diff writes, and the server's structure sees it as an array tagged with a strange
symbol: hover, completion, hints, colors and references do not see the path it names.

**What:**
- The server's structure gives a row the data path its key names (Baker's `PathKey` token),
  segment by segment, so hover, the layer hints, colors, live values, references and rename
  work on a row as on the nested form it stands for.
- **Completion** after `(/`: the keys of the merged tree, segment by segment.
- **Highlighting:** a row's key as a path, its separators apart from its segments.
- **The outline** shows a row's full path; go-to-definition on a segment goes where that key
  is defined in the stack, as for a nested key.
- **Code actions:** "Write as a row" on a nested leaf, and "Write nested" on a row, laid out
  by the project's format.

**Baker:** none.

**Acceptance:** in a layer written as rows, hover on `health` in `(/fighters.kestrel.health
1100)` shows the same layers and value as in the nested form; a row's override has its inlay
hint; renaming a shape field renames it in rows; both code actions round-trip a file
unchanged in Baker's tree.

**Effort:** about two days.

## F3. Layer verbs and pins

**Why:** a layer may now add to or remove from a list below it, `(items #append rope)`, and
pin what it expected below, `(@was 30)`. The layer hints and views show what a layer sets,
and treat a composing row as if it set the value.

**What:**
- **The layer hints and views** read Baker's `op`, `stale` and the `Composes` standing: a
  verb row's hint shows the list it makes from the one below (`← default.dta:12 (rope torch)
  #append lamp → (rope torch lamp)`), and the merged view names the verb beside the value.
- **Hover** on a verb row: the list below, the operands, and the result.
- **A stale pin** is Baker's warning at the row; its quick fix updates the pin to the value
  below.
- **Code actions:** "Restate as a list" on a verb row (the list it now makes), "Write as
  #append" on a layer's list that extends the one below, and "Pin to the value below" on an
  override.
- Highlighting of the three verbs as directives in their slot.

**Baker:** none (`layer_effect`'s `below`, `op` and `stale` give everything).

**Acceptance:** with `(fighters (kestrel (moves #append (jab2 ...))))` in `local.dta`, the
hint and hover show the moves below and after; the pin quick fix and "Restate as a list" each
leave a file that checks clean and merges to the same tree.

**Effort:** about two days.

## F4. Infix braces

**Why:** `{0.3 <= $value <= 0.7}` calls `<=`, not `0.3`. The server takes a brace's first token
as what it calls: hover, completion, signature help and highlighting name the wrong function.

**What:**
- The cursor and the structure take an infix brace's head from Baker's `TextBracket::infix`:
  hover shows `<=`'s signatures, completion of an operand knows the call, the semantic tokens
  mark the operator as the function.
- Baker's three errors for a malformed brace get quick fixes: nest mixed operators, insert the
  missing operator.
- A code action between prefix and infix spelling for a call that can be written both ways.

**Baker:** none.

**Acceptance:** in Cube's objectives, hover on a `<=` in `{2 <= $value <= 10}` shows `<=`'s
signatures (Ledger's and Wayfarer's objectives are written infix too); the fixes each produce text that reads without the error.

**Effort:** a day.

## F5. List counts and conditional layers

**Why:** shapes now say how many values a list holds, and a project's layer may wait on a
define. Neither shows in the editor: a person learns a list's count from the error, and whether
a layer is in the stack only from what the values do.

**What:**
- **Counts:** completion of a count after `(list T `; hover on a list field shows its count;
  an inlay hint on a list value of a counted field shows how many it holds of how many.
- **Conditions:** completion in a project file's `(when ...)` of the defines and `not`, `and`,
  `or`; an inlay hint on each conditional layer saying whether it holds now
  (`Config::layer_condition`), and its layer's lens says when it is skipped; the merged view
  leaves a skipped layer out, as the game does.

**Baker:** none.

**Acceptance:** in a project with `(local "data/local.dta" optional (when HANDHELD))`, the
lens and inlay say the layer is skipped, and turn when `HANDHELD` is defined; completion in
`(when ` offers the project's defines.

**Effort:** a day.

## Order and status

F1 first: every file uses metadata and shapes, and its note can go to Baker at once. F2 and F4
next, since both are places where the editor now reads the text wrongly (a row's path, an
infix brace's head). F3 and F5 after. D6, publishing, still waits for Baker 1.0, and the
manual Windows pass (D1) is still the owner's.

| Step | Baker | Status |
|---|---|---|
| F1 Metadata and declarations | note 0027 for the quick fix: migrations as edits | Planned |
| F2 Rows keyed by a path | none | Planned |
| F3 Layer verbs and pins | none | Planned |
| F4 Infix braces | none | Planned |
| F5 List counts and conditional layers | none | Planned |
