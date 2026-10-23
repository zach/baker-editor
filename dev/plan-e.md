# Plan E: the editor support, closer to the game

Plans B, C and D are built (`README.md`, `dev/features-plan.md`, `dev/next-plan.md`), on
Baker 0.5.0; D6, publishing, waits for Baker 1.0. This round closes the limits the README
still lists, uses what Baker 0.5.0's tolerant read makes possible, and keeps a large file
responsive while a check runs. Each step names what it needs from Baker; a need goes as a
requirement note, as before.

## E1. Hints that follow the text while a file does not read

**Why:** the README's third limit. A layer's hints (`← local.dta:3 (1200)`, the faded
shadowed value) come from the last check in which every layer read. While a person types in
a file that does not parse, which is most of the time they type, every edit above a hint
moves the text and not the hint: hints sit a line off, or on the wrong value, until the file
reads again.

**What:** the server keeps, with the last whole stack, each open file's text as that check
read it. When a file does not read, each hint is carried from its old line to its line in
the current text, by a line diff of the two texts; a hint whose line was edited or removed is
dropped, not guessed at. The shadowed-value diagnostics move the same way. Once the file
reads, the hints come from the new check, as now.

**Baker:** none.

**Acceptance:** in Duel, with a hint on `default.dta`'s kestrel health, inserting three lines
above it and leaving a bracket unclosed below it keeps the hint on the health line; editing
the health line itself drops its hint until the file reads.

**Effort:** about a day.

**As built:** the last whole check keeps each hinted file's text and its faded values
(`Loaded::texts`, `Loaded::shadowed`); a check whose layers do not all read, and every
inlay request, carries them through `LineMap`: the same first and last lines, then Myers'
line diff of the middle, whose cost grows with the lines edited (at most 1,000) and not with
the file. On the 2 MB Duel, three lines put in mid-file with a bracket left open at the end
map in about 1 ms. In Duel the acceptance holds: the hint and the faded value move from line
18 to 21, and go when the health line itself is edited.

## E2. Every tweak checked, lists included

**Why:** the README's second limit. A tweak Baker cannot make from its description is not
checked by the editor, though the game's own `check` checks it: a list
(`Convert<std::vector<T>>`, whose element type the reference does not write) and a
fixed-size list of a game's own type (`std::array<vec2, 4>`, whose values note 0019 left
null). In the editor, such a value of the wrong type passes, and fails only in the game.

**What:** the server registers these tweaks from their descriptions, as it does the rest
(D2), so `check_tweaks` holds each element to its type, and a fixed-size list of a game's
type to N of that type's values.

**Baker:** one requirement note (0025): a list's converter says its element type, and a
fixed-size list of a type of the game's own describes its values as N of that type's; the
reference writes both, and `DescribedTweaks` makes the tweaks from them. Until then, as now.

**Acceptance:** in a stack with a list tweak and an array of a game type, an element of the
wrong type is the same error in the editor as in the game's `check`; the README's limit is
gone.

**Effort:** half a day here, after the Baker release.

## E3. Highlighting from Baker's own tokens

**Why:** VS Code colors Baker text with a TextMate grammar, and the other editors with
tree-sitter: two more copies of Baker's syntax, which know a token's shape but not what it
is. Baker 0.5.0's `read_structure` says what each token is (a macro this text defines, a
directive, a variable, a quoted symbol), and the server knows more: which symbols are the
game's functions, the data's `{func}`s and macros, a tweak's field, a value a higher layer
shadows.

**What:** semantic tokens (`textDocument/semanticTokens/full` and `/range`) from the kept read
of each open file: macros, directives, variables, strings and numbers from Baker's token
kinds; a call's name as a function (the game's or the data's), a field that is a tweak, a
method on a named object; a shadowed value marked so. The TextMate grammar stays as the
fallback while the server starts, and the tree-sitter grammar for other editors.

**Baker:** none.

**Acceptance:** in Duel, a macro use, a `{func}` call and a game function call each have
their own color in VS Code; the integration suite checks the token types the server sends
for the test project; the 2 MB file's tokens answer within 100 ms.

**Effort:** about two days.

**As built:** `highlight.cpp` classifies the kept read's tokens (`Token::kind`, Baker's token
kinds): directives, variables, macro definitions (the name of a top-level `[...]`) and uses,
a `{func}`'s definition and calls, the game's functions and named objects with their
methods, tweak fields and the shadowed value's field (carried by `LineMap` as E1's hints
are). Strings and numbers stay with the grammar: on the 2 MB file the server's tokens are a
few, not 240,000. The extension declares the `tweak` and `shadowed` modifiers and their
scopes. The full request answers in 25 to 28 ms on the 2 MB file after an edit, almost all of
it the file's read, shared with the other requests (looking up a tweak only for an array
named as a tweak's last part took it from about 45 ms). The stacks use no macros or `{func}`
in their data, so Duel shows its tweaks and its scenarios' game calls; the test project's
server and integration tests check the rest.

## E4. Formatting as Baker writes

**Why:** nothing formats Baker text. A person aligns brackets by hand, an agent's edit
leaves the indentation it happened to write, and every tool that writes a file (write-back,
promote, the color picker, rename) writes as it can. A formatter in the server alone would
be a fourth copy of Baker's syntax, and would disagree with what Baker's own writers emit.

**What:** `textDocument/formatting` and `rangeFormatting` that lay a file out as Baker's
writer does, keeping every comment, `#` directive and macro use where it is written, and
leaving a file that does not parse unchanged.

**Baker:** one requirement note (0026): a formatter for text, from the tolerant read (note
0021), that keeps comments and directives, with the few choices a project sets (indent
width, line length), so a tool and Baker's own writers lay text out the same way. Until
then, nothing is offered: no second rule here.

**Acceptance:** formatting each stack's files changes no merged value (the check is the same
before and after) and keeps every comment; formatting twice changes nothing the second time.

**Effort:** about a day here, after the Baker release.

## E5. A large file stays responsive while it is checked

**Why:** the server answers one message at a time. On the 2 MB Duel a check takes 55 to 60
ms after an edit (90 to 100 ms while a file does not read), and the outline about 90 ms; a
hover or completion asked meanwhile waits for both. Typing at a normal pace in that file
queues requests behind checks.

**What:**
- an edit that arrives while requests wait cancels the ones it makes stale (LSP's
  `$/cancelRequest`, and the server's own drop of an outline or folds for an older
  version);
- hover, completion and definition answer before a pending check, from the last whole
  stack, and the check runs after them;
- measured as a person types: the time from each keystroke to its completion, on the 2 MB
  file, before and after.

A check on its own thread is not proposed: the kept Config is read by hover and changed by
the check, and Baker's Config is not thread-safe.

**Baker:** none.

**Acceptance:** typing ten characters a second into the 2 MB file, every completion answers
within 50 ms of its keystroke; the diagnostics still settle within the debounce and one
check of the last keystroke.

**Effort:** about two days.

**As built:** the server takes incremental edits (the 2 MB text was 20 ms of JSON per
keystroke); a queue (`Server::receive`, `work`) answers edits and the requests a person waits
on first, a file's views after them, stale views ContentModified, cancelled ones at once, and
runs a due check before waiting views; completion, hover, definition, inlay hints and colors
answer from the last whole stack instead of running the due check. `tools/typing_bench.py`
measures it as VS Code 1.90 asks (its debounce, read from its code: 25 ms for inlay hints,
200 to 1,000 ms for the other views): completion 38–42 ms median, 49–66 ms worst, from
186–199 ms. Diagnostics settle in 404 ms, the debounce and one check, where a forced check
had settled them in 223–288 ms. With every view after every keystroke, 80–99 ms: the outline
(54 ms) and colors (36 ms) are the views a completion can wait behind. The rest of a
completion's time is the file's read (Baker's lexer, whole); reading only what an edit
changed would take a Baker read that resumes from an edit, not yet asked for.

## Order and status

E1 and E3 need nothing from Baker and can start at once; E5 next, since E3 adds a request on
every edit. E2 and E4 each wait on a Baker note: their notes can go together at the start,
and the steps follow when a release has them. D6 still waits for Baker 1.0, and the manual
Windows pass (D1) is still the owner's.

| Step | Baker | Status |
|---|---|---|
| E1 Hints that follow the text | none | Done (first pass) |
| E2 Every tweak checked, lists included | note 0025: list element types, arrays of a game's types | Planned; the note first |
| E3 Highlighting from Baker's own tokens | none | Done (first pass) |
| E4 Formatting as Baker writes | note 0026: a formatter for text that keeps comments | Planned; the note first |
| E5 A large file stays responsive | none | Done (first pass) |
