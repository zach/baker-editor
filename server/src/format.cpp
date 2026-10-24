#include "format.hpp"

#include <baker/tools/format.hpp>

#include <algorithm>
#include <cstdint>

namespace baker_lsp {

std::vector<TextEdit> format_edits(std::string_view text, const baker::WriteOptions& layout,
                                   std::string& error, size_t begin, size_t end) {
    baker::tools::FormatOptions options;
    options.layout = layout;
    options.begin = static_cast<uint32_t>(std::min<size_t>(begin, UINT32_MAX));
    options.end = static_cast<uint32_t>(std::min<size_t>(end, UINT32_MAX));
    // The laid-out text is not needed: the edits make it.
    const baker::tools::Formatted f = baker::tools::format_text(text, baker::TextSink{}, options);
    std::vector<TextEdit> out;
    if (!f.errors.empty()) {
        error = std::string(f.errors.begin()->message());
        return out;
    }
    out.reserve(f.edits.size());
    for (const baker::tools::FormatEdit& e : f.edits) {
        TextEdit& t = out.emplace_back();
        t.begin = e.begin;
        t.end = e.end;
        for (uint32_t i = 0; i < e.newlines; ++i) t.text += f.line_end;
        t.text.append(e.spaces, ' ');
    }
    return out;
}

}  // namespace baker_lsp
