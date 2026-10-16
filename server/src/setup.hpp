#pragma once
// The server's configuration: how the game loads its data. A project file (baker-project.dta,
// Baker's read_project) says it for the game and its tools alike; the same setup can also be
// given on the command line, for a game without one.
#include <span>
#include <string>
#include <vector>

namespace baker_lsp {

struct Layer {
    std::string name;
    std::string path;  // absolute
    bool optional = false;
    bool enabled = true;  // in the stack, skipped by the merge
};

inline constexpr const char* kProjectFile = "baker-project.dta";

struct Setup {
    // The project file, canonical. It is read again for every check, so an edit to it takes
    // effect at once. Empty: the layers and the rest below, from the command line.
    std::string project;
    std::vector<std::string> skip;  // the project's layers to leave out, by name

    std::vector<Layer> layers;              // bottom to top
    std::vector<std::string> pack_levels;   // hardest first
    std::vector<std::string> include_dirs;  // absolute
    std::vector<std::string> defines;

    // Objects the game names in its Context ({clock thread_task ...}): calls on them are not
    // calls to undefined functions. The reference does not list them yet.
    std::vector<std::string> objects;
    std::string reference;  // the game's reference JSON; empty for none

    [[nodiscard]] bool by_hand() const {
        return !layers.empty() || !pack_levels.empty() || !include_dirs.empty() || !defines.empty();
    }
};

inline constexpr const char* kUsage =
    "usage: baker-lsp [options]\n"
    "  --project FILE              the project file; by default, baker-project.dta at the\n"
    "                              root of the workspace the editor opens\n"
    "  --skip LAYER                leave one of the project's layers out, such as local\n"
    "for a game without a project file, instead:\n"
    "  --layer NAME=FILE           a layer, bottom first\n"
    "  --optional-layer NAME=FILE  a layer whose file may be missing\n"
    "  --pack-levels A,B,...       the pack levels, hardest first\n"
    "  --include DIR               an include directory, searched in order\n"
    "  --define NAME               a define, for #ifdef\n"
    "either way:\n"
    "  --reference FILE            the game's reference JSON (baker.reference/1, or the\n"
    "                              stacks' {reference, tweaks, shapes}): its functions'\n"
    "                              arities, and calls to undefined functions are errors\n"
    "  --object NAME               an object the game names, such as clock\n"
    "  --stdio                     speak LSP on stdin and stdout (the default)\n"
    "Relative paths are taken from the working directory.\n";

// Reads `args` (without the program name) into `setup`, or says what is wrong in `error`.
// Neither a project nor layers is not an error: the server then looks for the project file
// in the workspace the editor opens.
bool parse_args(std::span<const std::string> args, Setup& setup, std::string& error);

// `path` absolute and normalized, resolving symbolic links where it exists, as the server
// compares files.
std::string canonical_path(const std::string& path);

}  // namespace baker_lsp
