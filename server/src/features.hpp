#pragma once
// Hover and completion: what Baker knows about the place in a file the cursor is at.
//
// The place is read from the text (scan.hpp), so both work while the text does not parse.
// What is known about it comes from the last check (Loaded): the shapes, the layer chain
// (Config::explain), the packs and macros, and the game's functions (Reference).
#include "diagnose.hpp"
#include "reference.hpp"
#include "scan.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace baker_lsp {

// The shape that governs the array whose tags from the root are `tags`, or null.
const baker::shape::Shape* governing(const baker::shape::Set& shapes,
                                     const std::vector<std::string>& tags);

struct Hover {
    std::string markdown;
    size_t begin = 0, end = 0;  // the bytes it is about
    std::string path;           // the data path it is about, as a Path's text; empty if none
};

// The data path the cursor's value or array name is at, as a Path's text; empty if none.
std::string data_path_at(std::string_view text, size_t offset);

std::optional<Hover> hover(std::string_view text, size_t offset, const Loaded& loaded,
                           const Reference& reference);

// LSP's CompletionItemKind values for what completion offers.
enum class Kind {
    Method = 2,
    Function = 3,
    Field = 5,
    Variable = 6,
    Module = 9,
    EnumMember = 20,
    Constant = 21,
};

struct Completion {
    std::string label{};
    Kind kind = Kind::Field;
    std::string detail{};         // a type or a signature
    std::string documentation{};  // markdown
    std::string snippet{};        // an LSP snippet to insert instead of the label, if any
    std::string sort{};           // the order to offer it in, if not the label's
};

struct Completions {
    std::vector<Completion> items;
    size_t begin = 0, end = 0;  // the bytes a chosen item replaces: the token being typed
};

// Reads a project file's text, as the server sees it (an open editor's, else the disk's).
using ReadFile = std::function<bool(const std::string& path, std::string& out)>;

// `file` is the text's own path; with `read`, a symbol value is completed from the values the
// project's files give the same field.
Completions complete(std::string_view text, size_t offset, const Loaded& loaded,
                     const Reference& reference, const std::string& file = {},
                     const ReadFile& read = {});

struct Location {
    std::string file;  // canonical
    int line = 0;      // 1-based; 0 for the file's start
};

// Where the thing under the cursor in `file` is defined:
// - a macro, a script function, a pack: where it is written;
// - a value or an array's name: every other layer that sets it (Config::explain), so from
//   local.dta's override to the default.dta line it shadows;
// - a file named in #include, #require or #merge, or a layer's file in the project file.
// `exists` says whether a file can be read (the editor's buffers, else the disk).
std::vector<Location> definition(std::string_view text, size_t offset, const std::string& file,
                                 const Loaded& loaded, const Reference& reference,
                                 const std::function<bool(const std::string&)>& exists);

}  // namespace baker_lsp
