# tree-sitter-baker

A [tree-sitter](https://tree-sitter.github.io) grammar for [Baker](https://github.com/zach/baker)
data files (`.dta`), with highlight queries, for editors that highlight with tree-sitter:
Neovim, Helix and Zed. VS Code uses the TextMate grammar in `../vscode`.

The tree is Baker's surface syntax: arrays `(...)`, commands `{...}`, brackets `[...]`
(macros and properties), numbers (with hex), strings, quoted symbols, variables, directives
(`#include`, `#ifdef`, ...) and both kinds of comment. An array's tag, a command's function
and a bracket's name are its first child, which the queries highlight.

```sh
npm ci
npx tree-sitter generate                              # src/ from grammar.js (checked in)
npx tree-sitter test --config-path test/config.json   # the corpus and highlight tests
```

Every `.dta` file in Baker's repository (61, the stacks' included) parses without an error;
a 2 MB file parses at about 8 MB/s on first read.

## Using it

`.dta` is also another format's extension, so an editor should treat a `.dta` file as Baker
only under a `baker-project.dta`.

**Neovim** (nvim-treesitter):

```lua
require("nvim-treesitter.parsers").get_parser_configs().baker = {
  install_info = {
    url = "https://github.com/zach/baker-editor",
    location = "tree-sitter-baker",
    files = { "src/parser.c" },
    branch = "main",
  },
  filetype = "baker",
}
vim.filetype.add({ pattern = { [".*%.dta"] = function(path)
  return vim.fs.root(path, "baker-project.dta") and "baker" or nil
end } })
```

Then `:TSInstall baker`, and copy `queries/highlights.scm` to
`~/.config/nvim/queries/baker/highlights.scm`.

**Helix** (`languages.toml`; then `hx --grammar fetch && hx --grammar build`, and copy the
queries to `~/.config/helix/runtime/queries/baker/`):

```toml
[[language]]
name = "baker"
scope = "source.baker"
file-types = [{ glob = "*.dta" }]
roots = ["baker-project.dta"]
comment-token = ";"
language-servers = ["baker-lsp"]

[language-server.baker-lsp]
command = "baker-lsp"

[[grammar]]
name = "baker"
source = { git = "https://github.com/zach/baker-editor", rev = "main", subpath = "tree-sitter-baker" }
```

**Zed** loads grammars from extensions: a Zed extension for Baker would name this grammar
(repository and `tree-sitter-baker` path) and carry the queries. None is published yet.

## License

MIT.
