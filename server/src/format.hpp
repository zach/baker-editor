#pragma once
// Formatting: text laid out as Baker's writer lays out a tree (baker::tools::format_text),
// keeping its comments, directives and macros, as edits of the whitespace between tokens.
#include <baker/write.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

// Replace [begin, end) of the text with `text`.
struct TextEdit {
    size_t begin = 0, end = 0;
    std::string text;
};

// The edits that lay `text` out by `layout`, touching only the top-level forms that
// [begin, end) does (all of them by default). None if it is laid out already, and none for
// text that does not read, whose first error is then in `error`.
std::vector<TextEdit> format_edits(std::string_view text, const baker::WriteOptions& layout,
                                   std::string& error, size_t begin = 0,
                                   size_t end = std::string_view::npos);

}  // namespace baker_lsp
