#pragma once
// The server's configuration: how the game loads its data. A project file (baker-project.dta,
// Baker's read_project) says it for the game and its tools alike; the same setup can also be
// given on the command line, for a game without one.
#include <cstdint>
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

struct Setup {
    // The project file, canonical. It is read again for every check, so an edit to it takes
    // effect at once. Empty: the layers and the rest below, from the command line.
    std::string project;
    std::vector<std::string> skip;  // the project's layers to leave out, by name

    std::vector<Layer> layers;              // bottom to top
    std::vector<std::string> pack_levels;   // hardest first
    std::vector<std::string> include_dirs;  // absolute
    std::vector<std::string> defines;

    // The game's reference JSON: from --reference, else the project file's (reference ...).
    std::string reference;
    // The objects the game names ({clock thread_task ...}), from the reference: calls on
    // them are not calls to undefined functions.
    std::vector<std::string> objects;
    // The running game's console, on 127.0.0.1; 0: none.
    uint16_t console_port = 4555;
    // Inlay hints show the running game's value where a live edit differs from the files.
    bool live_inlays = false;

    [[nodiscard]] bool by_hand() const {
        return !layers.empty() || !pack_levels.empty() || !include_dirs.empty() || !defines.empty();
    }
};

inline constexpr const char* kUsage =
    "usage: baker-lsp [options]\n"
    "  --project FILE              the project file; by default, each open file's nearest\n"
    "                              baker-project.dta, at or above it\n"
    "  --skip LAYER                leave one of the project's layers out, such as local\n"
    "for a game without a project file, instead:\n"
    "  --layer NAME=FILE           a layer, bottom first\n"
    "  --optional-layer NAME=FILE  a layer whose file may be missing\n"
    "  --pack-levels A,B,...       the pack levels, hardest first\n"
    "  --include DIR               an include directory, searched in order\n"
    "  --define NAME               a define, for #ifdef\n"
    "either way:\n"
    "  --reference FILE            the game's reference JSON (baker.reference/1, or the\n"
    "                              stacks' {reference, tweaks, shapes}), if the project\n"
    "                              file names none: its functions and named objects, so\n"
    "                              calls are checked and undefined ones are errors\n"
    "  --console-port N            a running game's console on 127.0.0.1 (default 4555;\n"
    "                              0: none): hover shows its live values\n"
    "  --live-inlays               show the game's live value beside each value it\n"
    "                              differs from, as an inlay hint\n"
    "  --stdio                     speak LSP on stdin and stdout (the default)\n"
    "Relative paths are taken from the working directory.\n";

// Reads `args` (without the program name) into `setup`, or says what is wrong in `error`.
// Neither a project nor layers is not an error: the server then finds each open file's
// project.
bool parse_args(std::span<const std::string> args, Setup& setup, std::string& error);

// `path` absolute and normalized, resolving symbolic links where it exists, as the server
// compares files.
std::string canonical_path(const std::string& path);

}  // namespace baker_lsp
