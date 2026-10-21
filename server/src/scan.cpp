#include "scan.hpp"

#include <baker/parse.hpp>

#include <cstdint>

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

namespace {

std::string unquoted(std::string_view t, baker::TextTokenKind kind) {
    const char q = kind == baker::TextTokenKind::String         ? '"'
                   : kind == baker::TextTokenKind::QuotedSymbol ? '\''
                                                                : 0;
    if (q && t.size() >= 2 && t.back() == q) return std::string(t.substr(1, t.size() - 2));
    return std::string(t);
}

// The cursor in Baker's read of a text that runs far enough past it to hold the token it
// touches.
Cursor scan_in(std::string_view text, const baker::TextStructure& read, size_t offset) {
    Cursor c;
    // In a comment: after its first character, to its end (a ; comment's line end too, and
    // the text's end for one not closed).
    for (const baker::TextComment& m : read.comments)
        if (m.begin < offset &&
            (offset < m.end || (offset == m.end && (!m.block || m.end == text.size())))) {
            c.quiet = true;
            return c;
        }
    c.begin = c.end = offset;
    std::vector<uint32_t> ends;  // each frame's bracket's end
    size_t next = 0;             // the next of Baker's brackets, which open in token order
    std::string_view previous;   // the last token in the current bracket (or at the top)
    for (const baker::TextToken& t : read.tokens) {
        while (!ends.empty() && ends.back() <= t.begin) {  // closed before this token
            ends.pop_back();
            c.stack.pop_back();
        }
        // Past the cursor, or at a bracket it stands before: between tokens.
        if (t.begin > offset) break;
        if (t.kind == baker::TextTokenKind::Open) {
            if (t.begin == offset) break;
            const baker::TextBracket& b = read.brackets[next++];
            if (!c.stack.empty()) ++c.stack.back().count;
            c.stack.push_back({b.open, {}, 0});
            ends.push_back(b.closed && b.end <= offset ? b.end : UINT32_MAX);
            previous = {};
            continue;
        }
        if (t.kind == baker::TextTokenKind::Close) {
            if (t.begin == offset) break;
            previous = {};
            continue;
        }
        const std::string_view raw = text.substr(t.begin, t.end - t.begin);
        if (t.end >= offset) {  // it touches the cursor
            c.token = unquoted(raw, t.kind);
            c.begin = t.begin;
            c.end = t.end;
            const bool closed = raw.size() >= 2 && raw.back() == '"';
            if (t.kind == baker::TextTokenKind::String && offset > t.begin &&
                (offset < t.end || !closed))
                c.quiet = c.in_string = true;
            break;
        }
        if (!c.stack.empty()) {
            Frame& f = c.stack.back();
            if (f.count == 0) f.tag = unquoted(raw, t.kind);
            ++f.count;
        }
        previous = raw;
    }
    while (!ends.empty() && ends.back() <= offset) {  // closed before the cursor
        ends.pop_back();
        c.stack.pop_back();
    }
    c.index = c.stack.empty() ? 0 : c.stack.back().count;
    c.previous = std::string(previous);
    return c;
}

}  // namespace

Cursor scan(std::string_view text, size_t offset) {
    if (offset > text.size()) offset = text.size();
    // The text to the end of the cursor's line holds all the cursor needs, unless the token it
    // touches, or a comment it is in, runs on past it (a string across lines).
    const size_t nl = text.find('\n', offset);
    if (nl != std::string_view::npos) {
        const std::string_view head = text.substr(0, nl);
        const Cursor c = scan_in(head, baker::read_structure(head), offset);
        const bool runs_on = c.end == head.size() && c.begin < c.end &&
                             (head[c.begin] == '"' || head[c.begin] == '\'');
        if (!runs_on && !(c.quiet && !c.in_string && offset == head.size())) return c;
    }
    return scan_in(text, baker::read_structure(text), offset);
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
