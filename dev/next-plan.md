# Plan D: the editor support, proven and finished

Plans B and C are built (`README.md`, `dev/features-plan.md`), each step a first pass, on
Baker 0.3.0. This round turns the first passes into something a person can rely on: it runs
the extension where people use it, closes the gaps the README lists under *Limits of this
pass*, and gets the extension and grammar to the people who would use them. Each step names
what it needs from Baker; a need goes as a requirement note, as before.

## D1. Run it in VS Code

**Why:** CI tests the server and the extension's logic in Node, and every feature has been
tried from the shell client, but the extension has never run inside VS Code. The test view,
color picker, code lenses, inlays, virtual documents and code actions are each a contract
with VS Code that only VS Code can check.

**What:**
- Integration tests with `@vscode/test-electron` (VS Code's own harness), in a CI job that
  downloads a pinned VS Code and runs headless (`xvfb-run`) on Linux, against the test
  project and the server built in the same job:
  - a `.dta` under a project gets the language `baker`; one elsewhere does not;
  - a syntax error and a shape error show as diagnostics;
  - hover, completion (a field's value snippet, a `one_of` list), go-to-definition across
    layers, rename;
  - an override's inlay hint and its faded shadowed value;
  - a color swatch, and the picker's rewrite;
  - **Show the merged stack** and **Show what a layer changes** open, and definition from them
    jumps to the file;
  - the test view lists the project's scenarios and runs one against a small fake game
    binary written for the test (it prints a fixed `baker.sim/1`), failing and passing.
- A manual pass by the owner on Windows, with the stacks, from a checklist in the README:
  what CI cannot see (how it looks, how fast it feels on Duel's 2 MB file).

**Acceptance:** the job is green on every push; the checklist is done once and its findings
fixed or recorded.

**Effort:** about three days.

**As built:** the suite runs in VSCodium 1.90.2 (the same editor without Microsoft's services,
from GitHub's releases), which this environment and CI can both download; `VSCODE_EXECUTABLE`
names it, and without it a pinned VS Code is downloaded. Ten tests, about 4 seconds. The
manual checklist is in the README.

## D2. The game's tweaks, checked by Baker

**Why:** Baker 0.3.0's `check_tweaks(root, registry)` holds the data to the game's tweaks:
a value of the wrong type, outside its range or `one_of`, a tweak the data never sets, and a
misspelled key beside a group's tweaks. The server already warns on tweak ranges itself
(`hints.cpp`), a second copy of a rule Baker now owns.

**What:** the server builds a registry from the reference's tweaks and runs `check_tweaks`
with every check, in place of its own range warnings; its errors are errors, as the game's
`check` reports them, and its warnings are warnings, located.

**Baker:** `check_tweaks` reads registered `Tweak<T>` objects. The server can make those for
Baker's own types (`int32`, `float32`, `bool`, `symbol`, `string`), but not for a game's own
(`vec2`), whose tweaks would be left out. One requirement note: registering a tweak from its
description (`baker.tweaks/1.1`: path, type name, default, metadata), as `Context::declare`
does for functions (note 0003), so a tool holds data to every tweak without the game's code.
Until then, the basic types alone, and the rest by the server's range rule.

**Acceptance:** in Duel, `(buffer 50)` is the same error in the editor as in `stacks_duel
check`; a misspelled key under `(duel ...)` is a warning; the server's own range rule is gone
for every tweak Baker checks.

**Effort:** about two days here, plus one Baker note.

**As built:** `tweaks.cpp` makes a `Tweak<T>` per tweak of Baker's own types (a `Symbol` path,
the metadata's strings kept beside them) in a registry per project, and runs `check_tweaks`
with every check. A tweak the data does not set is not shown (it has no place in the files).
The server's range rule stays for game types and for ranges written beside a value no tweak
reads. The note is `requirements/0017`.

## D3. Method calls and the game's types

**Why:** the first two limits the README lists. A call on an object the game names
(`{clock thread_task ...}`) is not checked at all, and a parameter of the game's own type
takes any value: `Context::declare` declares functions only.

**What:** with Baker's help, the server declares the reference's types and their methods, so
`check` counts a method call's arguments and types its literal ones, and a literal of the
wrong shape for a game type (a `vec2` given one number) is an error. Hover and completion on
a method (after `{clock `) come from the same declarations.

**Baker:** one requirement note: declaring a type with its methods from the reference's
description, and checking a call on a named object against them, at no cost to a game.

**Acceptance:** in a stack with a named object, a method call with the wrong argument count is
an error in the editor and in the game's `check`; completion after `{clock ` offers its
methods.

**Effort:** about two days here, after the Baker note.

**As built, the first half:** hover and completion need no Baker: the reference already lists
each type's methods and each named object's type. After `{scheduler ` completion offers the
type's methods and its bases' (a base's method a type redeclares is offered once, the
type's), and hover shows a method's signatures and doc, and an object's type and methods.
**The second half (Baker 0.4.0):** the server declares the reference's types (bases first)
and named objects, and each parameter with its `values`; a type or object Baker's own
Context already has (its `Scheduler`, `scheduler`) is taken as known. A call to a method a
known type lacks is reported from `check`'s demand. All seven stacks stay clean; in Duel,
`{scheduler dely_task 2 ...}` is an error. D2 moved to `DescribedTweaks` at the same time;
a tweak it cannot describe (Cube's fixed-size-list `color`) is left out and its key not
taken for a stray, which would otherwise have been a false warning in Cube.

## D4. Live values that follow the game

**Why:** live inlay hints are asked for when the editor scrolls or the text changes, not
when the game changes a value, and one console port serves the whole workspace, so two games
running at once cannot both be live.

**What:**
- while live inlays are on and a game answers, the server asks it at most once a second
  whether its live edits changed, and refreshes the inlays (`workspace/inlayHint/refresh`)
  when they did; nothing is asked while no game answers;
- the console port per project: the project file's, if Baker gives it one, else a setting
  mapping a project directory to a port.

**Baker:** the console has no cheap "has anything changed" question: `Config::epoch()` moves
on every live edit, but no command reports it. One requirement note: a `config.epoch` command
(or the epoch in every reply), and optionally `(console 4555)` in the project file for tools.
Until then, `{config.unsaved}` once a second, which is one reply per poll.

**Acceptance:** with Duel running and live inlays on, `{config.set fighters.kestrel.health
1234}` from the console shows `live 1234` in the open `default.dta` within about a second,
with no edit or scroll; with two stacks running on two ports, each file's hover is its own
game's.

**Effort:** about two days.

**As built:** the server polls each project's game once a second while live inlays are on,
and refreshes the hints when its answer moves: `{config.epoch}` where the game binds it (note
0020); else the newest time in `{config.journal T}`, which answers only edits after T, so an
unchanged game answers an empty list; else `{config.unsaved}`. The plan's `{config.unsaved}`
was not enough: it names the paths with live edits, so a second edit of the same path looked
like no change. One `Live` per port; `--console DIR=PORT` (`baker.console.ports`) gives a
project's. In Duel a value set from the game's console reaches the hints in 524 to 981 ms;
Duel on 4556 and Cube on 4557 each show their own value in hover.

## D5. Running scenarios on what is in the editor

**Why:** a scenario run reads the saved files, so a person tuning a value must save before
each run, and a run after an unsaved edit silently tests the old value.

**What:** before a run, if a file of that project has unsaved changes, the test view asks:
save them and run, or run the saved files. A setting makes the answer stick (`baker.sim.save`:
`ask`, `always`, `never`). The run's output says which files it read.

**Baker:** none for this. (Running a game on unsaved text would need the game to read its
files through the editor; the kit's `CommandHooks::reader` makes that possible in-process,
not across a process boundary. Not proposed.)

**Acceptance:** with an unsaved edit that breaks an objective, running asks first, and "save
and run" fails the scenario; "never" runs the saved files and says so.

**Effort:** half a day.

**As built:** as planned, in the test view's run: the project's unsaved files are found
under its directory; `baker.sim.save` is `ask` (a modal: Save and Run, Run the Saved Files,
or cancel, which skips the run), `always` or `never`, and a run of the saved files lists in
its output the files it did not see. The integration suite covers `never` and `always` with
an unsaved edit that breaks a scenario; the modal cannot be shown in VS Code's tests.

## D6. Published

**Why:** today the extension is a `.vsix` built by hand, and the tree-sitter grammar is set up
by hand in each editor. Nobody outside this repository can install either.

**What:**
- the extension on the VS Code Marketplace and Open VSX, under the owner's publisher, with
  the server bundled per platform (Windows x64, Linux x64, macOS arm64 and x64) from CI, and
  a release job that publishes on a tag;
- the tree-sitter grammar proposed to nvim-treesitter, Helix's `languages.toml`, and as a Zed
  extension, each with its queries;
- a short user guide in the README: install, open a project, what each feature does.

**Owner's decisions:** the publisher name and account, the extension's name and icon, the
license text shown, and whether to submit to each registry. Every publish and every
submission to another project is the owner's to approve, one at a time.

**Acceptance:** `code --install-extension` from the Marketplace gives a working extension on
Windows and Linux with no build; each grammar submission is open or merged.

**Effort:** about three days, most of it CI for four platforms; the registries' reviews take
what they take.

## Order and status

D1 first: every later step is checked by it. D2 and D3 each wait on a Baker note; their notes
can go together at the start, and the steps follow when a release has them. D4 and D5 need
nothing from Baker. D6 last, once D1 has proven what is published, and not before Baker reaches 1.0.

| Step | Baker | Status |
|---|---|---|
| D1 Run it in VS Code | none | Integration tests done (VSCodium, in CI); the manual pass is the owner's |
| D2 The game's tweaks, checked by Baker | note 0017, met by Baker 0.4.0 | Done (first pass) |
| D3 Method calls and the game's types | note 0018, met by Baker 0.4.0 | Done (first pass) |
| D4 Live values that follow the game | note 0020: the config's epoch on the console, the port in the project file | Done (first pass), on the journal until note 0020 |
| D5 Running scenarios on what is in the editor | none | Done (first pass) |
| D6 Published | none; the owner's accounts | Waits for Baker 1.0 (the owner's call) |
