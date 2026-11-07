#include "structure.hpp"

#include <baker/parse.hpp>
#include <baker/tools/structure.hpp>

#include <algorithm>

namespace baker_lsp {
namespace {

TokenKind kind_of(baker::tools::TextTokenKind k) {
    switch (k) {
        case baker::tools::TextTokenKind::QuotedSymbol: return TokenKind::QuotedSymbol;
        case baker::tools::TextTokenKind::String: return TokenKind::String;
        case baker::tools::TextTokenKind::Int: return TokenKind::Int;
        case baker::tools::TextTokenKind::Float: return TokenKind::Float;
        case baker::tools::TextTokenKind::Variable: return TokenKind::Variable;
        case baker::tools::TextTokenKind::Directive: return TokenKind::Directive;
        case baker::tools::TextTokenKind::Macro: return TokenKind::Macro;
        case baker::tools::TextTokenKind::PathKey: return TokenKind::PathKey;
        default: return TokenKind::Symbol;
    }
}

// The segments of a row's key `t`, written /a.b.'c d'/e, into `out`, as Baker reads them;
// false, with none added, if the key does not read.
bool split_key(const Token& t, std::vector<Segment>& out) {
    constexpr size_t kMaxSegments = 16;  // as many as a Path holds
    const std::string_view key = t.text.substr(1);
    const size_t first = out.size();
    const auto bad = [&] {
        out.resize(first);
        return false;
    };
    for (size_t i = 0, from = 0; i < key.size(); from = i) {
        if (out.size() - first == kMaxSegments) return bad();
        std::string_view name;
        if (key[i] == '\'') {
            i = key.find('\'', i + 1);
            if (i == std::string_view::npos) return bad();
            name = key.substr(from + 1, i++ - from - 1);
        } else {
            i = std::min(key.find_first_of("./", i), key.size());
            name = key.substr(from, i - from);
            if (name.find_first_not_of("0123456789") == std::string_view::npos) return bad();
        }
        if (name.empty()) return bad();
        out.push_back({name, t.begin + 1 + from, t.begin + 1 + i});
        if (i == key.size()) break;
        if ((key[i] != '.' && key[i] != '/') || ++i == key.size()) return bad();
    }
    return out.size() > first;
}

}  // namespace

std::string_view Structure::head(int b) const {
    if (b < 0) return {};
    const Bracket& br = brackets[static_cast<size_t>(b)];
    if (br.infix >= 0) return tokens[static_cast<size_t>(br.infix)].text;
    return tag(b);
}

bool Structure::is_head(size_t t) const {
    const Token& tok = tokens[t];
    if (tok.bracket < 0) return false;
    const Bracket& br = brackets[static_cast<size_t>(tok.bracket)];
    if (br.infix < 0) return tok.index == 0;
    return (tok.index & 1) && tok.quote == 0 &&
           tok.text == tokens[static_cast<size_t>(br.infix)].text;
}

std::string_view Structure::name(int b) const {
    if (is_row(b)) return key(b).back().name;
    return tag(b);
}

bool Structure::is_row(int b) const {
    return b >= 0 && brackets[static_cast<size_t>(b)].segments > 0;
}

std::span<const Segment> Structure::key(int b) const {
    if (!is_row(b)) return {};
    const Bracket& br = brackets[static_cast<size_t>(b)];
    return std::span<const Segment>(segments).subspan(static_cast<size_t>(br.segment),
                                                      static_cast<size_t>(br.segments));
}

std::string_view Structure::tag(int b) const {
    if (b < 0 || brackets[static_cast<size_t>(b)].tag < 0) return {};
    return tokens[static_cast<size_t>(brackets[static_cast<size_t>(b)].tag)].text;
}

bool Structure::is_metadata(int b) const {
    if (b < 0) return false;
    const Bracket& br = brackets[static_cast<size_t>(b)];
    if (br.open != '(' || br.tag < 0) return false;
    const Token& tag = tokens[static_cast<size_t>(br.tag)];
    return tag.quote == 0 && tag.text.size() > 1 && tag.text[0] == '@';
}

std::string_view Structure::declared_by(int b) const {
    if (b < 0 || brackets[static_cast<size_t>(b)].declares < 0) return {};
    return tokens[static_cast<size_t>(brackets[static_cast<size_t>(b)].declares)].text;
}

std::vector<std::string> Structure::data_path(int b) const {
    std::vector<std::string> tags;
    for (; b >= 0; b = brackets[static_cast<size_t>(b)].parent) {
        const Bracket& br = brackets[static_cast<size_t>(b)];
        if (br.open != '(' || br.tag < 0 || tokens[static_cast<size_t>(br.tag)].quote == '"' ||
            br.declares >= 0)  // a declaration's form is not in the tree
            return {};
        if (tokens[static_cast<size_t>(br.tag)].kind == TokenKind::PathKey) {
            if (!is_row(b)) return {};  // a key that does not read names no path
            const std::span<const Segment> k = key(b);
            for (size_t i = k.size(); i-- > 0;) tags.emplace_back(k[i].name);
            continue;
        }
        tags.emplace_back(tokens[static_cast<size_t>(br.tag)].text);  // a copy
    }
    std::reverse(tags.begin(), tags.end());
    return tags;
}

Structure read_structure(std::string_view text) {
    Structure s;
    const baker::tools::TextStructure read = baker::tools::read_structure(text);
    s.brackets.reserve(read.brackets.size());
    s.tokens.reserve(read.tokens.size() - read.brackets.size());  // less the opens
    for (const baker::tools::TextComment& c : read.comments)
        s.comments.push_back({c.begin, c.end, c.block});
    std::vector<int> open;    // the brackets around the next token, innermost last
    std::vector<int> counts;  // tokens and arrays in each of them so far
    size_t next = 0;          // the next of Baker's brackets, which open in token order
    std::vector<int> ours(read.tokens.size(), -1);  // Baker's token index -> this one's
    for (size_t i = 0; i < read.tokens.size(); ++i) {
        const baker::tools::TextToken& t = read.tokens[i];
        // A close Baker read has ended its bracket; an unclosed one ends with the text.
        while (!open.empty() && s.brackets[static_cast<size_t>(open.back())].end <= t.begin) {
            open.pop_back();
            counts.pop_back();
        }
        if (t.kind == baker::tools::TextTokenKind::Close) continue;
        if (!counts.empty()) ++counts.back();
        if (t.kind == baker::tools::TextTokenKind::Open) {
            const baker::tools::TextBracket& b = read.brackets[next++];
            s.brackets.push_back({b.open, open.empty() ? -1 : open.back(), b.begin, b.end, -1,
                                  false, b.closed, counts.empty() ? 0 : counts.back() - 1,
                                  b.declares >= 0 ? ours[static_cast<size_t>(b.declares)] : -1});
            open.push_back(static_cast<int>(s.brackets.size() - 1));
            counts.push_back(0);
            continue;
        }
        Token token;
        token.begin = t.begin;
        token.end = t.end;
        token.quote = t.kind == baker::tools::TextTokenKind::String         ? '"'
                      : t.kind == baker::tools::TextTokenKind::QuotedSymbol ? '\''
                                                                            : 0;
        token.kind = kind_of(t.kind);
        token.text = text.substr(t.begin, t.end - t.begin);
        if (token.quote && token.text.size() >= 2 && token.text.back() == token.quote)
            token.text = token.text.substr(1, token.text.size() - 2);
        token.bracket = open.empty() ? -1 : open.back();
        token.index = counts.empty() ? 0 : counts.back() - 1;
        if (token.bracket >= 0 && token.index == 0) {
            Bracket& br = s.brackets[static_cast<size_t>(token.bracket)];
            br.tag = static_cast<int>(s.tokens.size());
            if (token.kind == TokenKind::PathKey) {
                br.segment = static_cast<int>(s.segments.size());
                if (split_key(token, s.segments))
                    br.segments = static_cast<int>(s.segments.size()) - br.segment;
            }
        }
        ours[i] = static_cast<int>(s.tokens.size());
        s.tokens.push_back(token);
    }
    // An infix brace's operator, which Baker names by its own token's index.
    for (size_t b = 0; b < s.brackets.size(); ++b)
        if (const int32_t op = read.brackets[b].infix; op >= 0)
            s.brackets[b].infix = ours[static_cast<size_t>(op)];
    // Each bracket's tag is known now: a child other than metadata makes its parent hold one.
    for (size_t b = 0; b < s.brackets.size(); ++b)
        if (const int p = s.brackets[b].parent; p >= 0 && !s.is_metadata(static_cast<int>(b)))
            s.brackets[static_cast<size_t>(p)].has_child = true;
    return s;
}

bool bare_segment(std::string_view name) {
    if (name.empty() || name.find_first_not_of("0123456789") == std::string_view::npos)
        return false;
    for (const char ch : name)
        if (ch == '.' || ch == '/' || ch == '\'' || ch == ' ' || ch == '(' || ch == ')' ||
            ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == ';' || ch == '"')
            return false;
    return true;
}

LineIndex::LineIndex(std::string_view text) : text_(text) {
    starts_.push_back(0);
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n') starts_.push_back(i + 1);
}

size_t LineIndex::start(int line) const {
    if (line < 0) return 0;
    return static_cast<size_t>(line) < starts_.size() ? starts_[static_cast<size_t>(line)]
                                                      : text_.size();
}

std::pair<int, int> LineIndex::position(size_t offset) const {
    offset = std::min(offset, text_.size());
    const auto it = std::upper_bound(starts_.begin(), starts_.end(), offset) - 1;
    const size_t start = *it;
    const std::string_view line = text_.substr(start, offset - start);
    int units = 0;
    for (const char c : line) {
        if ((static_cast<unsigned char>(c) & 0xC0) == 0x80) continue;
        units += static_cast<unsigned char>(c) >= 0xF0 ? 2 : 1;
    }
    return {static_cast<int>(it - starts_.begin()), units};
}

}  // namespace baker_lsp
