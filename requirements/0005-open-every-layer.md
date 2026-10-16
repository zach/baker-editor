# 0005: Opening a project reports every layer's problem, not only the first

## The need

`open_project` stops at the first layer that does not read and returns its error. A tool
should be able to ask it to go on: push every layer it can, and give back each layer's error
(name and error), so one call reports a syntax error in `default.dta` and another in
`agent.dta` together.

## Why

An editor or a checker shows everything wrong at once; a person fixing errors one per run is
the workflow Baker's located errors are meant to end. A game that stops at the first broken
layer is right to; a tool is not.

## Constraints

- The game's behaviour stays the default: stop at the first error.
- Going on is well defined: a layer that does not read is absent from the stack, as a failed
  `push_layer` leaves it, and the layers above it read with the macros and defines below
  them.
- Allocation only on the error path.

## Today

The editor's server does not call `open_project`. It reads the project with `read_project`
and repeats `open_project`'s steps itself (pack levels, include directories, defines, then
each layer with its `optional` and `enabled`), going on past a layer that fails. That is a
second copy of `open_project`, which drifts as soon as the project file gains a setting.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
