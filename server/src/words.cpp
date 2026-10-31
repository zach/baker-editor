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
};

constexpr std::string_view kData[] = {"@range", "@step",   "@kind",  "@desc",
                                      "@note",  "@one_of", "@group", "@was"};
constexpr std::string_view kShape[] = {"@at", "@keyed", "@rule", "@desc", "@note"};
constexpr std::string_view kField[] = {"@range", "@step",   "@kind",    "@desc",
                                       "@note",  "@one_of", "@optional"};
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
    };
    return kTypes;
}

}  // namespace baker_lsp
