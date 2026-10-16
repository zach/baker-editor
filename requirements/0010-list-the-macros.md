# 0010: A load lists the macros and script functions it defined, and where

## The need

After a load, a host should be able to list every macro the session defined (`#define NAME`,
`#define NAME (body)`, a top-level `[NAME ...]`), each with its file and line, and every
script function the load's `#autorun` commands define with `{func NAME ...}`, with its file
and line, without running them.

## Why

A tool that helps a person write data needs the data's own vocabulary, not only the game's:

- completion offers the macros where a value goes, and the script functions where a command
  goes;
- hover and go-to-definition on a macro or a script function jump to where it is defined;
- a checker reports a macro defined twice, or one never used.

The session already knows each macro (`LoadSession::macro(name)` finds one by name) but
cannot list them, and a `{func}` is known only once its `#autorun` runs, which a tool checking
data must not do.

## Constraints

- A list on the session (or the Config's read session) after the load; nothing added to the
  load's own work beyond recording the location it already reads.
- Edit mode only, if recording locations costs anything in a gameplay load.
- Macros a layer brought go with the layer, as the session's other state does.

## Today

The editor's server reads the text of every file the load read and looks for `#define`, a
top-level `[`, and `{func`. It finds what is written plainly, and misses a macro or function
made any other way, such as one a macro expands into.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
