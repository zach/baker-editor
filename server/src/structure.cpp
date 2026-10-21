#include "structure.hpp"

#include <baker/parse.hpp>

#include <algorithm>

namespace baker_lsp {

std::string_view Structure::tag(int b) const {
    if (b < 0 || brackets[static_cast<size_t>(b)].tag < 0) return {};
    return tokens[static_cast<size_t>(brackets[static_cast<size_t>(b)].tag)].text;
}

std::vector<std::string> Structure::data_path(int b) const {
    std::vector<std::string> tags;
    for (; b >= 0; b = brackets[static_cast<size_t>(b)].parent) {
        const Bracket& br = brackets[static_cast<size_t>(b)];
        if (br.open != '(' || br.tag < 0 || tokens[static_cast<size_t>(br.tag)].quote == '"')
            return {};
        tags.emplace_back(tokens[static_cast<size_t>(br.tag)].text);  // a copy
    }
    std::reverse(tags.begin(), tags.end());
    return tags;
}

Structure read_structure(std::string_view text) {
    Structure s;
    const baker::TextStructure read = baker::read_structure(text);
    s.brackets.reserve(read.brackets.size());
    s.tokens.reserve(read.tokens.size() - read.brackets.size());  // less the opens
    for (const baker::TextComment& c : read.comments)
        s.comments.push_back({c.begin, c.end, c.block});
    std::vector<int> open;    // the brackets around the next token, innermost last
    std::vector<int> counts;  // tokens and arrays in each of them so far
    size_t next = 0;          // the next of Baker's brackets, which open in token order
    for (const baker::TextToken& t : read.tokens) {
        // A close Baker read has ended its bracket; an unclosed one ends with the text.
        while (!open.empty() && s.brackets[static_cast<size_t>(open.back())].end <= t.begin) {
            open.pop_back();
            counts.pop_back();
        }
        if (t.kind == baker::TextTokenKind::Close) continue;
        if (!counts.empty()) ++counts.back();
        if (t.kind == baker::TextTokenKind::Open) {
            const baker::TextBracket& b = read.brackets[next++];
            if (!open.empty()) s.brackets[static_cast<size_t>(open.back())].has_child = true;
            s.brackets.push_back(
                {b.open, open.empty() ? -1 : open.back(), b.begin, b.end, -1, false, b.closed});
            open.push_back(static_cast<int>(s.brackets.size() - 1));
            counts.push_back(0);
            continue;
        }
        Token token;
        token.begin = t.begin;
        token.end = t.end;
        token.quote = t.kind == baker::TextTokenKind::String         ? '"'
                      : t.kind == baker::TextTokenKind::QuotedSymbol ? '\''
                                                                     : 0;
        token.text = text.substr(t.begin, t.end - t.begin);
        if (token.quote && token.text.size() >= 2 && token.text.back() == token.quote)
            token.text = token.text.substr(1, token.text.size() - 2);
        token.bracket = open.empty() ? -1 : open.back();
        token.index = counts.empty() ? 0 : counts.back() - 1;
        if (token.bracket >= 0 && token.index == 0)
            s.brackets[static_cast<size_t>(token.bracket)].tag = static_cast<int>(s.tokens.size());
        s.tokens.push_back(token);
    }
    return s;
}

LineIndex::LineIndex(std::string_view text) : text_(text) {
    starts_.push_back(0);
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n') starts_.push_back(i + 1);
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
