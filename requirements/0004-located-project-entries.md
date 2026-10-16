# 0004: Each entry of a project file keeps where it is written

## The need

`read_project` should give every entry it returns its line and column in the project file:
each layer (its name, and its path), each pack level, include directory and define. An error
`open_project` or `push_layer` reports about a layer whose file is missing or unreadable
could then be located at the entry that names it, not only at the missing file's path.

## Why

A tool that shows a project file to a person needs to point into it:

- an editor underlines `(default "data/defualt.dta")` when that file does not exist;
- go-to-definition from a layer's entry to its file, and from a value in `local.dta` back to
  the project entry that makes it a layer;
- a checker in CI names the project line that broke the build.

Today the missing file's error has no location in the project file, so the tool can only
point at the file's first line.

## Constraints

- Positions as Baker's errors give them: 1-based lines, columns in code points.
- Reading a project file stays a load-time cost; nothing per frame.

## Today

The editor's server reports a missing layer on the project file's first line, with the
missing path in the message.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
