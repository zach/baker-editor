#include "outline.hpp"

#include "features.hpp"

#include <algorithm>
#include <optional>

namespace baker_lsp {
namespace {

// The token under `offset`, or -1.
int token_at(const Structure& s, size_t offset) {
    for (size_t i = 0; i < s.tokens.size(); ++i)
        if (s.tokens[i].begin <= offset && offset <= s.tokens[i].end) return static_cast<int>(i);
    return -1;
}

bool defined(const std::vector<Definition>& list, std::string_view name) {
    return std::any_of(list.begin(), list.end(),
                       [&](const Definition& d) { return d.name == name; });
}

// The token before token `t` in the same bracket (or at the top), or null.
const Token* previous(const Structure& s, size_t t) {
    if (t == 0) return nullptr;
    const Token& prev = s.tokens[t - 1];
    return prev.bracket == s.tokens[t].bracket ? &prev : nullptr;
}

// Whether bracket `b`, an array tagged `field`, is a field of shape `shape`: governed by it
// through its parent, or its declaration in (shapes (shape ...)).
bool is_field(const Structure& s, int b, std::string_view field, const std::string& shape,
              const Loaded& loaded) {
    if (s.brackets[static_cast<size_t>(b)].open != '(' || s.tag(b) != field) return false;
    const std::vector<std::string> path = s.data_path(b);
    if (path.size() == 3 && path[0] == "shapes" && path[1] == shape) return true;
    const int parent = s.brackets[static_cast<size_t>(b)].parent;
    if (parent < 0) return false;
    const baker::shape::Shape* gov = governing(loaded.shapes, s.data_path(parent));
    return gov && gov->name.str() == shape;
}

}  // namespace

std::vector<OutlineSymbol> outline(const Structure& s, std::string_view text, size_t limit) {
    const bool leaves = s.brackets.size() <= limit;
    // Each bracket's symbol, if it shows one, and the nearest ancestor that shows one.
    std::vector<std::optional<OutlineSymbol>> made(s.brackets.size());
    std::vector<int> up(s.brackets.size(), -1);
    for (size_t b = 0; b < s.brackets.size(); ++b) {
        const Bracket& br = s.brackets[b];
        for (int p = br.parent; p >= 0; p = s.brackets[static_cast<size_t>(p)].parent)
            if (made[static_cast<size_t>(p)]) {
                up[b] = p;
                break;
            }
        if (br.tag < 0) continue;
        const Token& tag = s.tokens[static_cast<size_t>(br.tag)];
        OutlineSymbol sym;
        sym.begin = br.begin;
        sym.end = br.end;
        if (br.open == '(' && tag.quote != '"') {
            sym.name = tag.text;
            sym.name_begin = tag.begin;
            sym.name_end = tag.end;
            // A leaf, (health 1000), is a field, with its values as detail.
            const bool leaf = !br.has_child;
            if (leaf && !leaves) continue;
            std::string detail;
            for (size_t t = static_cast<size_t>(br.tag) + 1;
                 leaf && t < s.tokens.size() && s.tokens[t].begin < br.end && detail.size() < 40;
                 ++t)
                detail += (detail.empty() ? "" : " ") +
                          std::string(
                              text.substr(s.tokens[t].begin, s.tokens[t].end - s.tokens[t].begin));
            sym.kind = leaf ? SymbolKind::Field
                            : (br.parent < 0 ? SymbolKind::Namespace : SymbolKind::Object);
            if (leaf) sym.detail = detail;
        } else if (br.open == '[' && br.parent < 0) {
            sym.name = tag.text;
            sym.name_begin = tag.begin;
            sym.name_end = tag.end;
            sym.kind = SymbolKind::Constant;
            sym.detail = "macro";
        } else if (br.open == '{' && tag.text == "func" &&
                   static_cast<size_t>(br.tag) + 1 < s.tokens.size() &&
                   s.tokens[static_cast<size_t>(br.tag) + 1].bracket == static_cast<int>(b)) {
            const Token& name = s.tokens[static_cast<size_t>(br.tag) + 1];
            sym.name = name.text;
            sym.name_begin = name.begin;
            sym.name_end = name.end;
            sym.kind = SymbolKind::Function;
            sym.detail = "script function";
        } else {
            continue;
        }
        made[b] = std::move(sym);
    }
    // Children before parents: a bracket opens after its ancestors, so walking backwards
    // moves each symbol into its parent once it is complete.
    std::vector<OutlineSymbol> roots;
    for (size_t b = s.brackets.size(); b-- > 0;) {
        if (!made[b]) continue;
        std::reverse(made[b]->children.begin(), made[b]->children.end());
        if (up[b] >= 0)
            made[static_cast<size_t>(up[b])]->children.push_back(std::move(*made[b]));
        else
            roots.push_back(std::move(*made[b]));
    }
    std::reverse(roots.begin(), roots.end());
    return roots;
}

namespace {

size_t count_symbols(const std::vector<OutlineSymbol>& list) {
    size_t n = list.size();
    for (const OutlineSymbol& s : list) n += count_symbols(s.children);
    return n;
}

void drop_leaves(std::vector<OutlineSymbol>& list) {
    std::erase_if(list, [](const OutlineSymbol& s) { return s.kind == SymbolKind::Field; });
    for (OutlineSymbol& s : list) drop_leaves(s.children);
}

// Keeps symbols in order, parents before children, until `left` runs out.
void keep_first(std::vector<OutlineSymbol>& list, size_t& left) {
    if (list.size() > left) list.resize(left);
    left -= list.size();
    for (OutlineSymbol& s : list) keep_first(s.children, left);
}

}  // namespace

size_t cap_outline(std::vector<OutlineSymbol>& symbols, size_t limit) {
    const size_t all = count_symbols(symbols);
    if (all <= limit) return 0;
    drop_leaves(symbols);
    if (count_symbols(symbols) > limit) {
        size_t left = limit;
        keep_first(symbols, left);
    }
    return all - count_symbols(symbols);
}

std::vector<Fold> folds(const Structure& s, std::string_view text) {
    std::vector<Fold> out;
    const auto multiline = [&text](size_t a, size_t b) {
        return text.substr(a, std::min(b, text.size()) - a).find('\n') != std::string_view::npos;
    };
    for (const Bracket& b : s.brackets)
        if (multiline(b.begin, b.end)) out.push_back({b.begin, b.end > 0 ? b.end - 1 : 0, false});
    for (const Comment& c : s.comments)
        if (c.block && multiline(c.begin, c.end))
            out.push_back({c.begin, c.end > 0 ? c.end - 1 : 0, true});
    return out;
}

std::optional<Target> target_at(const Structure& s, size_t offset, const Loaded& loaded,
                                const Reference& reference) {
    const int t = token_at(s, offset);
    if (t < 0) return std::nullopt;
    const Token& tok = s.tokens[static_cast<size_t>(t)];
    if (tok.quote == '"') return std::nullopt;
    Target target;
    target.name = tok.text;
    target.begin = tok.begin;
    target.end = tok.end;
    const int b = tok.bracket;
    const Bracket* br = b >= 0 ? &s.brackets[static_cast<size_t>(b)] : nullptr;
    const Token* prev = previous(s, static_cast<size_t>(t));

    // A script function: called, or defined by {func NAME}.
    if ((br && br->open == '{' && tok.index == 0) ||
        (br && br->open == '{' && tok.index == 1 && s.tag(b) == "func")) {
        if (tok.text == "func") return std::nullopt;
        target.kind = Target::Kind::Function;
        const bool data = defined(loaded.functions, tok.text) || tok.index == 1;
        const bool game = std::any_of(reference.all.begin(), reference.all.end(),
                                      [&](const FunctionInfo& f) { return f.name == tok.text; });
        if (!data && !game) return std::nullopt;
        target.renamable = data;
        return target;
    }
    // A macro: defined by [NAME ...] or #define NAME, or used.
    const bool macro_def = (br && br->open == '[' && br->parent < 0 && tok.index == 0) ||
                           (prev && prev->text == "#define");
    if (macro_def || (tok.quote == 0 && defined(loaded.macros, tok.text))) {
        target.kind = Target::Kind::Macro;
        return target;
    }
    // A pack: declared by (pack NAME), or required.
    if (br &&
        ((s.tag(b) == "pack" && tok.index == 1) || (s.tag(b) == "requires" && tok.index > 0))) {
        target.kind = Target::Kind::Pack;
        return target;
    }
    // A field: an array's tag, under a shape that has it, or its declaration.
    if (br && br->open == '(' && tok.index == 0) {
        const std::vector<std::string> path = s.data_path(b);
        if (path.size() == 3 && path[0] == "shapes") {
            if (const baker::shape::Shape* sh = loaded.shapes.find(baker::Symbol(path[1])))
                if (sh->field(baker::Symbol(tok.text))) {
                    target.kind = Target::Kind::Field;
                    target.shape = path[1];
                    return target;
                }
        }
        if (br->parent >= 0)
            if (const baker::shape::Shape* sh = governing(loaded.shapes, s.data_path(br->parent)))
                if (sh->field(baker::Symbol(tok.text))) {
                    target.kind = Target::Kind::Field;
                    target.shape = std::string(sh->name.str());
                    return target;
                }
    }
    return std::nullopt;
}

std::vector<Occurrence> occurrences(
    const Target& target, const std::vector<std::string>& files, const Loaded& loaded,
    const std::function<bool(const std::string&, std::string&)>& read) {
    std::vector<Occurrence> out;
    for (const std::string& file : files) {
        std::string text;
        if (!read(file, text)) continue;
        const Structure s = read_structure(text);
        for (size_t t = 0; t < s.tokens.size(); ++t) {
            const Token& tok = s.tokens[t];
            if (tok.text != target.name || tok.quote == '"') continue;
            const int b = tok.bracket;
            const Bracket* br = b >= 0 ? &s.brackets[static_cast<size_t>(b)] : nullptr;
            bool hit = false;
            switch (target.kind) {
                case Target::Kind::Macro: hit = tok.quote == 0; break;
                case Target::Kind::Function:
                    hit = br && br->open == '{' &&
                          (tok.index == 0 || (tok.index == 1 && s.tag(b) == "func"));
                    break;
                case Target::Kind::Pack:
                    hit = br && ((s.tag(b) == "pack" && tok.index == 1) ||
                                 (s.tag(b) == "requires" && tok.index > 0));
                    break;
                case Target::Kind::Field:
                    hit = br && tok.index == 0 && is_field(s, b, target.name, target.shape, loaded);
                    break;
            }
            if (hit) out.push_back({file, tok.begin, tok.end, tok.quote == '\''});
        }
    }
    return out;
}

bool valid_name(const std::string& name, const Target& target) {
    if (name.empty()) return false;
    for (const char c : name)
        if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' || c == ';' ||
            c == '"' || c == '\'' || c == '$' || c == '#')
            return false;
    const bool spaced = name.find_first_of(" \t\r\n") != std::string::npos;
    return !spaced || target.kind == Target::Kind::Field;  // a field may be a quoted symbol
}

}  // namespace baker_lsp
