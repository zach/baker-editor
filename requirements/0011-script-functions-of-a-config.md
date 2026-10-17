# 0011: Listing the script functions of a Config's layers

## The need

`script_functions` should also take what a Config holds: either the Config itself, or the
`#autorun` commands as a span of nodes, so `script_functions(cfg.autorun())` lists the
`{func NAME ...}`s of every enabled file layer, where each is written.

## Why

`script_functions(const LoadSession&)` reads `session.autorun()`. A Config reads each layer
with a session of its own, and keeps the layers' `#autorun` commands in `Config::autorun()`,
not in `read_session().autorun()` (config.hpp says so). So for data loaded the way a game
loads it, through a Config, the call finds nothing. Every tool that opens a project file
loads through a Config: the editor, a checker in CI, an agent.

## Constraints

- The same walk and the same result as for a session: nested `{func}`s and ones a macro
  expands into, at the command's file and line, in load order, duplicates kept.
- A disabled layer's commands are not in `Config::autorun()`; that is right here too.

## Today

The editor's server walks `Config::autorun()` itself (about fifteen lines), repeating what
`script_functions` does for a session.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
