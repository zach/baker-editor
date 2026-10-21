# 0024: A check that reuses the layers that did not change

## The need

When a tool checks a stack again after one layer's text changed, the layers whose text did
not change should not be read again: a Config, or a load, keeps each layer's parsed tree with
what it was read from (the file's text, through the host's reader), and on the next load
reuses it when the text is the same, so the cost of a check after an edit is the edited
layer's read plus the merge and the check.

## Why

An editor checks the stack on every pause in typing. In Baker's Duel grown to a 2 MB
`default.dta`, a check takes 100 to 200 ms, almost all of it reading the 2 MB layer again
although the person is typing in a ten-line `local.dta`. A language server, a watch-mode CI
checker, or an agent checking each of its edits pays this on every change. Games reload by
file already (`reload()`); a tool that hands Baker the text of open editors through a reader
has no file times to go by.

## Constraints

- The result exactly as a fresh load gives it: errors, sources and the merged tree.
- Keyed by the text, not by file times, since a tool's text comes from open editors.
- Memory for the kept trees is the tool's to bound; nothing changes for a game.

## Today

The editor's server reads every layer for every check: 100 to 200 ms on the 2 MB stack, 1 to
2 ms on the stacks as they are.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
