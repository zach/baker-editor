#pragma once
// A whole file's structure, read from its text (which it points into, so the text must
// outlive it): every bracket with its tag and extent, every token with the bracket it is in,
// and every comment. Baker's own lexer reads it (baker::tools::read_structure), so it serves text
// that does not parse (an unclosed bracket runs to the end of the file) and cannot disagree
// with Baker about what a token is. Outline, folding, references and rename are built on it.
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace baker_lsp {

struct Bracket {
    char open = '(';   // '(' an array, '{' a command, '[' a macro or property
    int parent = -1;   // the enclosing bracket's index, or -1 at the top
    size_t begin = 0;  // the opening bracket's byte
    size_t end = 0;    // one past the closing bracket (the text's end if unclosed)
    int tag = -1;      // its first token's index in Structure::tokens, or -1
    // Holds a bracket of its own other than metadata: (health 100 (@range 1 9)) holds none.
    bool has_child = false;
    bool closed = false;  // false: it runs to the text's end
    int index = 0;        // its place in its parent, as Token::index
    // The #pack, #origin or #shape token in Structure::tokens whose one form it is, at the
    // top level: a declaration, which is not data. -1 for data.
    int declares = -1;
    // A row, (/fighters.kestrel.health 1100): its key's segments in Structure::segments, from
    // `segment`, `segments` of them. 0 for an array that is no row, or a key that does not
    // read (an empty segment, an element's, more than 16).
    int segment = 0, segments = 0;
    // A brace that reads infix, {0.3 <= $value <= 0.7}: its first operator's token in
    // Structure::tokens, the command's head; -1 for any other bracket.
    int infix = -1;
};

// A segment of a row's key: (/fighters.kestrel.health 1100) has three, each the key of an
// array the row stands for.
struct Segment {
    std::string_view name;      // into the file's text, unquoted
    size_t begin = 0, end = 0;  // its bytes, quotes included
};

// What a token is, as Baker's lexer reads it.
enum class TokenKind : uint8_t {
    Symbol,
    QuotedSymbol,  // '...'
    String,        // "..."
    Int,
    Float,
    Variable,   // $name
    Directive,  // #name
    Macro,      // a use of a macro this text defines ([name ...] at the top level)
    PathKey,    // a row's key: (/a.b.c ...) at the top level
};

struct Token {
    std::string_view text;  // into the file's text: unquoted for a quoted symbol
    size_t begin = 0, end = 0;
    int bracket = -1;  // the innermost enclosing bracket, or -1 at the top
    int index = 0;     // its place in that bracket: 0 is the tag (or command)
    char quote = 0;    // '"' a string, '\'' a quoted symbol, 0 a bare token
    TokenKind kind = TokenKind::Symbol;
};

struct Comment {
    size_t begin = 0, end = 0;  // a ; comment ends before its line's end
    bool block = false;         // /* */
};

struct Structure {
    std::vector<Bracket> brackets;  // in order of their opening
    std::vector<Token> tokens;      // in order
    std::vector<Comment> comments;  // in order
    std::vector<Segment> segments;  // the rows' keys', by Bracket::segment

    // Whether bracket `b` is metadata: an array tagged @ and more, (@range 0 10). A lone @ is a
    // symbol.
    [[nodiscard]] bool is_metadata(int b) const;
    // The directive that declares bracket `b` ("#shape", "#pack", "#origin"), or empty.
    [[nodiscard]] std::string_view declared_by(int b) const;
    // The head of command `b`: its first token, or an infix brace's operator. Empty if none.
    [[nodiscard]] std::string_view head(int b) const;
    // Whether token `t` is the head of the command it is in: its first, or an infix brace's
    // operator, each of them ({2 <= $v <= 10} has two).
    [[nodiscard]] bool is_head(size_t t) const;
    // The tag text of bracket `b`, or empty: a row's whole key, /fighters.kestrel.health.
    [[nodiscard]] std::string_view tag(int b) const;
    // The key bracket `b` is named by: its tag, or a row's last segment (health). Empty if none.
    [[nodiscard]] std::string_view name(int b) const;
    // Whether bracket `b` is a row whose key reads, and its key's segments.
    [[nodiscard]] bool is_row(int b) const;
    [[nodiscard]] std::span<const Segment> key(int b) const;
    // The tags of `b` and its enclosing arrays from the top, if all are tagged arrays, a row's
    // key as its segments; else empty. A data path, as Cursor::data_path gives.
    [[nodiscard]] std::vector<std::string> data_path(int b) const;
};

Structure read_structure(std::string_view text);

// Whether `name` is written bare in a row's key: a symbol with no separator, not all digits.
// Else it is quoted, '...'.
bool bare_segment(std::string_view name);

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

// A file's text with its structure and line index: read once, for every request on that
// text (the server keeps one per open file until its text changes).
struct FileRead {
    explicit FileRead(std::string t)
        : text(std::move(t)), structure(read_structure(text)), lines(text) {}
    FileRead(const FileRead&) = delete;
    FileRead& operator=(const FileRead&) = delete;

    const std::string text;
    const Structure structure;  // into `text`
    const LineIndex lines;
};

}  // namespace baker_lsp
