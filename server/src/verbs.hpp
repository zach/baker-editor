#pragma once
// Plan F3's edits of a layer's rows (Baker 0.7.0's verbs and pins), from what the last check
// says of each (Loaded::rows, from Config::layer_effect):
//
//   (items #append lamp)           "Restate as a list": (items rope torch lamp)
//   (items rope torch lamp)        "Write as #append", over (items rope torch): (items #append
//   lamp) (reaction 26)                  "Pin to the value below": (reaction 26 (@was 30))
//   (reaction 26 (@was 28))        a stale pin's quick fix: (reaction 26 (@was 30))
#include "diagnose.hpp"
#include "format.hpp"
#include "structure.hpp"

#include <baker/write.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct RowEdit {
    std::string title;  // as the editor offers it
    std::string kind;   // LSP's: "quickfix" or "refactor.rewrite"
    std::string id;     // "restate", "append", "pin" or "repin"
    TextEdit edit;
};

// The edits for the row of a layer the cursor at `offset` of `file` is in (`text` and its
// structure `s`), by `rows`: the innermost array around it that is one of them.
std::vector<RowEdit> row_edits(const Structure& s, std::string_view text, size_t offset,
                               const std::string& file, const std::vector<LayerRow>& rows);

// `e` widened to the top-level form of `s` it is in, that form laid out by `layout` with `e`
// made: a restated list, which may be long, written as Baker writes it.
void lay_out(const Structure& s, std::string_view text, TextEdit& e,
             const baker::WriteOptions& layout);

}  // namespace baker_lsp
