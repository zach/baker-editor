# 0009: Each stack's build writes its reference where its project file names it

## The need

After each stack's game is linked, its build should run the game's `reference --json` and
write the result to a fixed path beside the stack's project file, for example
`stacks/duel/build/reference.json`, ignored by git. The stack's `baker-project.dta` then
names it: `(reference "build/reference.json")`.

The path is relative to the project file, not to the build directory: the build directory is
the developer's choice (`build-stacks/` in the stacks' README, or another), and a tool that
opens the stack knows only the project file. Two build directories writing the same file is
harmless: they write the same vocabulary from the same source.

## Why

A tool that opens a stack (an editor, a CI checker, an agent's pre-flight check) learns the
stack's setup from its project file, and the game's functions and named objects from its
reference. With the reference named there, opening the stack's directory is enough: no
flag repeats a path the build already knows, and the reference is as new as the last build.

The stacks are where tools are tried against real data, so this is also where the project
file's `(reference ...)` gets its first users.

## Constraints

- Written by the build, after linking, so it always matches the binary; a stack not yet built
  has no reference, and tools say so.
- A build step, not a command a person remembers to run.
- No change to the games themselves: `reference --json` exists.

## Today

The editor's server takes `--reference FILE` for the stacks, which applies to every project it
checks: a workspace holding several stacks cannot give each its own reference, so calls are
checked in at most one of them.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
