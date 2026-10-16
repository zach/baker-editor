#pragma once
// The server's configuration: how the game loads its data, given on the command line until
// a project file can say it (requirements/0001-project-file.md).
#include <span>
#include <string>
#include <vector>

namespace baker_lsp {

struct Layer {
    std::string name;
    std::string path;  // absolute
    bool optional = false;
};

struct Setup {
    std::vector<Layer> layers;              // bottom to top
    std::vector<std::string> pack_levels;   // hardest first
    std::vector<std::string> include_dirs;  // absolute
    std::vector<std::string> defines;
    // Objects the game names in its Context ({clock thread_task ...}): calls on them are not
    // calls to undefined functions. The reference does not list them yet.
    std::vector<std::string> objects;
    std::string reference;  // the game's reference JSON; empty for none
};

inline constexpr const char* kUsage =
    "usage: baker-lsp [options]\n"
    "  --layer NAME=FILE           a layer, bottom first\n"
    "  --optional-layer NAME=FILE  a layer whose file may be missing\n"
    "  --pack-levels A,B,...       the pack levels, hardest first\n"
    "  --include DIR               an include directory, searched in order\n"
    "  --define NAME               a define, for #ifdef\n"
    "  --object NAME               an object the game names, such as clock\n"
    "  --reference FILE            the game's reference JSON (baker.reference/1, or the\n"
    "                              stacks' {reference, tweaks, shapes}): its functions'\n"
    "                              arities, and calls to undefined functions are errors\n"
    "  --stdio                     speak LSP on stdin and stdout (the default)\n"
    "Relative paths are taken from the working directory.\n";

// Reads `args` (without the program name) into `setup`, or says what is wrong in `error`.
bool parse_args(std::span<const std::string> args, Setup& setup, std::string& error);

// `path` absolute and normalized, resolving symbolic links where it exists, as the server
// compares files.
std::string canonical_path(const std::string& path);

}  // namespace baker_lsp
