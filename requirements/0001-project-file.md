# 0001: A project file that describes how a game loads its data

## The need

A tool other than the game must be able to load a project's data exactly as the game does,
without running the game's code. Today each game states its load setup in C++ before
the first `push_layer`:

- the layer stack: each layer's name, its file, and whether it is optional (for example
  `default`, then optional `local` and `agent`);
- the pack levels, hardest first (for example `engine`, `game`, `task`);
- the include directories;
- the defines, as flags or macros, that the data's `#ifdef`s read.

Baker should give that setup a home in data: a file in Baker text at the project's root (for
example `baker-project.dta`) that names these four things, and a public call that reads it
and applies it to a `Config` (its `LoadSession` and its layers). The game's own startup reads
the file through that call, so the game and every tool share one description and cannot
drift apart.

What a reader of the file needs to get out of it:

- the same `Config` the game would build, with the same layers in the same order, where the
  caller may still add what only a running game has (validators, a reader for unsaved
  buffers, a time source, its Context's functions);
- the location of every entry in the project file, so a tool can report an error in the
  project file itself at its line and column;
- a way to find the project file from a data file: the nearest one in that file's directory
  or the directories above it.

## Why

Any tool that checks, explains or completes a project's data offline needs the game's load
setup: an editor's language server, a command-line checker in CI, a review bot, an agent
editing files. Each of them re-stating the setup in its own configuration drifts from the
game's. One project file that the game itself reads is the setup's single source.

The file's presence also marks a directory as a Baker project, so a tool knows that
`.dta` files there are Baker's.

## Constraints

- Reading the project file is a load-time cost only, paid once per open and once per edit of
  the project file. Nothing changes on the per-frame path, and the file adds nothing to a
  shipping build that does not read it.
- Paths in the file are relative to the file's own directory, so a checkout works wherever
  it sits.
- The setup must be complete for the stacks in baker-dev `stacks/`: `base::Live::open`
  should be able to take its layers, pack levels and include directory from the file and
  keep only what is code (validators, tools, autorun, the watcher). A setting it can't
  express is a gap in the format, not a reason for the game to keep a second copy.
- A setting that depends on the build or the platform (for example, a layer only in
  development builds) needs a way to say so: a define the caller supplies, which the file
  tests, is enough.
- The reading call allocates through the Config's allocator, like any other load.

## Today

The editor's server takes the setup as command-line arguments: each layer (`--layer
NAME=FILE`, `--optional-layer`), `--pack-levels`, `--include` and `--define`. That duplicates
each game's setup by hand: the stacks' `base::Live::open` states the same three layers, pack
levels and include directory in C++, and the two drift apart as soon as either changes.

## Not included

This note contains no code, identifiers, structure or documentation from any other product, and
nothing confidential.
