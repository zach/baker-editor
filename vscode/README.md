# Baker for VS Code

Diagnostics, hover, completion and go-to-definition for [Baker](https://github.com/zach/baker)
data files, from `baker-lsp`, which runs Baker's own load and check.

A `.dta` file is Baker's when a `baker-project.dta` is at or above it: the extension activates
in a workspace that holds one, and gives such files the language `baker`. (`.dta` is also
another format's extension, which it leaves alone everywhere else.)

## Settings

- `baker.server.path`: the `baker-lsp` binary. Empty: the one bundled in the extension, else
  `baker-lsp` on the PATH.
- `baker.server.args`: arguments for it, such as `["--reference", "build/reference.json"]`
  for a project whose project file names no reference.

The command **Baker: Restart the language server** restarts it, for example after rebuilding
the game's reference.
