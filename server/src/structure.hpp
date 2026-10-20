#pragma once
// A whole file's structure, read from its text (which it points into, so the text must
// outlive it): every bracket with its tag and extent, and
// every token with the bracket it is in. One pass, no parse, so it serves text that does not
// read (an unclosed bracket runs to the end of the file). Outline, folding, references and
// rename are built on it.
#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct Bracket {
    char open = '(';         // '(' an array, '{' a command, '[' a macro or property
    int parent = -1;         // the enclosing bracket's index, or -1 at the top
    size_t begin = 0;        // the opening bracket's byte
    size_t end = 0;          // one past the closing bracket (the text's end if unclosed)
    int tag = -1;            // its first token's index in Structure::tokens, or -1
    bool has_child = false;  // holds a bracket of its own
};

struct Token {
    std::string_view text;  // into the file's text: unquoted for a quoted symbol
    size_t begin = 0, end = 0;
    int bracket = -1;  // the innermost enclosing bracket, or -1 at the top
    int index = 0;     // its place in that bracket: 0 is the tag (or command)
    char quote = 0;    // '"' a string, '\'' a quoted symbol, 0 a bare token
};

struct BlockComment {
    size_t begin = 0, end = 0;
};

struct Structure {
    std::vector<Bracket> brackets;  // in order of their opening
    std::vector<Token> tokens;      // in order
    std::vector<BlockComment> comments;

    // The tag text of bracket `b`, or empty.
    [[nodiscard]] std::string_view tag(int b) const;
    // The tags of `b` and its enclosing arrays from the top, if all are tagged arrays; else
    // empty. A data path, as Cursor::data_path gives.
    [[nodiscard]] std::vector<std::string> data_path(int b) const;
};

Structure read_structure(std::string_view text);

// Byte offsets to LSP positions, for many at once: the line starts, found once.
class LineIndex {
public:
    explicit LineIndex(std::string_view text);
    // Line (0-based) and UTF-16 column of byte `offset`.
    [[nodiscard]] std::pair<int, int> position(size_t offset) const;

private:
    std::string_view text_;
    std::vector<size_t> starts_;
};

}  // namespace baker_lsp
