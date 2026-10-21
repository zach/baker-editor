#include "scan.hpp"

#include <algorithm>

namespace baker_lsp {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
std::vector<std::string> tags_of(const std::vector<Frame>& stack, size_t count) {
    std::vector<std::string> tags;
    for (size_t i = 0; i < count; ++i) {
        const Frame& f = stack[i];
        if (f.open != '(' || f.tag.empty()) return {};
        tags.push_back(f.tag);
    }
    return tags;
}
}  // namespace

std::vector<std::string> Cursor::data_path() const {
    return tags_of(stack, stack.size());
}

std::vector<std::string> Cursor::parent_path() const {
    return stack.empty() ? std::vector<std::string>() : tags_of(stack, stack.size() - 1);
}

Cursor scan(const Structure& s, std::string_view text, size_t offset) {
    Cursor c;
    if (offset > text.size()) offset = text.size();
    // In a comment: after its first character, to its end (a ; comment's line end too, and
    // the text's end for one not closed).
    for (const Comment& m : s.comments) {
        if (m.begin >= offset) break;
        if (offset < m.end || (offset == m.end && (!m.block || m.end == text.size()))) {
            c.quiet = true;
            return c;
        }
    }
    c.begin = c.end = offset;
    const std::vector<Token>& tokens = s.tokens;
    const std::vector<Bracket>& brackets = s.brackets;
    // The token touching the cursor, if any, and the last one before it.
    const auto at = std::lower_bound(tokens.begin(), tokens.end(), offset,
                                     [](const Token& t, size_t o) { return t.end < o; });
    const Token* touching = at != tokens.end() && at->begin <= offset ? &*at : nullptr;
    const Token* before = at != tokens.begin() ? &*(at - 1) : nullptr;
    // The last bracket opened before the cursor; the innermost around it is it or one of
    // its ancestors: one opened before the cursor, and closed after it or not at all.
    const auto opened = std::lower_bound(brackets.begin(), brackets.end(), offset,
                                         [](const Bracket& b, size_t o) { return b.begin < o; });
    const int last = static_cast<int>(opened - brackets.begin()) - 1;
    int inner = last;
    while (inner >= 0) {
        const Bracket& b = brackets[static_cast<size_t>(inner)];
        if (offset < b.end || (!b.closed && offset == b.end)) break;
        inner = b.parent;
    }
    // Each frame's elements before the cursor: an outer frame's, up to the bracket the next
    // frame is; the innermost's, up to the last element holding a token or bracket before it.
    std::vector<int> chain;
    for (int b = inner; b >= 0; b = brackets[static_cast<size_t>(b)].parent) chain.push_back(b);
    const auto place_in = [&](int bracket, int index) {  // the element of `inner` holding it
        while (bracket >= 0 && bracket != inner) {
            const Bracket& b = brackets[static_cast<size_t>(bracket)];
            if (b.parent == inner) return b.index + 1;
            bracket = b.parent;
        }
        return bracket == inner ? index : 0;
    };
    int count = 0;
    if (before) count = std::max(count, place_in(before->bracket, before->index + 1));
    if (last >= 0 && last != inner) count = std::max(count, place_in(last, 0));
    for (size_t k = chain.size(); k-- > 0;) {
        const Bracket& b = brackets[static_cast<size_t>(chain[k])];
        Frame f{b.open, {}, k == 0 ? count : brackets[static_cast<size_t>(chain[k - 1])].index + 1};
        if (b.tag >= 0 && tokens[static_cast<size_t>(b.tag)].end < offset)
            f.tag = std::string(tokens[static_cast<size_t>(b.tag)].text);
        c.stack.push_back(std::move(f));
    }
    c.index = c.stack.empty() ? 0 : c.stack.back().count;
    if (touching) {
        c.token = std::string(touching->text);
        c.begin = touching->begin;
        c.end = touching->end;
        const bool closed = c.end - c.begin >= 2 && text[c.end - 1] == '"';
        if (touching->quote == '"' && offset > c.begin && (offset < c.end || !closed))
            c.quiet = c.in_string = true;
    }
    // The token before the cursor's, in the same bracket with no bracket between them.
    if (before && before->bracket == inner &&
        (last < 0 || brackets[static_cast<size_t>(last)].begin < before->begin))
        c.previous = std::string(text.substr(before->begin, before->end - before->begin));
    return c;
}

std::string path_text(const std::vector<std::string>& tags) {
    std::string out;
    for (const std::string& t : tags) {
        if (!out.empty()) out += '/';
        bool plain = !t.empty();
        bool digits = true;
        for (const char ch : t) {
            plain = plain && ch != '.' && ch != '/' && ch != '\'' && !is_space(ch);
            digits = digits && ch >= '0' && ch <= '9';
        }
        if (plain && !digits)
            out += t;
        else
            out += "'" + t + "'";
    }
    return out;
}

}  // namespace baker_lsp
