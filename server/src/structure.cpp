#include "structure.hpp"

#include <algorithm>

namespace baker_lsp {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
bool is_open(char c) {
    return c == '(' || c == '{' || c == '[';
}
bool is_close(char c) {
    return c == ')' || c == '}' || c == ']';
}

}  // namespace

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
    std::vector<int> open;    // the brackets not yet closed
    std::vector<int> counts;  // tokens and arrays in each of them so far
    const auto add_to_parent = [&] {
        if (!counts.empty()) ++counts.back();
    };
    size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];
        if (is_space(ch)) {
            ++i;
        } else if (ch == ';') {
            const size_t nl = text.find('\n', i);
            i = nl == std::string_view::npos ? text.size() : nl;
        } else if (ch == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            const size_t close = text.find("*/", i + 2);
            const size_t end = close == std::string_view::npos ? text.size() : close + 2;
            s.comments.push_back({i, end});
            i = end;
        } else if (is_open(ch)) {
            add_to_parent();
            if (!open.empty()) s.brackets[static_cast<size_t>(open.back())].has_child = true;
            s.brackets.push_back({ch, open.empty() ? -1 : open.back(), i, text.size(), -1});
            open.push_back(static_cast<int>(s.brackets.size() - 1));
            counts.push_back(0);
            ++i;
        } else if (is_close(ch)) {
            if (!open.empty()) {
                s.brackets[static_cast<size_t>(open.back())].end = i + 1;
                open.pop_back();
                counts.pop_back();
            }
            ++i;
        } else {
            size_t j = i;
            char quote = 0;
            if (ch == '"' || ch == '\'') {
                quote = ch;
                j = text.find(ch, i + 1);  // \n and \q hold no quote
                j = j == std::string_view::npos ? text.size() : j + 1;
            } else {
                while (j < text.size() && !is_space(text[j]) && !is_open(text[j]) &&
                       !is_close(text[j]) && text[j] != ';' && text[j] != '"')
                    ++j;
            }
            Token t;
            t.begin = i;
            t.end = j;
            t.quote = quote;
            t.text = text.substr(i, j - i);
            if (quote && t.text.size() >= 2 && t.text.back() == quote)
                t.text = t.text.substr(1, t.text.size() - 2);
            t.bracket = open.empty() ? -1 : open.back();
            t.index = counts.empty() ? 0 : counts.back();
            if (t.bracket >= 0 && t.index == 0)
                s.brackets[static_cast<size_t>(t.bracket)].tag = static_cast<int>(s.tokens.size());
            s.tokens.push_back(std::move(t));
            add_to_parent();
            i = j;
        }
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
