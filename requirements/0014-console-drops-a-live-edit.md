# 0014: The console drops one path's live edit

## The need

A console command that drops the live edit of one path from scratch, so the value goes back
to what the files say, as `Config::clear_scratch(path)` does in C++: for example
`{config.clear fighters.kestrel.health}`. With no path it could clear all of scratch, as
`clear_scratch()` does. The journal records the edit as reverted, as it does today for
`clear_scratch`.

## Why

A tool that lets a person try a value in the running game (an editor's "try this value", a
tuning panel, an agent exploring a balance change) needs the matching undo: "I tried 1200,
put it back." Without it, the only ways back are a whole-state snapshot restore, which also
undoes every other live edit and layer toggle, or setting the value to the files' value,
which leaves an entry in scratch that keeps overriding the file when the file is next
edited.

## Constraints

- Development builds only, like `config.set`: a shipping game binds neither.
- Not behind the write permission: it writes no file.
- The reply says whether there was an edit to drop, so a tool can tell "reverted" from
  "nothing to revert".

## Today

The editor's server offers **Revert the live edit** only when the game binds a command
named `config.clear` (it asks once per connection), so today it is never offered. It will
follow whatever name Baker gives the command.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
