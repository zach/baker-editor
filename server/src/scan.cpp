#include "scan.hpp"

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

std::string unquote(std::string_view t) {
    if (t.size() >= 2 && (t.front() == '\'' || t.front() == '"') && t.back() == t.front())
        return std::string(t.substr(1, t.size() - 2));
    return std::string(t);
}

}  // namespace

namespace {
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

Cursor scan(std::string_view text, size_t offset) {
    Cursor c;
    if (offset > text.size()) offset = text.size();
    std::vector<Frame> stack;
    std::string previous;  // the last token in the current bracket (or at the top)
    bool found = false;
    const auto at_cursor = [&](size_t begin, size_t end, std::string_view token) {
        c.stack = stack;
        c.token = unquote(token);
        c.begin = begin;
        c.end = end;
        c.index = stack.empty() ? 0 : stack.back().count;
        c.previous = previous;
        found = true;
    };
    size_t i = 0;
    while (i < text.size() && !found) {
        const char ch = text[i];
        if (is_space(ch)) {
            ++i;
            continue;
        }
        // Past the cursor, or at a bracket or comment it stands before: between tokens. A
        // token that starts at the cursor is the one it touches.
        if (i > offset || (i == offset && (is_open(ch) || is_close(ch) || ch == ';'))) {
            at_cursor(offset, offset, {});
            break;
        }
        if (ch == '/' && i + 1 < text.size() && text[i + 1] == '*') {  // a block comment
            const size_t close = text.find("*/", i + 2);
            const size_t end = close == std::string_view::npos ? text.size() : close + 2;
            if (offset < end) {
                c.quiet = true;
                return c;
            }
            i = end;
            continue;
        }
        if (ch == ';') {  // a comment, to the end of the line
            const size_t nl = text.find('\n', i);
            const size_t end = nl == std::string_view::npos ? text.size() : nl;
            if (offset <= end) {
                c.quiet = true;
                return c;
            }
            i = end;
            continue;
        }
        if (is_open(ch)) {
            if (!stack.empty()) ++stack.back().count;
            stack.push_back({ch, {}, 0});
            previous.clear();
            ++i;
            continue;
        }
        if (is_close(ch)) {
            if (!stack.empty()) stack.pop_back();
            previous.clear();
            ++i;
            continue;
        }
        // A token: a string or a run up to a space, bracket or comment.
        size_t j = i;
        if (ch == '"' || ch == '\'') {
            j = i + 1;
            while (j < text.size() && text[j] != ch) ++j;  // \n and \q hold no quote
            j = j < text.size() ? j + 1 : text.size();
            if (ch == '"' && offset > i && offset < j) {
                at_cursor(i, j, text.substr(i, j - i));
                c.quiet = c.in_string = true;
                return c;
            }
        } else {
            while (j < text.size() && !is_space(text[j]) && !is_open(text[j]) &&
                   !is_close(text[j]) && text[j] != ';')
                ++j;
        }
        const std::string_view token = text.substr(i, j - i);
        if (offset >= i && offset <= j) {
            at_cursor(i, j, token);
            break;
        }
        if (!stack.empty()) {
            Frame& f = stack.back();
            if (f.count == 0) f.tag = unquote(token);
            ++f.count;
        }
        previous = std::string(token);
        i = j;
    }
    if (!found) at_cursor(offset, offset, {});
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
