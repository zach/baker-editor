#pragma once
// Plan F4's edits of braces written infix (Baker 0.7.0), {2 <= $value <= 10}:
//
//   {0.3 <= $value 0.7}         "Insert the missing operator": {0.3 <= $value <= 0.7}
//   {0 < $value <= 10}          "Nest under and": {and {0 < $value} {$value <= 10}}
//   {<= 0.3 $value 0.7}  <->    {0.3 <= $value <= 0.7}, "Write infix" and "Write prefix"
//
// Which brace is malformed, and how a brace reads, is Baker's: its text is read with Baker's
// own reader, before an edit and after it.
#include "format.hpp"
#include "structure.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct BraceEdit {
    std::string title;
    std::string kind;  // LSP's: "quickfix" or "refactor.rewrite"
    TextEdit edit;     // the brace's text, replaced
};

// The quick fixes for the malformed infix brace around `offset`: the missing operator put
// between every two operands, and a chain of comparisons with two operators nested under
// `and`. Each reads infix as Baker reads it, with no error.
std::vector<BraceEdit> infix_fixes(const Structure& s, std::string_view text, size_t offset);

}  // namespace baker_lsp
