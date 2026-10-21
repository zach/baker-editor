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
    std::string tag;  // its first token, unquoted; empty until one is written
    int count = 0;    // tokens and arrays inside it so far
};

struct Cursor {
    std::vector<Frame> stack;  // the enclosing brackets, outermost first
    // The token the cursor touches, unquoted, and its bytes; empty between tokens, where
    // begin == end == the cursor.
    std::string token;
    size_t begin = 0, end = 0;
    int index = 0;           // the token's place in the innermost frame: 0 is the tag (or command)
    bool quiet = false;      // in a comment or a string: nothing to complete
    bool in_string = false;  // in a string: `token` is its text, which may name a file
    std::string previous;    // the token before the cursor's, in the same bracket or file

    // The tags of the enclosing arrays, if every enclosing bracket is an array, so the
    // cursor is in data with a path; else empty.
    [[nodiscard]] std::vector<std::string> data_path() const;
    // The same for the brackets around the innermost one: where an array being named sits.
    [[nodiscard]] std::vector<std::string> parent_path() const;
};

// `s` is `text`'s structure, as read_structure gives it.
Cursor scan(const Structure& s, std::string_view text, size_t offset);

// A Path's text for `tags`, quoting a segment that needs it: menu/'bacon and eggs'/price.
std::string path_text(const std::vector<std::string>& tags);

}  // namespace baker_lsp
