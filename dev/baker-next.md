# Baker 0.7.0: what this editor meets

Six notes from the Baker session that changed the language (baker-dev D248, D249, D251, D260,
D261, D262), and two changes without a note (D263, D264), all released in Baker 0.7.0. The pin,
the test data's migration and what broke are done (the server reads `@` metadata and `#shape`
and `#pack` declarations; the grammars' `#` rule; the 3/100 layout); the rest seeds plan F.

Where a later change revised an earlier note, the later one holds:

- D261 replaced the `(shapes ...)` container: shapes are declared with `#shape (NAME ...)` at a
  file's top level, and `(shapes ...)` is ordinary data. So D248's shape words are offered
  inside `#shape`, D251's "a verb inside `(shapes ...)` is an error" applies to a `#shape`, and
  D260's refused received shapes are a `#shape`. The binary cache is format 8, not 7.
- No limit on where metadata sits: D262 had refused metadata past child 8,191 of an array, and
  D266, in the 0.7.0 release, removed the limit (a marker scan finds metadata at any position).

## D263: the house layout

`WriteOptions` defaults to indent 3 and width 100 (it was 2 and 80). Format Document on a file
whose project has no `(format ...)` lays out at 3 and 100, and a Config opened from a project
writes write-back's and promote's new forms in the project's `(format ...)`.

**Here:** the README's formatting section names the old defaults; the format tests that assume
an indent of 2 without a project need it said.

## D264: a layer's condition

A project file's `(layers ...)` entry may carry `(when COND)` beside `optional`, `disabled` and
`dev_only`: COND is a define's name, `(not C)`, `(and C C...)` or `(or C C...)`. While it does
not hold, the layer is skipped as a disabled one is. `Config::layer_condition(name)` and
`ExplainEntry::when` give it.

**Here:** completion and diagnostics in project files; the server sets a disabled layer's
`enabled` from the project, and its key for a kept stack (`key_of` in `diagnose.cpp`) must
include the condition.

## D248: metadata is spelled with `@`

**The language:**

- A child array whose tag is `@` and at least one more character is metadata:
  `(speed 4.5 (@range 0 10) (@desc "Run speed"))`. Nothing else is. A lone `'@'` is a symbol.
- `Array::values()` ends at the first metadata child, for every reader; `Array::metadata()`
  lists them and `Array::meta(tag)` finds one. `split_metadata` and `ValueMetadata` are gone.
- Metadata comes after the value: `(x (@range 0 1) 5)` is a parse error at the array. Fields
  may come before, between and after metadata.
- The shape grammar's words carry the sigil: `@at`, `@keyed`, `@optional`, `@range`, `@step`,
  `@kind`, `@desc`, `@note`, `@one_of`, and `(@rule "message" {expr} (@warn))`. `(key_of TABLE)`
  and `(list T)` are types, unchanged. A shape may carry its own `(@desc ...)` and
  `(@note ...)`. A bare word after a field's type, or an unknown `@word`, is a located error.
- `{func}` takes `(@status ...)` and `(@doc ...)` after its parameters; a form without the `@`
  is the body's first form.
- `check` warns, once per layer, of an array holding a value and a child tagged with an old
  metadata word (`range step desc note kind group one_of`), and of an unknown `@word` in data.
- JSON: `baker.check/1.5`, `baker.reply/1.2`, no field changed; a path to metadata is spelled
  `box.width.@note`.
- Migration: `tools/baker_migrate_metadata` inserts the `@` and nothing else, in `.dta`, C++
  string literals (`--cpp`) and Markdown (`--md`); `--report` lists what it would do.

**Here:**

- The server's own metadata reading (`hints.cpp`: `has_meta`, `read_meta`, `is_meta_tag`, the
  colors' metadata test) moves to `Array::metadata()` / `meta()`.
- Completion offers the `@` words after a value or a field's type, and the shape words inside
  a `#shape`; hover on them.
- Highlighting: a distinct scope for an `@` tag, in the grammar (TextMate and tree-sitter) and
  the semantic tokens.
- The test project, its fixtures and the tests' literal data need the migrator.

## D249: rows keyed by a path

**The language:**

- A top-level parenthesized form whose first atom is a bare `/`-led path of two or more
  bytes is a row: `(/fighters.kestrel.health 1100)` is `(fighters (kestrel (health 1100)))`.
  Segments are separated by `.` or `/`; a segment may be `'quoted'`; at most 16; all-digit
  (element) segments are refused.
- Rows sharing a prefix fold into one array per key; a file with rows holds each key once, a
  second writing an error at its key. A file without rows keeps first-wins.
- Not a row: a quoted `('/x' 1)`, a lone `/`, a `/`-led symbol anywhere but the head of a
  top-level array.
- Errors at the key: an empty segment, a trailing separator, more than 16 segments; an
  unclosed quote; a segment that must be quoted; an element segment; a row below the top level;
  a repeated key (in a list of records that also holds a row, naming the file's first row).
- `read_structure` gives a row's key the token kind `TextTokenKind::PathKey`.
- `SourceMap::row_made(array)` says whether a row's key made an array; edit-mode spans give each
  segment a sub-span of the key token.
- Write-back replaces a row's value in place, adds a new key in a file of rows as a row beside
  its prefix's rows, writes a layer file it creates as rows; promote can carry a removal.
- The kit: `diff --baker-rows` and `promote --baker-rows`.

**Here:** the server's `Structure` and data paths (`data_path`, the cursor's frames, outline,
hints placement, colors) treat a row's key as the path it names; highlighting of its segments;
completion after `(/`, segment by segment from the merged tree; outline showing the full path;
go-to-definition through segments; a "convert to row" action and back.

## D251: layer verbs and pins

**The language:**

- `(items #append rope torch)` adds its operands to the list below; `(things #drop (phone -2
  4.6 180))` removes the first value equal to each operand. Like `#remove`, a verb is a
  directive that reads only as a key's first value; what follows it, up to any metadata, is an
  operand (a scalar, or a whole array as an element).
- `(key #remove)` takes metadata after it; `#append` and `#drop` take one or more operands,
  then metadata. A verb anywhere else is an error at the array, with the form as its hint. A
  quoted `'#append'` in a verb's slot is the verb; elsewhere a quoted verb is a symbol.
- A key with two verbs takes two rows, applied in order; in a file of rows a key's verb rows
  stay apart while its other rows fold.
- `(@was v...)` records the value below when the row was written; a check warns at the row
  once the stack below has moved.
- A verb inside a `#shape` is an error.

**Baker's tools:** `LayerEffectEntry::op` and `stale`; `explain`'s standing `Composes`, with
`op`, never a value's `source()`; `DiffEntry::op_a` and `op_b`; the console's `config.effect`,
`config.explain` and `config.diff` carry `(op ...)`, `(stale 1)`, `(op_a ...)`/`(op_b ...)`.
Check warnings at the row: "'x' was 30 when this row was written; the stack has 32", "'items'
has no z to drop", "'items' already has a; #append adds another". Write-back refuses a path a
layer at or above the target composes, naming that row.

**Here:** the layer hints and the merged and layer views read `op`, `stale` and `Composes` (a
composing row is not a "wins"); highlighting of the verbs; hover and inlay hints on a verb row
(the list below, from `layer_effect`'s `below`); code actions "Restate as a list", "Convert to
#append" and "Pin to the value below" (`write_back_to(path, layer, true)`); the live revert and
write-back actions say why a composed path is refused.

## D260: `#` before a letter, and a list's count

**The language:**

- `#` opens a directive only before an ASCII letter: a lone `#`, `#1`, `#00aa00` and `#-` are
  symbols (`#x1` is still a directive). Baker's printer writes them bare.
  `read_structure` gives a `#`-led token the `Directive` kind only before a letter.
- `(list T N)` declares exactly N values and `(list T N M)` N to M: `(pos (list float 3))`.
  Any other count is an error at the field ("'pos' holds 2 values, not 3"); `baker.shapes/1.5`
  gives each field a `count`, `[min, max]` or `null`.

**Baker's tools:** the kit's `check` warns of a shape that governs nothing, at its
declaration; a received layer's shapes are refused under the data-only and scalars-only
policies.

**Here:** the TextMate and tree-sitter grammars' directive rule; completion of a count after
`(list T`; hover and inlay hints of a list field's count; the governs-nothing warning shown at
the shape (Baker's).

## D261: `#pack`, `#origin` and `#shape`

**The language:**

- A file declares with a directive taking one form, at its top level: `#pack (core (level game)
  (owners a b))` and `#origin ("economy.xlsx" (tool sheets-export))` once a file, `#shape (unit
  (@at units) (@keyed) (hp int (@range 1 9)))` any number. None of them is in the tree; each is
  recorded in the session, and the printer never writes one.
- `(pack ...)`, `(origin "...")` and `(shapes ...)` are ordinary arrays; a layer check warns of
  each, once per layer: "(shapes ...) is data; a file declares each shape with #shape (NAME
  ...)".
- Errors, located: a declaration below the top level; a second `#pack` or `#origin` in a file;
  a directive with no form; an origin whose first value is not a string; a shape with no name.
- `#define NAME (body)` still defines a macro, with a warning at the name: "#define RED (...)
  is the older macro form; write [RED ...]".

**Baker's tools:** `read_structure` gives a declaration's form `TextBracket::declares`, the
index of its directive's token. `layer_effect`, a layer check's effect, and `diff` list
declarations after the values, with `declares` (`#shape`, `#pack`, `#origin`), the shape's or
pack's name as the path, the form as the value, and `below` the same declaration under it.
`explain` takes `#shape.NAME` and `#shape.NAME.FIELD`. The shapes JSON is unchanged; a shape's
`file` and `line` are its `#shape`'s. The migrator `baker_migrate_declarations` rewrites the old
forms.

**Here:** the server finds packs by their file's `(pack` (`diagnose.cpp`), shapes and
completion of shape words inside `(shapes ...)`, and the test data declares both the old way:
all move to the directives. Highlighting of the three as declarations; outline and
go-to-definition for shapes by name (a field typed by another shape goes to its `#shape`);
hover on a governed array naming its `#shape`; the old forms' warning with the migrator's
rewrite as a quick fix.

## D262: braces written infix

**The language:**

- A `{...}` of an odd number of nodes, three or more, whose odd slots hold one and the same
  bare operator symbol (made of `+ - * / % < > = ! & | ^ ~`), reads head-first, if an operand
  is a number, a `$variable`, a `[property]` or a `{command}`: `{0.3 <= $value <= 0.7}` is
  `{<= 0.3 $value 0.7}`; `{wheat -> flour}` stays a call of `wheat`.
- Never infix: parentheses and square brackets; a quoted operator; a brace with a macro used
  between its operands, or a macro expanding to more than one node.
- Errors at the brace: "mixed operators in {...}: nest braces to say which applies first";
  "infix {...} needs '<=' between every two operands"; "infix {...} ends with '<=': write an
  operand after it".
- No ternary, no `&&`, `||` or `!`; `?` and `:` are symbols.
- `+`, `-` and `*` take two numbers or more; `<`, `<=`, `>` and `>=` chain over each adjacent
  pair; `+=`, `-=`, `*=` and `/=` store to a `$variable` or `[property]`.
- A command read infix prints infix; a prefix command the reader would take as infix prints
  with its operator quoted: `{+ '-' 1}`.

**Baker's tools:** `read_structure` gives an infix brace `TextBracket::infix`, the index of
its first operator's token (the command's head), -1 otherwise; its `tag` is still its first
token when a symbol. It reports the three errors, but leaves a brace with a macro use as
written.

**Here:** the cursor and hover take the operator as a brace's head where `infix` says so
(completion and hover of a call, the semantic tokens' function); highlighting of the operator;
quick fixes for a malformed brace (nest under `and`; insert the missing operator); a code
action between prefix and infix, leaving live-ops overlay data in prefix for older builds.
