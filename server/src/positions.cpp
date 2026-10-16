#include "positions.hpp"

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
        while (j < line.size() && line[j] != '"') j += line[j] == '\\' ? 2 : 1;
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

}  // namespace baker_lsp
