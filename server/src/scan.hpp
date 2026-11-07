#pragma once
// Where the cursor is in Baker text, from the text's structure (structure.hpp, Baker's lexer):
// the brackets that enclose it, each with its tag, and the token under it. It needs no parse,
// so it works on text being typed, which often does not parse (an unclosed `(kestrel (hea`).
#include "structure.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct Frame {
    char open = '(';  // '(' an array, '{' a command, '[' a macro or list
    // Its first token, unquoted; empty until one is written. For an infix brace,
    // {2 <= $value <= 10}, its operator: the command's head.
    std::string tag;
    int count = 0;         // tokens and arrays inside it so far
    std::string declared;  // "#pack", "#origin" or "#shape" for a declaration's form; data: empty
    // A row's key's segments, the path it stands for: (/fighters.kestrel.health ...) gives
    // fighters, kestrel, health, and `tag` is the last. Empty for an array that is no row.
    std::vector<std::string> keys;
};

struct Cursor {
    std::vector<Frame> stack;  // the enclosing brackets, outermost first
    // The token the cursor touches, unquoted, and its bytes; empty between tokens, where
    // begin == end == the cursor.
    std::string token;
    size_t begin = 0, end = 0;
    // The token's place in the innermost frame: 0 is the tag (or command), and an infix
    // brace's operator; its operands are 1 and on.
    int index = 0;
    bool quiet = false;      // in a comment or a string: nothing to complete
    bool in_string = false;  // in a string: `token` is its text, which may name a file
    std::string previous;    // the token before the cursor's, in the same bracket or file
    // On a row's key: `token` is the segment the cursor is on, and these the ones before it.
    std::vector<std::string> key_before;

    // The tags of the enclosing arrays, if every enclosing bracket is an array, so the
    // cursor is in data with a path; else empty.
    [[nodiscard]] std::vector<std::string> data_path() const;
    // The same for the brackets around the innermost one: where an array being named sits.
    [[nodiscard]] std::vector<std::string> parent_path() const;
    // The path of the array the cursor's tag names (index 0): parent_path() and the token,
    // or on a row's key, the segments up to the cursor's. Empty if it has none.
    [[nodiscard]] std::vector<std::string> named_path() const;
};

// `s` is `text`'s structure, as read_structure gives it.
Cursor scan(const Structure& s, std::string_view text, size_t offset);

// A Path's text for `tags`, quoting a segment that needs it: menu/'bacon and eggs'/price.
std::string path_text(const std::vector<std::string>& tags);

}  // namespace baker_lsp
