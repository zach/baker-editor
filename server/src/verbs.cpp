#include "verbs.hpp"

#include "scan.hpp"

#include <algorithm>

namespace baker_lsp {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string joined(const std::vector<std::string>& values, size_t from = 0) {
    std::string out;
    for (size_t i = from; i < values.size(); ++i) out += (out.empty() ? "" : " ") + values[i];
    return out;
}

// 1-based line of byte `at`.
int line_of(std::string_view text, size_t at) {
    return 1 +
           static_cast<int>(std::count(text.begin(), text.begin() + static_cast<long>(at), '\n'));
}

}  // namespace

std::vector<RowEdit> row_edits(const Structure& s, std::string_view text, size_t offset,
                               const std::string& file, const std::vector<LayerRow>& rows) {
    std::vector<RowEdit> out;
    // The innermost array around the cursor that is a row of a layer: out of its metadata and
    // a verb's array operands.
    int b = -1;
    for (size_t k = 0; k < s.brackets.size() && s.brackets[k].begin <= offset; ++k)
        if (offset < s.brackets[k].end) b = static_cast<int>(k);
    const LayerRow* row = nullptr;
    for (; b >= 0 && !row; b = row ? b : s.brackets[static_cast<size_t>(b)].parent) {
        if (s.is_metadata(b)) continue;
        const std::vector<std::string> path = s.data_path(b);
        if (path.empty()) continue;
        const std::string p = path_text(path);
        const int line = line_of(text, s.brackets[static_cast<size_t>(b)].begin);
        for (const LayerRow& r : rows)
            if (r.file == file && r.line == line && r.path == p) row = &r;
    }
    if (!row) return out;
    const Bracket& br = s.brackets[static_cast<size_t>(b)];
    if (!br.closed || br.tag < 0) return out;

    // Its values (a verb's with its operands), up to its metadata; and its pin.
    size_t begin = s.tokens[static_cast<size_t>(br.tag)].end, end = br.end - 1;
    int was = -1;
    for (size_t c = static_cast<size_t>(b) + 1;
         c < s.brackets.size() && s.brackets[c].begin < br.end; ++c) {
        if (s.brackets[c].parent != b || !s.is_metadata(static_cast<int>(c))) continue;
        end = std::min(end, s.brackets[c].begin);
        if (s.tag(static_cast<int>(c)) == "@was") was = static_cast<int>(c);
    }
    while (begin < end && is_space(text[begin])) ++begin;
    while (end > begin && is_space(text[end - 1])) --end;
    const auto edit = [&](const char* title, const char* kind, const char* id, TextEdit e) {
        out.push_back({title, kind, id, std::move(e)});
    };

    if (row->stale && was >= 0 && !row->below_values.empty()) {
        const Bracket& w = s.brackets[static_cast<size_t>(was)];
        edit(
            "Update the pin to the value below", "quickfix", "repin",
            {s.tokens[static_cast<size_t>(w.tag)].end, w.end - 1, " " + joined(row->below_values)});
    }
    if (row->composes() && !row->result_values.empty())
        edit("Restate as a list", "refactor.rewrite", "restate",
             {begin, end, joined(row->result_values)});
    if (row->op.empty() && !row->below_values.empty()) {
        const std::vector<std::string>& below = row->below_values;
        if (row->values.size() > below.size() &&
            std::equal(below.begin(), below.end(), row->values.begin()))
            edit("Write as #append", "refactor.rewrite", "append",
                 {begin, end, "#append " + joined(row->values, below.size())});
        if (was < 0)
            edit("Pin to the value below", "refactor.rewrite", "pin",
                 {br.end - 1, br.end - 1, " (@was " + joined(below) + ")"});
    }
    return out;
}

void lay_out(const Structure& s, std::string_view text, TextEdit& e,
             const baker::WriteOptions& layout) {
    int top = -1;
    for (size_t b = 0; b < s.brackets.size(); ++b)
        if (s.brackets[b].parent < 0 && s.brackets[b].begin <= e.begin &&
            e.end <= s.brackets[b].end)
            top = static_cast<int>(b);
    if (top < 0) return;
    const Bracket& t = s.brackets[static_cast<size_t>(top)];
    std::string after(text.substr(0, e.begin));
    after += e.text;
    after.append(text.substr(e.end));
    const size_t end = t.end + e.text.size() - (e.end - e.begin);  // the form's, in `after`
    std::string error;  // a file that does not read is left as written
    const std::vector<TextEdit> edits = format_edits(after, layout, error, t.begin, end);
    std::string form;
    size_t at = t.begin;
    for (const TextEdit& f : edits) {  // in order, within the form
        form.append(after, at, f.begin - at);
        form += f.text;
        at = f.end;
    }
    form.append(after, at, end - at);
    e = {t.begin, t.end, std::move(form)};
}

}  // namespace baker_lsp
