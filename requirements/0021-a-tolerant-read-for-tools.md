# 0021: A read of text that does not parse, for tools

## The need

A call that reads Baker text the way the parser lexes it, and gives back what is there even
when the text does not parse: every token (its kind: symbol, quoted symbol, string, number,
variable, directive, macro reference; and its byte span), every bracket (`(`, `{`, `[`, its
span, its tag token, its parent), and every comment. An unclosed bracket runs to the end of
the text; an unterminated string or comment ends there; nothing is refused. Errors, if any,
come beside it, located as the parser's are.

## Why

An editor shows a file while it is being typed, and a file being typed rarely parses: `(kestrel
(hea` is the normal state. Every feature that works from the text needs its structure anyway:
the outline and folding, where the cursor is for completion and hover, the references and a
rename, where an inline hint goes, which numbers make a color. Today a tool writes its own
lexer for this, a second copy of Baker's syntax (comments, strings and their escapes, quoted
symbols, hex numbers, directives, macros), which drifts the first time Baker's syntax
changes, and which no test of Baker's holds to Baker's grammar.

The same read serves a formatter, a syntax highlighter that wants Baker's own token kinds, and
an agent that edits a file by its structure.

## Constraints

- The parser's own lexer, so the two cannot disagree about what a token is.
- Linear in the text, no tree of nodes and no allocation per token beyond the output arrays:
  a 2 MB file is read on every keystroke (today's copy takes about 30 ms for one).
- Spans in bytes, as `SourceMap` gives them; a tool converts to its own columns.
- No macro expansion or include following: what is written in this text, only.

## Today

Met by Baker 0.5.0: `read_structure(text, file)` in `<baker/parse.hpp>`. The editor's server
reads with it in place of its own lexer: outline, folding, completion, hover, references,
rename, inlay hints and colors, and gives the same result as before on every file it was
tried on. On a 2 MB file the read takes 19 to 26 ms, against 15 to 18 ms for the server's own
lexer; the server reads an open file once per edit and keeps it for every request, so a
completion at the end of that file answers in 2.5 ms.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
