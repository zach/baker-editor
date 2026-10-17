# 0012: A stack's game can offer its console write-back, when asked to

## The need

The stacks' games should take a command-line option (for example `--console-write`) that
binds the Config's write tools for the console: `bind_config_tools(ctx, cfg,
{.allow_write = true})`, so `config.write_back` (and `config.promote`) answer on the loopback
console. Without the option, the games stay as they are: writing files stays with the person
at the panel.

## Why

A value tuned in the running game (`config.set`, the panel, an agent's request) lives in
the game's scratch layer until someone writes it back. An editor that shows the live value
next to the line it came from can offer to write it there, through the game's own
`config.write_back`, so the edit is Baker's minimal one-line diff with exact provenance, not
text the editor makes up. Today the stacks never bind it, so the editor can show a live edit
but never write one, and the stacks are where this is tried.

## Constraints

- Off by default, as now; on only when the person starting the game asks for it.
- The console stays on 127.0.0.1.
- Nothing else about the games changes.

## Today

The editor's server offers "Write the live value here" only when the running game binds
`config.write_back` (it asks once per connection). With the stacks, it never does: their
games show live values in hover, and nothing more.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
