#include "rewrite.hpp"

#include "format.hpp"

#include <baker/parse.hpp>
#include <baker/std_interop.hpp>

#include <algorithm>

namespace baker_lsp {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// The innermost bracket around `offset`, or -1.
int around(const Structure& s, size_t offset) {
    int found = -1;
    for (size_t b = 0; b < s.brackets.size() && s.brackets[b].begin <= offset; ++b)
        if (offset < s.brackets[b].end) found = static_cast<int>(b);
    return found;
}

int top_of(const Structure& s, int b) {
    while (s.brackets[static_cast<size_t>(b)].parent >= 0)
        b = s.brackets[static_cast<size_t>(b)].parent;
    return b;
}

// Whether a ; comment ends at `at`, or a comment holds the byte before it.
bool after_comment(const Structure& s, size_t at) {
    return std::any_of(s.comments.begin(), s.comments.end(),
                       [at](const Comment& c) { return c.begin < at && at <= c.end; });
}

// `a` as text that tells its values from its keyed children, and its children apart in any
// order: (tag values... | children sorted by tag).
std::string canonical(const baker::Array& a) {
    std::string values;
    std::vector<std::pair<std::string, std::string>> children;  // by tag, then as written
    for (const baker::Node& n : a) {
        if (const baker::Array* child = n.try_array())
            children.emplace_back(std::string(child->tag().str()), canonical(*child));
        else
            values += baker::to_string(n) + " ";
    }
    std::stable_sort(children.begin(), children.end(),
                     [](const auto& x, const auto& y) { return x.first < y.first; });
    std::string out = "(" + values + "|";
    for (const auto& [tag, text] : children) out += text;
    return out + ")";
}

}  // namespace

std::optional<Rewrite> as_row(const Structure& s, std::string_view text, size_t offset) {
    int b = around(s, offset);
    while (b >= 0 && s.is_metadata(b)) b = s.brackets[static_cast<size_t>(b)].parent;
    if (b < 0) return std::nullopt;
    const Bracket& br = s.brackets[static_cast<size_t>(b)];
    if (br.open != '(' || br.parent < 0 || br.has_child || !br.closed || s.is_row(b))
        return std::nullopt;
    const std::vector<std::string> path = s.data_path(b);
    constexpr size_t kMaxSegments = 16;
    if (path.size() < 2 || path.size() > kMaxSegments) return std::nullopt;
    std::string key;
    for (const std::string& seg : path) {
        if (seg.find_first_of(" \t\r\n'") != std::string::npos) return std::nullopt;
        key += (key.empty() ? "/" : ".") + (bare_segment(seg) ? seg : "'" + seg + "'");
    }
    for (const Comment& c : s.comments)
        if (c.begin > br.begin && c.begin < br.end) return std::nullopt;
    const size_t from = s.tokens[static_cast<size_t>(br.tag)].end;
    std::string_view value = text.substr(from, br.end - 1 - from);
    while (!value.empty() && is_space(value.front())) value.remove_prefix(1);
    while (!value.empty() && is_space(value.back())) value.remove_suffix(1);
    if (value.empty()) return std::nullopt;

    // Out of its forms, with the space before it: (a (x 1) (y 2)) less (y 2) is (a (x 1)).
    // Not past a ; comment, which would take the line's close with it.
    // With a sibling after it on its line, the space after it goes instead, so the line keeps
    // its place: (a\n   (x 1) (y 2)) less (x 1) is (a\n   (y 2)).
    const Bracket& top = s.brackets[static_cast<size_t>(top_of(s, b))];
    size_t cut = br.begin, end = br.end;
    size_t next = br.end;
    while (next < top.end && (text[next] == ' ' || text[next] == '\t')) ++next;
    const bool comment_next = std::any_of(s.comments.begin(), s.comments.end(),
                                          [next](const Comment& c) { return c.begin == next; });
    if (next < top.end && !is_space(text[next]) && text[next] != ')' && !comment_next) {
        end = next;
    } else {
        while (cut > top.begin && is_space(text[cut - 1])) --cut;
        if (after_comment(s, cut)) {
            cut = br.begin;
            while (cut > top.begin && (text[cut - 1] == ' ' || text[cut - 1] == '\t')) --cut;
        }
    }
    TextEdit e{top.begin, top.end, std::string(text.substr(top.begin, cut - top.begin))};
    e.text.append(text.substr(end, top.end - end));
    e.text += "\n";
    Rewrite r;
    r.fresh = e.text.size();
    r.fresh_edit = 0;
    e.text += "(" + key + " " + std::string(value) + ")";
    r.edits.push_back(std::move(e));
    return r;
}

std::optional<Rewrite> as_nested(const Structure& s, std::string_view text, size_t offset) {
    const int b = around(s, offset);
    if (b < 0) return std::nullopt;
    const int row = top_of(s, b);
    const Bracket& br = s.brackets[static_cast<size_t>(row)];
    if (!s.is_row(row) || !br.closed) return std::nullopt;
    const std::span<const Segment> key = s.key(row);

    // The form already written for the longest prefix of the key: the first at each level,
    // as Baker folds a row into it.
    int into = -1;
    size_t depth = 0;
    for (; depth < key.size(); ++depth) {
        int found = -1;
        for (size_t c = into < 0 ? 0 : static_cast<size_t>(into) + 1; c < s.brackets.size(); ++c) {
            const Bracket& cb = s.brackets[c];
            if (into >= 0 && cb.begin >= s.brackets[static_cast<size_t>(into)].end) break;
            if (cb.parent != into || cb.open != '(' || cb.declares >= 0 ||
                s.is_row(static_cast<int>(c)) || cb.tag < 0 ||
                s.tokens[static_cast<size_t>(cb.tag)].quote == '"' ||
                s.tag(static_cast<int>(c)) != key[depth].name)
                continue;
            found = static_cast<int>(c);
            break;
        }
        if (found < 0) break;
        into = found;
    }
    if (depth == key.size()) return std::nullopt;  // the file writes the whole key already

    // The arrays the key names below that form, around the row's value.
    std::string form;
    for (size_t i = depth; i < key.size(); ++i)  // each segment as written, quoted or bare
        form += (i > depth ? " (" : "(") +
                std::string(text.substr(key[i].begin, key[i].end - key[i].begin));
    const size_t from = s.tokens[static_cast<size_t>(br.tag)].end;
    form.append(text.substr(from, br.end - 1 - from));
    form.append(key.size() - depth, ')');

    Rewrite r;
    if (into < 0) {  // no form to fold into: in the row's place
        r.edits.push_back({br.begin, br.end, std::move(form)});
        r.fresh_edit = 0;
        return r;
    }
    // Into the form, after its last child: on a line of its own at that child's column if
    // the child starts its line or a comment ends it, else beside it.
    const Bracket& in = s.brackets[static_cast<size_t>(into)];
    if (!in.closed) return std::nullopt;
    int last = -1;
    for (size_t c = static_cast<size_t>(into) + 1;
         c < s.brackets.size() && s.brackets[c].begin < in.end; ++c)
        if (s.brackets[c].parent == into) last = static_cast<int>(c);
    TextEdit insert;
    if (last < 0) {
        insert = {in.end - 1, in.end - 1, " " + form};
    } else {
        const Bracket& lb = s.brackets[static_cast<size_t>(last)];
        const size_t line = text.rfind('\n', lb.begin) + 1;  // npos + 1 is 0
        const size_t eol = std::min(text.find('\n', lb.end), in.end - 1);
        size_t at = lb.end;
        for (const Comment& c : s.comments)
            if (c.begin >= lb.end && c.begin < eol) at = c.end;  // after a comment on its line
        const bool starts_line =
            text.substr(line, lb.begin - line).find_first_not_of(" \t") == std::string_view::npos;
        insert = {at, at,
                  starts_line || at != lb.end ? "\n" + std::string(lb.begin - line, ' ') + form
                                              : " " + form};
    }
    // The row out, with its line when it is alone on it.
    size_t cut = br.begin, end = br.end;
    while (cut > 0 && (text[cut - 1] == ' ' || text[cut - 1] == '\t')) --cut;
    size_t after = end;
    while (after < text.size() &&
           (text[after] == ' ' || text[after] == '\t' || text[after] == '\r'))
        ++after;
    if ((cut == 0 || text[cut - 1] == '\n') && (after == text.size() || text[after] == '\n'))
        end = after < text.size() ? after + 1 : after;
    else
        cut = br.begin;
    const TextEdit remove{cut, end, {}};
    if (insert.begin < remove.begin) {
        r.edits.push_back(std::move(insert));
        r.edits.push_back(remove);
    } else {
        r.edits.push_back(remove);
        r.edits.push_back(std::move(insert));
    }
    return r;
}

std::string apply(std::string_view text, Rewrite& r, const baker::WriteOptions& layout) {
    std::string out;
    size_t at = 0, begin = 0, end = 0;  // the new form's bytes in `out`
    for (size_t i = 0; i < r.edits.size(); ++i) {
        const TextEdit& e = r.edits[i];
        out.append(text, at, e.begin - at);
        if (static_cast<int>(i) == r.fresh_edit) {
            begin = out.size() + r.fresh;
            end = out.size() + e.text.size();
        }
        out += e.text;
        at = e.end;
    }
    out.append(text, at);
    if (r.fresh_edit < 0) return out;
    std::string error;  // a file that does not read is left as written
    const std::vector<TextEdit> edits = format_edits(out, layout, error, begin, end);
    if (edits.empty()) return out;
    std::string laid(out, 0, begin);
    at = begin;
    for (const TextEdit& e : edits) {  // in order, within the new form
        laid.append(out, at, e.begin - at);
        laid += e.text;
        at = e.end;
    }
    laid.append(out, at, end - at);
    TextEdit& fresh = r.edits[static_cast<size_t>(r.fresh_edit)];
    fresh.text = fresh.text.substr(0, r.fresh) + laid.substr(begin);
    laid.append(out, end);
    return laid;
}

bool same_tree(std::string_view before, std::string_view after, const std::string& name,
               const ReadSetup& setup, std::string& why) {
    std::vector<baker::Symbol> levels;
    for (const std::string& l : setup.pack_levels) levels.emplace_back(l);
    const auto read = [&](std::string_view text) {
        baker::LoadSession session;
        if (setup.reader) session.set_reader(setup.reader);
        if (!levels.empty())
            session.set_pack_levels(std::span<const baker::Symbol>(levels.data(), levels.size()));
        for (const std::string& d : setup.include_dirs) session.add_include_dir(d);
        for (const std::string& d : setup.defines) session.define(baker::Symbol(d));
        return baker::read_string(text, name, session);
    };
    const auto a = read(before);
    if (!a) {
        why = "the file does not read: " + std::string(a.error().message());
        return false;
    }
    const auto b = read(after);
    if (!b) {
        why = "the rewritten file would not read: " + std::string(b.error().message());
        return false;
    }
    if (canonical(**a) != canonical(**b)) {
        why = "the rewritten file would read to another tree";
        return false;
    }
    return true;
}

}  // namespace baker_lsp
