#include "hints.hpp"

#include "features.hpp"
#include "scan.hpp"
#include "setup.hpp"

#include <baker/config.hpp>
#include <baker/std_interop.hpp>
#include <baker/tweak.hpp>

#include <cstdio>

namespace baker_lsp {
namespace {

// Reads `a`'s metadata arrays into `h`, over what it holds.
void read_meta(const baker::Array& a, Hints& h) {
    for (const baker::Node& n : baker::split_metadata(a.values()).metadata) {
        const baker::Array* m = n.try_array();
        if (!m) continue;
        const std::string_view tag = m->tag().str();
        h.written = true;
        if (tag == "range" && m->size() == 3 && (*m)[1].try_float() && (*m)[2].try_float()) {
            h.min = *(*m)[1].try_float();
            h.max = *(*m)[2].try_float();
            h.ranged = h.min < h.max;
        } else if (tag == "one_of") {
            h.one_of.clear();
            for (size_t i = 1; i < m->size(); ++i)
                if (const auto sym = (*m)[i].try_sym()) h.one_of.emplace_back(sym->str());
        } else if (tag == "step" && m->size() == 2 && (*m)[1].try_float()) {
            h.step = *(*m)[1].try_float();
        } else if (m->size() == 2 && (tag == "kind" || tag == "desc" || tag == "note")) {
            const baker::Node& v = (*m)[1];
            std::string text = v.try_str()   ? std::string(*v.try_str())
                               : v.try_sym() ? std::string(v.try_sym()->str())
                                             : baker::to_string(v);
            (tag == "kind" ? h.kind : tag == "desc" ? h.desc : h.note) = std::move(text);
        }
    }
}

std::string number(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    return b;
}

// Whether `a` holds values alone, no arrays: (health 100).
bool values_only(const baker::Array& a) {
    for (const baker::Node& n : a)
        if (n.type() == baker::Type::Array) return false;
    return a.size() > 1;
}

// A warning for the first number of `a` (at `tags`) outside its range, if any.
using BakerChecks = std::function<bool(const std::string&)>;

void check(const baker::Array& a, const std::vector<std::string>& tags, const Loaded& loaded,
           const Reference& reference, const BakerChecks& baker_checks, std::vector<Problem>& out) {
    const Hints h = hints_at(loaded, reference, tags);
    if (!h.ranged || (h.field && h.field->ranged)) return;  // a shape's range is check's
    if (h.tweak && baker_checks && baker_checks(h.tweak->path)) return;  // check_tweaks's
    for (size_t i = 1; i < a.size(); ++i) {
        const auto v = a[i].try_float();
        if (!v || (*v >= h.min && *v <= h.max)) continue;
        const auto s = loaded.config->source(baker::Path(path_text(tags)));
        if (!s || s->file.str().empty()) return;
        Problem p;
        p.file = canonical_path(std::string(s->file.str()));
        p.line = s->line;
        p.severity = Severity::Warning;
        p.code = "range";
        p.key = tags.back();
        p.message = "'" + tags.back() + "' is " + baker::to_string(a[i]) + ", outside " +
                    number(h.min) + " to " + number(h.max) +
                    (h.tweak && !h.written ? " (the game's tweak " + h.tweak->path + ")" : "") +
                    ": the running game refuses it";
        out.push_back(std::move(p));
        return;
    }
}

void walk(const baker::Array& a, std::vector<std::string>& tags, const Loaded& loaded,
          const Reference& reference, const BakerChecks& baker_checks, std::vector<Problem>& out) {
    for (const baker::Node& n : a) {
        const baker::Array* child = n.try_array();
        if (!child || child->tag().empty()) continue;
        tags.emplace_back(child->tag().str());
        if (has_meta(*child) || (values_only(*child) && tweak_at(reference, tags)))
            check(*child, tags, loaded, reference, baker_checks, out);
        else
            walk(*child, tags, loaded, reference, baker_checks, out);
        tags.pop_back();
    }
}

}  // namespace

bool is_meta_tag(std::string_view tag) {
    return tag == "range" || tag == "step" || tag == "kind" || tag == "desc" || tag == "note" ||
           tag == "group" || tag == "one_of";
}

bool has_meta(const baker::Array& a) {
    return !baker::split_metadata(a.values()).metadata.empty();
}

const baker::Array* array_at(const Loaded& loaded, const std::vector<std::string>& tags) {
    if (!loaded.config || tags.empty()) return nullptr;
    const baker::Ref<baker::Array> root = loaded.config->root();
    const baker::Array* a = root ? root.get() : nullptr;
    for (const std::string& t : tags) {
        if (!a) return nullptr;
        a = a->try_find(baker::Symbol(t));
    }
    return a;
}

Hints hints_at(const Loaded& loaded, const Reference& reference,
               const std::vector<std::string>& tags) {
    Hints h;
    if (tags.empty()) return h;
    if ((h.tweak = tweak_at(reference, tags))) {
        h.ranged = h.tweak->ranged;
        h.min = h.tweak->min;
        h.max = h.tweak->max;
        h.step = h.tweak->step;
        h.kind = h.tweak->kind;
        h.desc = h.tweak->desc;
        h.one_of = h.tweak->one_of;
    }
    if (tags.size() >= 2) {
        const std::vector<std::string> parent(tags.begin(), tags.end() - 1);
        if ((h.shape = governing(loaded.shapes, parent)))
            if ((h.field = h.shape->field(baker::Symbol(tags.back())))) {
                const baker::shape::Field& f = *h.field;
                if (f.ranged) {
                    h.ranged = true;
                    h.min = f.min;
                    h.max = f.max;
                }
                if (f.step > 0) h.step = f.step;
                if (!f.kind.str().empty()) h.kind = std::string(f.kind.str());
                if (!f.desc.empty()) h.desc = std::string(f.desc);
                if (!f.note.empty()) h.note = std::string(f.note);
                if (!f.one_of.empty()) {
                    h.one_of.clear();
                    for (const baker::Symbol sym : f.one_of) h.one_of.emplace_back(sym.str());
                }
            }
        if (!h.field) h.shape = nullptr;
    }
    if (const baker::Array* a = array_at(loaded, tags); a && has_meta(*a)) read_meta(*a, h);
    return h;
}

Hints written_at(const Loaded& loaded, const std::vector<std::string>& tags) {
    Hints h;
    if (const baker::Array* a = array_at(loaded, tags); a && has_meta(*a)) read_meta(*a, h);
    return h;
}

bool is_color(const Loaded& loaded, const Hints& h, const std::vector<std::string>& tags) {
    const baker::Array* a = array_at(loaded, tags);
    if (!a) return false;
    const baker::TweakRange range =
        h.ranged ? baker::TweakRange{h.min, h.max} : baker::TweakRange{};
    return baker::presented_kind(a->values(), h.kind, range) == "color";
}

std::vector<Problem> range_problems(const Loaded& loaded, const Reference& reference,
                                    const BakerChecks& baker_checks) {
    std::vector<Problem> out;
    if (!loaded.config) return out;
    const baker::Ref<baker::Array> root = loaded.config->root();
    if (!root) return out;
    std::vector<std::string> tags;
    walk(*root, tags, loaded, reference, baker_checks, out);
    return out;
}

}  // namespace baker_lsp
