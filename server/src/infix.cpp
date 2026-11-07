#include "infix.hpp"

#include <baker/tools/structure.hpp>

#include <algorithm>

namespace baker_lsp {
namespace {

// A brace's element: a token of its own, or a bracket in it.
struct Element {
    size_t begin = 0, end = 0;
    bool op = false;  // a bare operator symbol: + - * / % < > = ! & | ^ ~ and only those
};

bool is_operator(const Token& t) {
    return t.kind == TokenKind::Symbol && t.quote == 0 && !t.text.empty() &&
           t.text.find_first_not_of("+-*/%<>=!&|^~") == std::string_view::npos;
}

std::vector<Element> elements_of(const Structure& s, int b) {
    std::vector<Element> out;
    const Bracket& br = s.brackets[static_cast<size_t>(b)];
    for (const Token& t : s.tokens)
        if (t.bracket == b) out.push_back({t.begin, t.end, is_operator(t)});
    for (size_t c = static_cast<size_t>(b) + 1;
         c < s.brackets.size() && s.brackets[c].begin < br.end; ++c)
        if (s.brackets[c].parent == b)
            out.push_back({s.brackets[c].begin, s.brackets[c].end, false});
    std::sort(out.begin(), out.end(),
              [](const Element& x, const Element& y) { return x.begin < y.begin; });
    return out;
}

// The first error Baker's reader gives `brace`, a brace's text alone, at the brace itself;
// empty if none.
std::string error_at_brace(std::string_view brace) {
    const baker::tools::TextStructure read = baker::tools::read_structure(brace);
    for (const baker::Error& e : read.errors)
        if (e.line() == 1 && e.column() == 1) return std::string(e.message());
    return {};
}

// Whether `brace` reads with no error at all.
bool reads(std::string_view brace) {
    return baker::tools::read_structure(brace).errors.empty();
}

bool comparison(std::string_view op) {
    return op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=";
}

}  // namespace

std::vector<BraceEdit> infix_fixes(const Structure& s, std::string_view text, size_t offset) {
    std::vector<BraceEdit> out;
    int b = -1;  // the innermost brace around the cursor
    for (size_t k = 0; k < s.brackets.size() && s.brackets[k].begin <= offset; ++k)
        if (offset < s.brackets[k].end && s.brackets[k].open == '{') b = static_cast<int>(k);
    if (b < 0) return out;
    const Bracket& br = s.brackets[static_cast<size_t>(b)];
    if (!br.closed || br.infix >= 0) return out;
    const std::string_view brace = text.substr(br.begin, br.end - br.begin);
    const std::string error = error_at_brace(brace);
    if (error.empty()) return out;
    const std::vector<Element> e = elements_of(s, b);
    const auto at = [&](const Element& x) { return text.substr(x.begin, x.end - x.begin); };

    if (error.find("needs '") != std::string::npos && e.size() >= 3 && e[1].op) {
        // After each operand an operand follows, the operator.
        const std::string_view op = at(e[1]);
        std::string fixed(brace);
        for (size_t i = e.size() - 1; i > 0; --i)
            if (!e[i].op && !e[i - 1].op)
                fixed.insert(e[i - 1].end - br.begin, " " + std::string(op));
        if (reads(fixed))
            out.push_back({"Insert the missing '" + std::string(op) + "'",
                           "quickfix",
                           {br.begin, br.end, std::move(fixed)}});
    }
    if (error.find("mixed operators") != std::string::npos && e.size() >= 5 && (e.size() & 1)) {
        // A chain of comparisons, each adjacent pair its own brace under `and`.
        bool chain = std::none_of(s.comments.begin(), s.comments.end(), [&](const Comment& c) {
            return c.begin > br.begin && c.begin < br.end;
        });
        for (size_t i = 0; i < e.size() && chain; ++i)
            chain = (i & 1) ? e[i].op && comparison(at(e[i])) : !e[i].op;
        if (chain) {
            std::string fixed = "{and";
            for (size_t i = 1; i < e.size(); i += 2)
                fixed += " {" + std::string(at(e[i - 1])) + " " + std::string(at(e[i])) + " " +
                         std::string(at(e[i + 1])) + "}";
            fixed += "}";
            if (reads(fixed))
                out.push_back({"Nest under and", "quickfix", {br.begin, br.end, std::move(fixed)}});
        }
    }
    return out;
}

}  // namespace baker_lsp
