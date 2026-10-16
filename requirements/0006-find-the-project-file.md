# 0006: Finding the project file that governs a path

## The need

A call that, given a path, returns the nearest project file: `baker-project.dta` in the
path's directory or the closest directory above it, or nothing. The file name is Baker's
convention, so the search belongs with it.

## Why

A tool is rarely opened exactly at a project's root:

- an editor opened at a repository holding several games (Baker's own `stacks/` holds
  seven) has one project per game, and each open file belongs to the nearest one;
- a command-line checker given a data file should check it in the stack that loads it;
- an agent editing one file needs its project without being told.

Every tool writing this search for itself gets the stopping rule (the file system root, a
repository root) slightly differently.

## Constraints

- Through the host's reader, as `read_project` is, so it works for content that is not on
  the disk; or with a host-supplied "does this file exist" check.
- No cost to a game: tools call it.

## Today

The editor's server looks only at the root of the workspace the editor opened. A workspace
holding several games finds no project, and nothing is checked.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
