#pragma once
// The stack as the game sees it, as text: the merged tree with each value's source, and what
// one layer changes. Both are read-only views the editor shows beside the files; each line
// that comes from a file says where, so go-to-definition can jump there.
#include <baker/config.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct ViewSource {
    int line = 0;       // 0-based, in the view's text
    std::string file;   // canonical
    int file_line = 0;  // 1-based
};

struct View {
    std::string text;
    std::vector<ViewSource> sources;  // by line, ascending
};

// Config::root(), one value a line, each followed by "; layer file:line" (`dir` cut from the
// file names). It reads back through Baker into the same tree.
View merged_view(const baker::Config& cfg, std::string_view dir);

// Config::layer_effect(layer), a line a path, as a game's `diff --layer` prints it; empty, with
// `error` set, when the stack has no such layer.
std::optional<View> layer_view(const baker::Config& cfg, std::string_view layer,
                               std::string& error);

}  // namespace baker_lsp
