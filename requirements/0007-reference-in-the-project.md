# 0007: The project file names the game's reference

## The need

An optional `(reference "build/reference.json")` in the project file: where the game's
build writes its reference JSON (`write_reference_json`), relative to the project file like
every other path. `read_project` returns it; a game ignores it.

## Why

Every tool that checks a game's data offline needs the reference to know the game's
functions: an editor, a CI checker, an agent's pre-flight check. Without it, `check` cannot
count a native call's arguments or tell a typo from a real function. The project file is
where tools already learn the rest of the setup, but an unknown key in it is an error, so a
tool cannot add the path itself. Today each tool is told the path separately, which is the
drift the project file exists to end.

## Constraints

- Optional: a project without script functions has no reference.
- A path, not the reference itself: it is a build output, newer than the project file.
- An absent file is not an error for `read_project`; the tool that reads it reports a stale
  or missing reference in its own terms.

## Today

The editor's server takes `--reference FILE` on its command line, so the editor's
configuration repeats a path the game's build already knows.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
