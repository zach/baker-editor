#pragma once
// Plan F2's code actions between a row and the nested form it stands for (Baker 0.7.0):
//
//   (fighters (kestrel (health 1000) (width 0.6)))     "Write as a row" on (health 1000)
//   (fighters (kestrel (width 0.6)))
//   (/fighters.kestrel.health 1000)                    "Write nested" goes back
//
// Each is built from the text's structure, a new top-level form laid out by the project's
// format (that form alone: what a person laid out by hand stays as it is), and checked with
// Baker to leave the file's tree as it was.
#include "format.hpp"
#include "structure.hpp"

#include <baker/parse.hpp>
#include <baker/write.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct Rewrite {
    std::vector<TextEdit> edits;  // in order, apart
    int fresh_edit = -1;          // the edit whose text ends with a new top-level form, or -1
    size_t fresh = 0;             // where that form starts in its text: apply() lays it out
};

// "Write as a row": the leaf around `offset` (a value, with any metadata, below the top
// level) taken out of its arrays and written as a row after the top-level form that holds it.
// Empty where there is none, or its path cannot be a row's key (a segment with a space or a
// quote; more than 16), or a comment is in its value.
std::optional<Rewrite> as_row(const Structure& s, std::string_view text, size_t offset);

// "Write nested": the row around `offset` as the arrays its key names, where Baker folds it:
// into the form the file already writes for the longest prefix of its key, (units (knight
// ...)) for /units.knight.health, as that form's last child; with none, in its place. Empty
// if the file writes its whole key already.
std::optional<Rewrite> as_nested(const Structure& s, std::string_view text, size_t offset);

// `text` with `r`'s edits made, its new form laid out by `layout`, which `r` then writes too.
std::string apply(std::string_view text, Rewrite& r, const baker::WriteOptions& layout);

// How a file of a project is read on its own, as the project's own reads are: its pack
// levels (hardest first), include directories and defines, and where other files are read
// from (an editor's buffers); none for Baker's defaults and the disk.
struct ReadSetup {
    std::vector<std::string> pack_levels, include_dirs, defines;
    baker::LoadSession::Reader reader;
};

// Whether `before` and `after`, each read by Baker as the text of file `name` by `setup`, give
// the same tree: the same values at the same paths, though a key may move among its siblings
// (a row folds into its record after the keys written there). Else why not, in `why`.
bool same_tree(std::string_view before, std::string_view after, const std::string& name,
               const ReadSetup& setup, std::string& why);

}  // namespace baker_lsp
