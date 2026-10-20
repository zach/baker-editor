#pragma once
// Outline, folding, references and rename (dev/features-plan.md C1), from each file's
// structure (structure.hpp) and what the last check loaded.
#include "diagnose.hpp"
#include "reference.hpp"
#include "structure.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace baker_lsp {

// LSP's SymbolKind values for what the outline shows.
enum class SymbolKind { Namespace = 3, Field = 8, Function = 12, Constant = 14, Object = 19 };

struct OutlineSymbol {
    std::string name;
    std::string detail;  // a leaf's values, abbreviated
    SymbolKind kind = SymbolKind::Object;
    size_t begin = 0, end = 0;            // the whole bracket
    size_t name_begin = 0, name_end = 0;  // its tag
    std::vector<OutlineSymbol> children;
};

// The file's tagged arrays as a tree, with its macros and {func}s. With more than `limit`
// arrays, leaf fields are left out from the start (cap_outline cuts the rest).
std::vector<OutlineSymbol> outline(std::string_view text, size_t limit = SIZE_MAX);

// At most `limit` symbols, as an editor can show: past it, leaf fields go first, then the
// tree is cut where the count runs out. (VS Code's YAML server caps its outline the same
// way.) Returns how many symbols were left out.
size_t cap_outline(std::vector<OutlineSymbol>& symbols, size_t limit);

struct Fold {
    size_t begin = 0, end = 0;  // bytes; a fold spans their lines
    bool comment = false;
};
// Every bracket and block comment over more than one line.
std::vector<Fold> folds(std::string_view text);

// What a name under the cursor is, for references and rename.
struct Target {
    enum class Kind { Macro, Function, Pack, Field } kind = Kind::Macro;
    std::string name;
    std::string shape;          // a field's shape
    bool renamable = true;      // false for the game's own functions
    size_t begin = 0, end = 0;  // the token under the cursor
};

std::optional<Target> target_at(std::string_view text, size_t offset, const Loaded& loaded,
                                const Reference& reference);

struct Occurrence {
    std::string file;
    size_t begin = 0, end = 0;
    bool quoted = false;  // written as a quoted symbol
};

// Every place `target` is written in `files`, read through `read`.
std::vector<Occurrence> occurrences(
    const Target& target, const std::vector<std::string>& files, const Loaded& loaded,
    const std::function<bool(const std::string&, std::string&)>& read);

// Whether `name` can replace a symbol as written: bare, or quoted for a field.
bool valid_name(const std::string& name, const Target& target);

}  // namespace baker_lsp
