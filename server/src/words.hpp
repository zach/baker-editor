#pragma once
// The words Baker gives `@` metadata and #shape declarations (Baker 0.7.0), for completion and
// hover: what each does, what it takes, and how to write it.
//
//   (speed 4.5 (@range 0 10) (@desc "Run speed"))          data's metadata, after a value
//   #shape (unit (@at units) (@keyed)                      a shape's own words
//      (health int (@range 1 1000) (@optional))           a field's, after its type
//      (@rule "alive" {> $health 0} (@warn)))              a rule's
#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct Word {
    std::string_view name;   // with its @: "@range"
    std::string_view takes;  // its arguments as hover shows them: "LO HI"; empty for none
    std::string_view doc;    // what it does, as markdown
    std::string_view args;   // an LSP snippet of its arguments: " ${1:0} ${2:1}"; empty for none
};

// Where a word may be written.
enum class Place {
    Data,   // after a value in the data
    Shape,  // in #shape (NAME ...), beside its fields
    Field,  // after a shape field's type
    Rule,   // after a (@rule ...)'s expression
};

// The words that may be written at `place`, in the order completion offers them.
std::vector<const Word*> words_at(Place place);

// The word `name` (with its @), or null if Baker has no such word.
const Word* word_named(std::string_view name);

// Hover's markdown for `w`: how it is written, and what it does.
std::string word_markdown(const Word& w);

// The types a shape field may have, as completion offers them after the field's name.
struct TypeWord {
    std::string_view name;     // "int", or "list" and "key_of" for the forms
    std::string_view doc;      // markdown
    std::string_view snippet;  // what choosing it writes, as an LSP snippet
};
const std::vector<TypeWord>& type_words();

}  // namespace baker_lsp
