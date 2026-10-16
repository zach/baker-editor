#include "features.hpp"

#include <baker/std_interop.hpp>

#include <algorithm>
#include <cstdio>

namespace baker_lsp {
namespace {

bool matches(const baker::List<baker::Symbol>& at, const std::vector<std::string>& tags,
             size_t count) {
    if (at.size() != count) return false;
    for (size_t i = 0; i < count; ++i)
        if (at.begin()[i].str() != "*" && at.begin()[i].str() != tags[i]) return false;
    return true;
}

std::string type_name(const baker::shape::Field& f) {
    using baker::shape::Type;
    const auto name = [&f](Type t) -> std::string {
        switch (t) {
            case Type::Any: return "any";
            case Type::Int: return "int";
            case Type::Float: return "float";
            case Type::Bool: return "bool";
            case Type::Symbol: return "symbol";
            case Type::String: return "string";
            case Type::List: return "list";
            case Type::Shape: return std::string(f.shape.str());
        }
        return "any";
    };
    if (f.type == Type::List) return "(list " + name(f.element) + ")";
    return name(f.type);
}

std::string number(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    return b;
}

std::string where(baker::Symbol file, int line) {
    std::string f(file.str());
    if (const size_t slash = f.rfind('/'); slash != std::string::npos) f = f.substr(slash + 1);
    return line > 0 ? f + ":" + std::to_string(line) : f;
}

// The field's type and metadata, as markdown lines.
std::string describe(const baker::shape::Shape& shape, const baker::shape::Field& f) {
    std::string s = "`" + type_name(f) + "`";
    if (f.ranged) s += ", " + number(f.min) + " to " + number(f.max);
    if (f.step > 0) s += ", step " + number(f.step);
    if (!f.kind.str().empty()) s += ", " + std::string(f.kind.str());
    if (f.optional) s += ", optional";
    s += "; shape `" + std::string(shape.name.str()) + "` (" + where(shape.file, shape.line) + ")";
    if (!f.desc.empty()) s += "\n\n" + std::string(f.desc);
    if (!f.note.empty()) s += "\n\n*" + std::string(f.note) + "*";
    return s;
}

const char* standing_name(baker::Standing s) {
    switch (s) {
        case baker::Standing::Wins: return "wins";
        case baker::Standing::Shadowed: return "shadowed";
        case baker::Standing::Removes: return "removes it";
        case baker::Standing::Disabled: return "disabled layer";
    }
    return "";
}

const FunctionInfo* function_named(const Reference& r, const std::string& name) {
    for (const FunctionInfo& f : r.all)
        if (f.name == name) return &f;
    return nullptr;
}

std::string function_markdown(const FunctionInfo& f) {
    std::string s = "```baker\n";
    for (const std::string& sig : f.signatures) s += sig + "\n";
    s += "```";
    if (f.status != "normal") s += "\n\n**" + f.status + "**";
    if (!f.doc.empty()) s += "\n\n" + f.doc;
    return s;
}

const Definition* defined(const std::vector<Definition>& list, const std::string& name) {
    for (const Definition& d : list)
        if (d.name == name) return &d;
    return nullptr;
}

}  // namespace

const baker::shape::Shape* governing(const baker::shape::Set& shapes,
                                     const std::vector<std::string>& tags) {
    if (tags.empty()) return nullptr;
    const size_t parents = tags.size() - 1;
    for (const baker::shape::Shape& s : shapes.shapes()) {
        if (s.keyed) {
            if (!s.at.empty() && matches(s.at, tags, parents)) return &s;
        } else if (s.name.str() == tags.back() && (s.at.empty() || matches(s.at, tags, parents))) {
            return &s;
        }
    }
    return nullptr;
}

std::optional<Hover> hover(std::string_view text, size_t offset, const Loaded& loaded,
                           const Reference& reference) {
    const Cursor c = scan(text, offset);
    if (c.quiet || c.token.empty()) return std::nullopt;
    Hover h{{}, c.begin, c.end};

    // A command's head: one of the game's functions, or a script function or object.
    if (!c.stack.empty() && c.stack.back().open == '{' && c.index == 0) {
        if (const FunctionInfo* f = function_named(reference, c.token)) {
            h.markdown = function_markdown(*f);
            return h;
        }
        if (const Definition* d = defined(loaded.functions, c.token)) {
            h.markdown = "script function `" + d->name + "`, defined at " +
                         where(baker::Symbol(d->file), d->line);
            return h;
        }
        if (std::find(reference.objects.begin(), reference.objects.end(), c.token) !=
            reference.objects.end()) {
            h.markdown = "object `" + c.token + "`, which the game names";
            return h;
        }
        return std::nullopt;
    }
    if (const Definition* d = defined(loaded.macros, c.token)) {
        h.markdown =
            "macro `" + d->name + "`, defined at " + where(baker::Symbol(d->file), d->line);
        return h;
    }

    // Data: the array the cursor's token names, or holds.
    std::vector<std::string> tags = c.data_path();
    if (tags.empty() && !(c.index == 0 && !c.stack.empty())) return std::nullopt;
    if (c.index == 0) {  // the tag of an array being named
        tags = c.parent_path();
        if (c.stack.size() > 1 && tags.empty()) return std::nullopt;
        tags.push_back(c.token);
    }
    if (tags.empty()) return std::nullopt;
    const std::string path = path_text(tags);
    h.markdown = "`" + path + "`";

    if (tags.size() >= 2) {
        const std::vector<std::string> parent(tags.begin(), tags.end() - 1);
        if (const baker::shape::Shape* s = governing(loaded.shapes, parent))
            if (const baker::shape::Field* f = s->field(baker::Symbol(tags.back())))
                h.markdown += "\n\n" + describe(*s, *f);
    }
    if (const baker::shape::Shape* s = governing(loaded.shapes, tags))
        h.markdown +=
            "\n\nshape `" + std::string(s->name.str()) + "` (" + where(s->file, s->line) + ")";

    if (loaded.config)
        if (auto chain = loaded.config->explain(baker::Path(path)); chain && !chain->empty()) {
            h.markdown += "\n\n| layer | value | where | |\n|---|---|---|---|";
            for (const baker::ExplainEntry& e : *chain)
                h.markdown += "\n| " + std::string(e.layer.str()) + " | `" +
                              baker::to_string(e.value) + "` | " +
                              where(e.source.file, e.source.line) + " | " +
                              standing_name(e.standing) + " |";
            h.markdown += "\n\n*from the files*";
        }
    return h;
}

Completions complete(std::string_view text, size_t offset, const Loaded& loaded,
                     const Reference& reference) {
    const Cursor c = scan(text, offset);
    Completions out;
    if (c.quiet) return out;
    out.begin = c.begin;
    out.end = offset;  // replace what is typed up to the cursor
    const std::string typed(text.substr(c.begin, offset - c.begin));
    const auto offer = [&](Completion item) {
        if (item.label.compare(0, typed.size(), typed) == 0) out.items.push_back(std::move(item));
    };

    if (!c.stack.empty() && c.index == 0) {
        const Frame& inner = c.stack.back();
        if (inner.open == '{') {  // a command: functions, script functions, objects
            for (const FunctionInfo& f : reference.all) {
                if (f.status == "deprecated") continue;
                offer({f.name, Kind::Function,
                       f.signatures.empty() ? std::string() : f.signatures.front(),
                       function_markdown(f)});
            }
            for (const Definition& d : loaded.functions)
                if (!function_named(reference, d.name))
                    offer({d.name, Kind::Function, "script function",
                           "defined at " + where(baker::Symbol(d.file), d.line)});
            for (const std::string& o : reference.objects) offer({o, Kind::Variable, "object", {}});
            return out;
        }
        if (inner.open == '(') {  // naming an array: the governing shape's fields
            const std::vector<std::string> parent = c.parent_path();
            if (!parent.empty())
                if (const baker::shape::Shape* s = governing(loaded.shapes, parent))
                    for (const baker::shape::Field& f : s->fields)
                        offer({std::string(f.name.str()), Kind::Field, type_name(f),
                               describe(*s, f)});
            return out;
        }
        return out;
    }
    // A value. In (requires ...) of a pack: the packs.
    if (!c.stack.empty() && c.stack.back().tag == "requires") {
        for (const std::string& p : loaded.packs) offer({p, Kind::Module, "pack", {}});
        return out;
    }
    for (const Definition& d : loaded.macros)
        offer({d.name, Kind::Constant, "macro",
               "defined at " + where(baker::Symbol(d.file), d.line)});
    return out;
}

}  // namespace baker_lsp
