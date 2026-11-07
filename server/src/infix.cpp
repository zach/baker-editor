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

// The innermost brace around `offset`, closed and with no comment in it; or -1.
int brace_at(const Structure& s, size_t offset) {
    int b = -1;
    for (size_t k = 0; k < s.brackets.size() && s.brackets[k].begin <= offset; ++k)
        if (offset < s.brackets[k].end && s.brackets[k].open == '{') b = static_cast<int>(k);
    if (b < 0 || !s.brackets[static_cast<size_t>(b)].closed) return -1;
    const Bracket& br = s.brackets[static_cast<size_t>(b)];
    for (const Comment& c : s.comments)
        if (c.begin > br.begin && c.begin < br.end) return -1;
    return b;
}

}  // namespace

std::vector<BraceEdit> infix_spellings(const Structure& s, std::string_view text, size_t offset) {
    std::vector<BraceEdit> out;
    const int b = brace_at(s, offset);
    if (b < 0) return out;
    const Bracket& br = s.brackets[static_cast<size_t>(b)];
    const std::vector<Element> e = elements_of(s, b);
    const auto at = [&](const Element& x) {
        return std::string(text.substr(x.begin, x.end - x.begin));
    };
    // Baker's word on the new text: it reads with no error, infix or not as asked.
    const auto reads_as = [](const std::string& brace, bool infix) {
        const baker::tools::TextStructure read = baker::tools::read_structure(brace);
        return read.errors.empty() && !read.brackets.empty() &&
               (read.brackets[0].infix >= 0) == infix;
    };
    if (br.infix >= 0) {  // {a op b op c} -> {op a b c}
        std::string fixed = "{" + at(e[1]);
        for (size_t i = 0; i < e.size(); i += 2) fixed += " " + at(e[i]);
        fixed += "}";
        if (reads_as(fixed, false))
            out.push_back(
                {"Write prefix", "refactor.rewrite", {br.begin, br.end, std::move(fixed)}});
        return out;
    }
    // {op a b c} -> {a op b op c}: a head that is an operator, two operands or more.
    if (e.size() < 3 || !e[0].op ||
        std::any_of(e.begin() + 1, e.end(), [](const Element& x) { return x.op; }))
        return out;
    if (!error_at_brace(text.substr(br.begin, br.end - br.begin)).empty()) return out;
    const std::string op = at(e[0]);
    std::string fixed = "{" + at(e[1]);
    for (size_t i = 2; i < e.size(); ++i) fixed += " " + op + " " + at(e[i]);
    fixed += "}";
    if (reads_as(fixed, true))
        out.push_back({"Write infix", "refactor.rewrite", {br.begin, br.end, std::move(fixed)}});
    return out;
}

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
