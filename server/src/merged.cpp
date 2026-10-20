#include "merged.hpp"

#include "scan.hpp"
#include "setup.hpp"

#include <baker/std_interop.hpp>

#include <unordered_map>

namespace baker_lsp {
namespace {

// A line of the merged view, its comment apart, so closing brackets go before the comment.
struct Line {
    std::string content, comment;
    std::string file;  // where the value is written, or empty
    int file_line = 0;
};

class Printer {
public:
    static constexpr size_t kWidth = 80;

    Printer(const baker::Config& cfg, std::string_view dir) : cfg_(cfg), dir_(dir) {}

    void top(const baker::Array& root) {
        for (const baker::Node& n : root) {
            const baker::Array* a = n.type() == baker::Type::Array ? n.array() : nullptr;
            if (a && !a->tag().empty()) {
                tags_.assign(1, std::string(a->tag().str()));
                array(*a, 0);
            } else {
                lines_.push_back({baker::to_string(n), {}, {}, 0});
            }
        }
    }

    View view() {
        View out;
        for (size_t i = 0; i < lines_.size(); ++i) {
            const Line& l = lines_[i];
            out.text += l.content;
            if (!l.comment.empty()) out.text += "  ; " + l.comment;
            out.text += '\n';
            if (!l.file.empty()) out.sources.push_back({static_cast<int>(i), l.file, l.file_line});
        }
        return out;
    }

private:
    static bool has_arrays(const baker::Array& a) {
        for (const baker::Node& n : a)
            if (n.type() == baker::Type::Array) return true;
        return false;
    }

    // `a`, whose path is tags_, at `depth`.
    void array(const baker::Array& a, int depth) {
        const std::string indent(static_cast<size_t>(depth) * 2, ' ');
        if (!has_arrays(a)) {
            Line l{indent + baker::to_string(a), {}, {}, 0};
            source(l);
            lines_.push_back(std::move(l));
            return;
        }
        // An array that fits on a line, as Baker's writer keeps it, is one line, if every
        // value in it comes from the same place: its children each one line with one source.
        const size_t first = lines_.size();
        expand(a, depth, indent);
        const Line& head = lines_[first];
        if (head.content.size() > indent.size() + kWidth) return;
        std::string joined = head.content;
        for (size_t i = first + 1; i < lines_.size(); ++i) {
            const Line& l = lines_[i];
            if (l.file.empty() || l.comment != lines_[first + 1].comment) return;
            const std::string_view content = std::string_view(l.content).substr(indent.size() + 2);
            if (content.find('\n') != std::string_view::npos) return;
            joined += ' ';
            joined += content;
            if (joined.size() > kWidth) return;
        }
        if (lines_.size() == first + 1) return;
        Line l{std::move(joined), lines_[first + 1].comment, lines_[first + 1].file,
               lines_[first + 1].file_line};
        lines_.resize(first);
        lines_.push_back(std::move(l));
    }

    // `a` over several lines: the values before its first array on the head line.
    void expand(const baker::Array& a, int depth, const std::string& indent) {
        std::string head = indent + "(";
        size_t i = 0;
        for (; i < a.size() && a[i].type() != baker::Type::Array; ++i)
            head += (i ? " " : "") + baker::to_string(a[i]);
        lines_.push_back({std::move(head), {}, {}, 0});
        for (; i < a.size(); ++i) {
            const baker::Array* child = a[i].type() == baker::Type::Array ? a[i].array() : nullptr;
            if (child && !child->tag().empty()) {
                tags_.emplace_back(child->tag().str());
                array(*child, depth + 1);
                tags_.pop_back();
            } else {
                lines_.push_back({indent + "  " + baker::to_string(a[i]), {}, {}, 0});
            }
        }
        lines_.back().content += ')';
    }

    void source(Line& l) {
        const auto s = cfg_.source(baker::Path(path_text(tags_)));
        if (!s || s->file.str().empty()) {
            if (s) l.comment = std::string(s->layer.str());
            return;
        }
        auto [known, added] = canonical_.try_emplace(std::string(s->file.str()));
        if (added) known->second = canonical_path(known->first);
        l.file = known->second;
        std::string_view shown = l.file;
        if (shown.starts_with(dir_) && shown.size() > dir_.size() && shown[dir_.size()] == '/')
            shown.remove_prefix(dir_.size() + 1);
        l.comment =
            std::string(s->layer.str()) + " " + std::string(shown) + ":" + std::to_string(s->line);
        l.file_line = s->line;
    }

    const baker::Config& cfg_;
    std::string_view dir_;
    std::vector<std::string> tags_;
    std::vector<Line> lines_;
    std::unordered_map<std::string, std::string> canonical_;  // a file as Baker names it
};

}  // namespace

View merged_view(const baker::Config& cfg, std::string_view dir) {
    Printer p(cfg, dir);
    if (const baker::Ref<baker::Array> root = cfg.root()) p.top(*root);
    return p.view();
}

std::optional<View> layer_view(const baker::Config& cfg, std::string_view layer,
                               std::string& error) {
    const auto effect = cfg.layer_effect(layer);
    if (!effect) {
        error = baker::to_string(effect.error());
        return std::nullopt;
    }
    View out;
    if (effect->empty()) {
        out.text = "layer '" + std::string(layer) + "' changes nothing\n";
        return out;
    }
    out.text = "layer '" + std::string(layer) + "' changes:\n";
    int line = 1;
    for (const baker::LayerEffectEntry& e : *effect) {
        const std::string file(e.source.file.str());
        out.text += "  " + std::string(e.path.str()) + "  " +
                    (e.below ? baker::to_string(*e.below) : std::string("(new)")) + " -> " +
                    baker::to_string(e.value) + "  (" + file + ":" + std::to_string(e.source.line) +
                    ")" + (e.shadowed ? "  shadowed" : "") + "\n";
        if (!file.empty()) out.sources.push_back({line, canonical_path(file), e.source.line});
        ++line;
    }
    return out;
}

}  // namespace baker_lsp
