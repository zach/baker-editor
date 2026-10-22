#include "positions.hpp"

#include <algorithm>

namespace baker_lsp {
namespace {

bool is_open(char c) {
    return c == '(' || c == '{' || c == '[';
}
bool is_bracket(char c) {
    return is_open(c) || c == ')' || c == '}' || c == ']';
}
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}
bool is_continuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// The byte offset of code point `code_points` in `line`, or its size if shorter.
size_t byte_offset(std::string_view line, int code_points) {
    size_t i = 0;
    for (int n = 0; n < code_points && i < line.size(); ++n) {
        ++i;
        while (i < line.size() && is_continuation(line[i])) ++i;
    }
    return i;
}

// UTF-16 units in line[0, bytes).
int units_before(std::string_view line, size_t bytes) {
    int units = 0;
    for (size_t i = 0; i < bytes && i < line.size(); ++i) {
        if (is_continuation(line[i])) continue;
        units += static_cast<unsigned char>(line[i]) >= 0xF0 ? 2 : 1;  // a surrogate pair
    }
    return units;
}

// The end of the token that starts at byte `i`: a quoted string, a bracket, or a run of
// anything else up to a space or bracket.
size_t token_end(std::string_view line, size_t i) {
    if (i >= line.size()) return i;
    if (line[i] == '"') {
        size_t j = i + 1;
        while (j < line.size() && line[j] != '"') ++j;  // \n and \q hold no quote
        return j < line.size() ? j + 1 : line.size();
    }
    if (is_bracket(line[i])) return i + 1;
    size_t j = i;
    while (j < line.size() && !is_space(line[j]) && !is_bracket(line[j])) ++j;
    return j;
}

size_t skip_spaces(std::string_view line, size_t i) {
    while (i < line.size() && is_space(line[i])) ++i;
    return i;
}

// An opening bracket at `i`: the end of the token after it (`(price`), else of the bracket.
size_t tag_end(std::string_view line, size_t i) {
    const size_t tag = skip_spaces(line, i + 1);
    return tag < line.size() && !is_bracket(line[tag]) ? token_end(line, tag) : i + 1;
}

// The byte offset of an array tagged `key` in `line`, or npos.
size_t find_tagged(std::string_view line, std::string_view key) {
    for (size_t i = 0; i < line.size(); ++i) {
        if (!is_open(line[i])) continue;
        const size_t tag = skip_spaces(line, i + 1);
        if (line.substr(tag, key.size()) == key && token_end(line, tag) == tag + key.size())
            return i;
    }
    return std::string_view::npos;
}

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::string_view line_text(std::string_view text, int line) {
    if (line < 1) return {};
    size_t start = 0;
    for (int n = 1; n < line; ++n) {
        const size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) return {};
        start = nl + 1;
    }
    size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string_view l = text.substr(start, end - start);
    if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
    return l;
}

size_t offset_of(std::string_view text, Position p) {
    size_t start = 0;
    for (int n = 0; n < p.line; ++n) {
        const size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) return text.size();
        start = nl + 1;
    }
    size_t i = start;
    for (int units = 0; i < text.size() && text[i] != '\n' && units < p.character;) {
        units += static_cast<unsigned char>(text[i]) >= 0xF0 ? 2 : 1;
        ++i;
        while (i < text.size() && is_continuation(text[i])) ++i;
    }
    return i;
}

Position position_of(std::string_view text, size_t offset) {
    if (offset > text.size()) offset = text.size();
    const size_t start = text.rfind('\n', offset == 0 ? 0 : offset - 1);
    const size_t line_start = start == std::string_view::npos || offset == 0 ? 0 : start + 1;
    int line = 0;
    for (size_t i = 0; i < line_start; ++i) line += text[i] == '\n';
    return {line, units_before(text.substr(line_start), offset - line_start)};
}

int utf16_units(std::string_view line, int code_points) {
    return units_before(line, byte_offset(line, code_points));
}

Range problem_range(std::string_view text, int line, int column, std::string_view key) {
    if (line <= 0) return {{0, 0}, {0, 1}};
    const std::string_view l = line_text(text, line);
    size_t begin, end;
    if (column > 0) {
        begin = byte_offset(l, column - 1);
        end = token_end(l, begin);
    } else if (const size_t at = key.empty() ? std::string_view::npos : find_tagged(l, key);
               at != std::string_view::npos) {
        begin = at;
        end = tag_end(l, at);
    } else {
        begin = skip_spaces(l, 0);
        end = begin < l.size() && is_open(l[begin]) ? tag_end(l, begin) : token_end(l, begin);
    }
    Range r{{line - 1, units_before(l, begin)}, {line - 1, units_before(l, end)}};
    if (r.end.character == r.start.character) ++r.end.character;  // at least one character
    return r;
}

std::string last_path_element(std::string_view path) {
    size_t start = 0;  // elements are separated by '/' outside quotes
    bool quoted = false;
    for (size_t i = 0; i < path.size(); ++i) {
        if (quoted && path[i] == '\\')
            ++i;
        else if (path[i] == '\'')
            quoted = !quoted;
        else if (path[i] == '/' && !quoted)
            start = i + 1;
    }
    std::string_view last = path.substr(start);
    if (last.size() >= 2 && last.front() == '\'' && last.back() == '\'')
        last = last.substr(1, last.size() - 2);
    return std::string(last);
}

std::string path_from_uri(std::string_view uri) {
    constexpr std::string_view kScheme = "file://";
    if (uri.substr(0, kScheme.size()) != kScheme) return {};
    uri.remove_prefix(kScheme.size());
    std::string path;
    for (size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == '%' && i + 2 < uri.size() && hex(uri[i + 1]) >= 0 && hex(uri[i + 2]) >= 0) {
            path += static_cast<char>(hex(uri[i + 1]) * 16 + hex(uri[i + 2]));
            i += 2;
        } else {
            path += uri[i];
        }
    }
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') path.erase(0, 1);  // /C:/x
    return path;
}

std::string uri_from_path(std::string_view path) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string uri = "file://";
    if (!path.empty() && path[0] != '/') uri += '/';  // C:/x
    for (const char ch : path) {
        const auto c = static_cast<unsigned char>(ch);
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '_' || c == '.' ||
                           c == '~' || c == ':';
        if (ch == '\\') {
            uri += '/';
        } else if (plain) {
            uri += ch;
        } else {
            uri += '%';
            uri += kHex[c >> 4];
            uri += kHex[c & 15];
        }
    }
    return uri;
}

namespace {

std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> out;
    size_t start = 0;
    for (size_t nl = text.find('\n'); nl != std::string_view::npos; nl = text.find('\n', start)) {
        out.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    out.push_back(text.substr(start));
    return out;
}

}  // namespace

LineMap::LineMap(std::string_view before, std::string_view after) {
    const std::vector<std::string_view> a = lines_of(before), b = lines_of(after);
    to_.assign(a.size(), -1);
    size_t head = 0;
    while (head < a.size() && head < b.size() && a[head] == b[head]) {
        to_[head] = static_cast<int>(head);
        ++head;
    }
    size_t tail = 0;
    while (tail < a.size() - head && tail < b.size() - head &&
           a[a.size() - 1 - tail] == b[b.size() - 1 - tail]) {
        to_[a.size() - 1 - tail] = static_cast<int>(b.size() - 1 - tail);
        ++tail;
    }
    // The middle: the fewest lines removed and put in that turn one into the other (Myers'
    // diff), in time and memory that grow with that number, not with the middle's length.
    const size_t n = a.size() - head - tail, m = b.size() - head - tail;
    if (n == 0 || m == 0) return;
    const auto same = [&](int x, int y) {
        return a[head + static_cast<size_t>(x)] == b[head + static_cast<size_t>(y)];
    };
    const int kn = static_cast<int>(n), km = static_cast<int>(m);
    const int most = static_cast<int>(std::min<size_t>(n + m, kMaxEdits));
    // v[k]: the furthest line of `a` reached on diagonal k (x - y), stored at k + most + 1;
    // trace[d]: v after d edits, for diagonals -d to d.
    std::vector<int> v(static_cast<size_t>(2 * most + 3), 0);
    const auto at = [&](int k) -> int& { return v[static_cast<size_t>(k + most + 1)]; };
    std::vector<std::vector<int>> trace;
    int found = -1;
    for (int d = 0; d <= most && found < 0; ++d) {
        for (int k = -d; k <= d; k += 2) {
            int x = (k == -d || (k != d && at(k - 1) < at(k + 1))) ? at(k + 1) : at(k - 1) + 1;
            int y = x - k;
            while (x < kn && y < km && same(x, y)) {
                ++x;
                ++y;
            }
            at(k) = x;
            if (x >= kn && y >= km) found = d;
        }
        trace.emplace_back(v.begin() + (most + 1 - d), v.begin() + (most + 2 + d));
    }
    if (found < 0) return;                // too many edits: the middle's lines have no place
    const auto was = [&](int d, int k) {  // v after d edits, on diagonal k
        return trace[static_cast<size_t>(d)][static_cast<size_t>(k + d)];
    };
    int x = kn, y = km;
    for (int d = found; d >= 0; --d) {
        const int k = x - y;
        int px = 0, py = 0;
        if (d > 0) {
            const int pk =
                (k == -d || (k != d && was(d - 1, k - 1) < was(d - 1, k + 1))) ? k + 1 : k - 1;
            px = was(d - 1, pk);
            py = px - pk;
        }
        while (x > px && y > py) {  // the diagonal run: lines both keep
            --x;
            --y;
            to_[head + static_cast<size_t>(x)] = static_cast<int>(head + static_cast<size_t>(y));
        }
        x = px;
        y = py;
    }
}

std::optional<int> LineMap::line(int line) const {
    if (line < 1 || static_cast<size_t>(line) > to_.size() || to_[line - 1] < 0)
        return std::nullopt;
    return to_[line - 1] + 1;
}

}  // namespace baker_lsp
