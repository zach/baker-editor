#pragma once
// Semantic highlighting: what each token is, beyond its shape, from Baker's own lexer
// (structure.hpp) and what the last check knows: a macro the data defines, a directive, a
// variable; a call's name as one of the data's {func}s, the game's functions, a named object
// and its methods; an array's name that is a game's tweak, and a value a higher layer
// shadows. Strings, numbers and plain symbols are left to the editor's grammar: on a large
// file they are most of its tokens, and the grammar colors them the same.
#include "diagnose.hpp"
#include "reference.hpp"
#include "structure.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace baker_lsp {

// LSP's token types and modifiers as the server names them, in the legend's order.
enum class TokenType : uint8_t { Macro, Keyword, Variable, Function, Method, Property };
inline constexpr const char* kTokenTypes[] = {"macro",    "keyword", "variable",
                                              "function", "method",  "property"};
enum TokenModifier : uint32_t {
    kDeclaration = 1u << 0,     // where a macro or a {func} is defined
    kDefaultLibrary = 1u << 1,  // the game's own: its functions and named objects
    kTweak = 1u << 2,           // an array the game reads as a tweak
    kShadowed = 1u << 3,        // a value a higher layer overrides
};
inline constexpr const char* kTokenModifiers[] = {"declaration", "defaultLibrary", "tweak",
                                                  "shadowed"};

struct SemanticToken {
    size_t begin = 0, end = 0;  // bytes
    TokenType type = TokenType::Macro;
    uint32_t modifiers = 0;
};

// The tokens of `read` worth a color of their own, in order. `loaded` (the last check's, or
// null) gives the data's macros and functions; `shadowed` the lines (1-based) and array names
// a higher layer overrides in this file, as they are in this text now.
std::vector<SemanticToken> semantic_tokens(
    const FileRead& read, const Loaded* loaded, const Reference& reference,
    const std::vector<std::pair<int, std::string>>& shadowed);

}  // namespace baker_lsp
