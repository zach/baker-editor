#include "words.hpp"

#include <array>

namespace baker_lsp {
namespace {

// Baker's docs/language.md, "Metadata", "Shapes" and "Rules between values", in short.
constexpr std::array kWords{
    Word{"@range", "LO HI",
         "The value's range: a tuning panel's slider, and a value outside it is refused (a "
         "shape's range is an error at check). A curve's range holds each point's y.",
         " ${1:0} ${2:1}"},
    Word{"@step", "STEP", "The step a tuning panel moves the value by.", " ${1:1}"},
    Word{"@kind", "KIND",
         "How a tool presents the value: `color`, `angle`, `percent`, `path`, ... Without it, "
         "3 or 4 numbers ranged 0 to 1 are a color.",
         " ${1:color}"},
    Word{"@desc", "\"TEXT\"", "What the value is, for whoever tunes it.", " \"$1\""},
    Word{"@note", "\"TEXT\"",
         "A remark from whoever tuned the value for whoever tunes it next. Data only: the game "
         "never reads it.",
         " \"$1\""},
    Word{"@one_of", "SYMBOL...",
         "The symbols the value may be, in the order a tool offers them; any other is refused.",
         " ${1:a} ${2:b}"},
    Word{"@group", "NAME", "The group a tuning panel shows the value in.", " ${1:name}"},
    Word{"@was", "VALUE...",
         "A layer's pin: the value below this layer when the row was written. Once the layers "
         "below move, a check warns at the row.",
         " ${1:value}"},
    Word{"@at", "TAG...",
         "Where the shape governs: arrays whose ancestors' tags from the root are these, `*` "
         "any one. Without it, every array tagged with the shape's name.",
         " ${1:table}"},
    Word{"@keyed", "",
         "The shape governs every child of the arrays at its `(@at ...)`, whatever its tag: "
         "keyed records, which layers merge by name.",
         ""},
    Word{"@optional", "", "The field may be left out; a field without it is required.", ""},
    Word{"@rule", "\"MESSAGE\" {EXPRESSION}",
         "What must hold between a record's values: `$field` reads the record's field, "
         "`{data PATH...}` the rest of the tree. A false expression is an error at the record, "
         "a warning with `(@warn)`.",
         " \"${1:message}\" {${2:expression}}"},
    Word{"@warn", "", "The rule's failure is a warning, not an error.", ""},
    // Baker 0.8.0: a record's identity, and a shape's C++ view.
    Word{"@id", "HEX",
         "The record's identity, 32 hex digits, minted by a tool (`format --baker-mint-ids`): "
         "it travels with the record and survives a rename of its key.",
         " ${1:id}"},
    Word{"@aka", "OLD",
         "A key the record had: a layer or a `(key_of ...)` naming the old key reaches it "
         "(a key wins over an alias).",
         " ${1:old}"},
    Word{"@renamed_from", "OLD",
         "The field's old name: a record spelling it is checked as the field, with a warning, "
         "and `format` rewrites it.",
         " ${1:old}"},
    Word{"@view", "[no | by_id]",
         "On a shape, generate a C++ struct its records are read into; `(@view by_id)` on a "
         "keyed shape also keeps each record's `(@id ...)` in its baked table, for a lookup by "
         "ID. On a field, `(@view no)` leaves it out of the struct.",
         ""},
    Word{"@cxx", "TYPE",
         "The C++ type of the field's member in the view: `int8` to `uint64` for an `int`, "
         "`float` or `double` for a `float`.",
         " ${1:int64}"},
    Word{"@cxx_name", "NAME",
         "The C++ name of the view's struct or member, where the data's is not a C++ name.",
         " ${1:name}"},
};

constexpr std::string_view kData[] = {"@range",  "@step",  "@kind", "@desc", "@note",
                                      "@one_of", "@group", "@was",  "@id",   "@aka"};
constexpr std::string_view kShape[] = {"@at",   "@keyed", "@rule",    "@desc",
                                       "@note", "@view",  "@cxx_name"};
constexpr std::string_view kField[] = {"@range", "@step",   "@kind",     "@desc",
                                       "@note",  "@one_of", "@optional", "@renamed_from",
                                       "@view",  "@cxx",    "@cxx_name"};
constexpr std::string_view kRule[] = {"@warn"};

}  // namespace

std::vector<const Word*> words_at(Place place) {
    std::vector<const Word*> out;
    const auto add = [&out](const auto& names) {
        for (const std::string_view n : names) out.push_back(word_named(n));
    };
    switch (place) {
        case Place::Data: add(kData); break;
        case Place::Shape: add(kShape); break;
        case Place::Field: add(kField); break;
        case Place::Rule: add(kRule); break;
    }
    return out;
}

const Word* word_named(std::string_view name) {
    for (const Word& w : kWords)
        if (w.name == name) return &w;
    return nullptr;
}

std::string word_markdown(const Word& w) {
    std::string s = "`(" + std::string(w.name);
    if (!w.takes.empty()) s += " " + std::string(w.takes);
    return s + ")`\n\n" + std::string(w.doc);
}

const std::vector<TypeWord>& type_words() {
    static const std::vector<TypeWord> kTypes{
        {"int", "An integer.", "int"},
        {"float", "A number.", "float"},
        {"bool", "`0` or `1`, `true` or `false`.", "bool"},
        {"symbol", "A symbol; `(@one_of ...)` lists the ones it may be.", "symbol"},
        {"string", "A string.", "string"},
        {"any", "Any value: not checked.", "any"},
        {"curve", "A curve: points, each `(x y)`; `(@range LO HI)` holds every y.", "curve"},
        {"list",
         "`(list T)`: values of type T. `(list T N)` holds exactly N, `(list T N M)` N to M.",
         "(list ${1:float})"},
        {"key_of",
         "`(key_of TABLE)`: the key of a record in a keyed table, looked up outward from the "
         "record.",
         "(key_of ${1:table})"},
        {"asset",
         "`(asset KIND)`: the path of a file, relative to the project file, of a kind the "
         "project's `(assets ...)` declares; `check` fails on a missing file.",
         "(asset ${1:kind})"},
    };
    return kTypes;
}

}  // namespace baker_lsp
