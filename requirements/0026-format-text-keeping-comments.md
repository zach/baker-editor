# 0026: Lay out text as Baker writes it, keeping its comments

## The need

A call that takes Baker text and gives it back laid out as Baker's own writer lays out a tree
(`write_dta`, with its `WriteOptions`), but keeping everything the text says that a tree does
not: every comment, every `#` directive, each macro definition and use as written, string
and number spellings, and the blank lines that separate top-level forms. It works from the
tolerant read (note 0021); text that does not parse comes back unchanged, with the read's
errors.

Also:

- **A range:** only the top-level forms that a byte span touches are laid out, the rest of
  the text unchanged, so an editor formats a selection or the forms an edit touched.
- **A project's layout:** the indent and line width a project's files are written with,
  read from the project file (for example `(format (indent 3) (width 100))`), so a tool
  formats each project as it is written. Baker's stacks are written with a three-space indent;
  `WriteOptions`' default is two.
- **The edits, or the text:** the result as the text, or as the smallest replacements that
  turn the old text into it, which an editor applies without moving the cursor.

## Why

Nothing lays out Baker text today. A person aligns brackets by hand; an agent's edit leaves
whatever indentation it wrote; and each tool that writes a file (write-back, promote, an
editor's color picker or rename, a migration script) writes as it can. A formatter in each
tool would be another copy of Baker's syntax and of its writer's rules, and would disagree
with what Baker writes. The same call serves an editor's *Format Document*, a CI step that
fails on an unformatted file, a pre-commit hook, and a tool that writes a file and wants it
as a person would have written it.

## Constraints

- What it gives back reads as the same tree as what it was given, macros unexpanded, and
  the same comments in the same order; formatting twice changes nothing the second time.
- A trailing `;` comment stays on the line of the token it follows; a block comment stays
  where it is between tokens. How a run of aligned trailing comments is kept is Baker's to
  decide, and to say.
- Linear in the text: a 2 MB file within about 100 ms, allocation only for the output.
- No file is read: an `#include` is laid out as written, not followed.

## Today

The editor offers no formatting: plan E's step E4 waits on this note, rather than build a
second set of rules in the editor.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
