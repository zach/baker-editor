# 0027: The migrations as a call that reports and rewrites a text

## The need

A call that takes Baker text and finds in it each form a language migration rewrites, and
gives back, for each, where it is, what it is, and the edits that rewrite it, as the
migrators in `tools/` would (`baker_migrate_metadata`, `baker_migrate_declarations`):

- **The forms:** a first `(pack NAME ...)`; an `(origin "source" ...)` first or after the
  pack; a top-level `(shapes (a ...) (b ...))`; `#define NAME (body)` at the top level; a
  child tagged `range`, `step`, `desc`, `note`, `kind`, `group` or `one_of` in the trailing run
  after a value; inside an old `(shapes ...)` the shape words and a field's words without
  their `@`; a `{func}`'s `(status ...)` and `(doc ...)`. And whatever a later release's
  migrator adds: one call for every migration Baker has, each finding naming its migration.
- **Each finding:** its byte span (the form, or the tag to rename), a message as a check's
  warning words it ("(shapes ...) is data; a file declares each shape with #shape (NAME
  ...)"), the migration it belongs to, and its edits: the smallest replacements of byte
  spans of the given text, as `format_text` gives its edits.
- **One finding, or the whole text:** a tool applies one finding's edits alone, or all of
  them at once; applying them all is the text the migrator writes for the file.
- **What it will not decide:** the cases the migrators report and leave alone (an array
  with no untagged value whose last child is tagged with a metadata word) come back as
  findings without edits, saying why.

## Why

Baker 0.7.0 changed how metadata and declarations are spelled, and every project, its
tools and its documentation migrate. The migrators rewrite whole files from a shell; a tool
that works on one file at a time has no way to use their rules. With this call:

- **An editor** shows each old form where it is written, in every file of the project and
  not only a candidate layer, and offers its rewrite as a fix for that form or the whole
  file. Today a check names the old forms only for a candidate layer (`check_layer`), so a
  project's own files, where the old forms are, show nothing.
- **A CI step or a pre-commit hook** fails on a file with an old form and prints the
  replacement, without running a migrator on the tree.
- **An agent** writing Baker text from older examples is told what to change, located, and
  given the edit, the way it is given a formatter's.
- **The migrators themselves** become a loop over files around this call, so their rules
  live in one place, as the formatter's do.

## Constraints

- The edits change nothing but what the migrator changes: comments, layout, macros and the
  forms' insides stay as written, and a second call on the result finds nothing.
- Reads tolerantly (as `read_structure` does): text that does not parse still gets the
  findings it can locate.
- Text in, findings out: no file is read, no `#include` followed. Linear in the text: a 2 MB
  file within about 50 ms.
- The C++ and Markdown runs (`--cpp`, `--md`) stay the migrators' business; this call takes
  Baker text alone.

## Today

The editor shows nothing for an old form in a project's files: the check warns only for a
candidate layer. Its README names the two migrators to run. A quick fix waits for this
call, so that the editor does not keep a second copy of the migrators' rules.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
