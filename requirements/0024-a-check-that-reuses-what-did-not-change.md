# 0024: A check that reuses the layers that did not change

## The need

When a tool checks a stack again after one layer's text changed, the layers whose text did
not change should not be read again: a Config, or a load, keeps each layer's parsed tree with
what it was read from (the file's text, through the host's reader), and on the next load
reuses it when the text is the same, so the cost of a check after an edit is the edited
layer's read plus the merge and the check.

## Why

An editor checks the stack on every pause in typing. In Baker's Duel grown to a 2 MB
`default.dta`, a check takes 100 to 200 ms, reading the 2 MB layer again each time although
the person is typing in a ten-line `local.dta` (how much of it is the read alone is not
measured; a check stopped early by a syntax error in it took about 50 ms). A language server, a watch-mode CI
checker, or an agent checking each of its edits pays this on every change. Games reload by
file already (`reload()`); a tool that hands Baker the text of open editors through a reader
has no file times to go by.

## Constraints

- The result exactly as a fresh load gives it: errors, sources and the merged tree.
- Keyed by the text, not by file times, since a tool's text comes from open editors.
- Memory for the kept trees is the tool's to bound; nothing changes for a game.

## Today

Met by Baker 0.5.0, with no new API: one Config kept, read through the reader, and `reload()`
after each edit, which re-reads from the first layer whose text changed. The editor's server
still loads fresh for each check until it keeps its Config.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
