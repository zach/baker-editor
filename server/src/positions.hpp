#pragma once
// Positions: Baker locates a problem by line and column in code points (1-based, 0 when
// unknown); LSP wants a range of 0-based lines and UTF-16 code units. File URIs map to
// the paths Baker reads.
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct Position {
    int line = 0;       // 0-based
    int character = 0;  // UTF-16 code units from the line's start
};

struct Range {
    Position start, end;
};

// The text of line `line` (1-based), without its line break; empty past the end.
std::string_view line_text(std::string_view text, int line);

// UTF-16 code units in the first `code_points` code points of `line` (all of it if shorter).
int utf16_units(std::string_view line, int code_points);

// The byte offset in `text` of an LSP position (UTF-16 units), clamped to its line.
size_t offset_of(std::string_view text, Position p);

// The LSP position of byte `offset` in `text`.
Position position_of(std::string_view text, size_t offset);

// The range to underline for a problem at `line`:`column` (Baker's, 1-based):
// - a known column: the token that starts there, at least one character;
// - column 0 (a problem located by its array's line): the array `(key` on that line if
//   `key` is given and the line has one, else the line's first token, with the symbol
//   after an opening bracket, so `(price 3)` underlines `(price`;
// - line 0 (no location): the start of the file.
Range problem_range(std::string_view text, int line, int column, std::string_view key = {});

// The last element of an error's array path, unquoted: menu/'bacon and eggs' -> bacon and eggs.
std::string last_path_element(std::string_view path);

// file:///a/b%20c.dta <-> /a/b c.dta. Only file URIs map; others give "".
std::string path_from_uri(std::string_view uri);
std::string uri_from_path(std::string_view path);

// Where each line of one text is in a later version of it: the lines both keep, matched
// in order (the same first and last lines, then the fewest lines removed and put in between,
// as a line diff gives them), so that something located in the first text can be shown in
// the second. A line that was edited, removed or replaced has no place; so has every line
// between the first and last edits once there are more than kMaxEdits lines of edits.
class LineMap {
public:
    static constexpr size_t kMaxEdits = 1000;
    LineMap(std::string_view before, std::string_view after);
    // Line `line` (1-based) of `before`, in `after`; empty if it has no place there.
    [[nodiscard]] std::optional<int> line(int line) const;

private:
    std::vector<int> to_;  // by line of `before`, 0-based: its line in `after`, or -1
};

}  // namespace baker_lsp
