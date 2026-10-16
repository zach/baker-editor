#pragma once
// Positions: Baker locates a problem by line and column in code points (1-based, 0 when
// unknown); LSP wants a range of 0-based lines and UTF-16 code units. File URIs map to
// the paths Baker reads.
#include <string>
#include <string_view>

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

}  // namespace baker_lsp
