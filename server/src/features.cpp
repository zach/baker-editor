#include "features.hpp"

#include "hints.hpp"
#include "structure.hpp"

#include <baker/std_interop.hpp>

#include <algorithm>
#include <cstdio>
#include <map>

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
    for (size_t i = 0; i < f.one_of.size(); ++i)
        s += (i ? ", " : ": one of ") + std::string(f.one_of[i].str());
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

std::optional<Hover> hover(const FileRead& read, size_t offset, const Loaded& loaded,
                           const Reference& reference) {
    const Cursor c = scan(read.structure, read.text, offset);
    if (c.quiet || c.token.empty()) return std::nullopt;
    Hover h{{}, c.begin, c.end, {}};

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
            if (const auto t = reference.object_types.find(c.token);
                t != reference.object_types.end()) {
                h.markdown += ": a `" + t->second + "`";
                const std::vector<const FunctionInfo*> methods = methods_of(reference, c.token);
                for (size_t i = 0; i < methods.size(); ++i)
                    h.markdown += (i ? ", `" : "\n\nmethods: `") + methods[i]->name + "`";
            }
            return h;
        }
        return std::nullopt;
    }
    // A method, in a call on one of the game's objects: {scheduler delay_task ...}.
    if (!c.stack.empty() && c.stack.back().open == '{' && c.index == 1)
        for (const FunctionInfo* m : methods_of(reference, c.stack.back().tag))
            if (m->name == c.token) {
                h.markdown = function_markdown(*m) + "\n\nmethod of `" +
                             reference.object_types.at(c.stack.back().tag) + "`";
                return h;
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
    h.path = path;
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

    // The metadata written beside the value, and the game's tweak at its path.
    const Hints written = written_at(loaded, tags);
    if (written.written) {
        const Hints& hints = written;
        std::string m = "in the data:";
        if (hints.ranged) m += " " + number(hints.min) + " to " + number(hints.max) + ",";
        if (hints.step > 0) m += " step " + number(hints.step) + ",";
        if (!hints.kind.empty()) m += " " + hints.kind + ",";
        for (size_t i = 0; i < hints.one_of.size(); ++i)
            m += (i ? " " : " one of ") + hints.one_of[i] +
                 (i + 1 == hints.one_of.size() ? "," : "");
        if (m.back() == ',') m.pop_back();
        if (!hints.desc.empty()) m += "\n\n" + hints.desc;
        if (!hints.note.empty()) m += "\n\n*" + hints.note + "*";
        h.markdown += "\n\n" + m;
    }
    if (const TweakInfo* t = tweak_at(reference, tags)) {
        std::string m = "the game's tweak `" + t->path + "`: `" + t->type + "`, code default `" +
                        t->default_text + "`";
        if (t->ranged) m += ", " + number(t->min) + " to " + number(t->max);
        if (t->step > 0) m += ", step " + number(t->step);
        if (!t->kind.empty()) m += ", " + t->kind;
        for (size_t i = 0; i < t->one_of.size(); ++i) m += (i ? " " : ", one of ") + t->one_of[i];
        if (t->dev_only) m += "; dev only: a shipping build keeps the default";
        m += t->bound ? "; the data sets it" : "; the data does not set it: the default stands";
        if (!t->desc.empty() && t->desc != written.desc) m += "\n\n" + t->desc;
        h.markdown += "\n\n" + m;
    }

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

std::string data_path_at(const FileRead& read, size_t offset) {
    const Cursor c = scan(read.structure, read.text, offset);
    if (c.quiet) return {};
    std::vector<std::string> tags = c.data_path();
    if (c.index == 0 && !c.token.empty()) {
        tags = c.parent_path();
        if (c.stack.size() > 1 && tags.empty()) return {};
        tags.push_back(c.token);
    }
    return tags.empty() ? std::string() : path_text(tags);
}

std::vector<Location> definition(const FileRead& read, size_t offset, const std::string& file,
                                 const Loaded& loaded, const Reference& reference,
                                 const std::function<bool(const std::string&)>& exists) {
    const std::string_view text = read.text;
    const Cursor c = scan(read.structure, text, offset);
    std::vector<Location> out;
    if (c.token.empty()) return out;
    const std::string dir = file.substr(0, file.rfind('/') + 1);

    if (c.in_string) {  // a file another names: a directive's, or a layer's in a project file
        const bool directive = c.previous == "#include" || c.previous == "#require" ||
                               c.previous == "#merge" || c.previous == "#include_opt";
        const bool layer =
            file.size() >= 17 && file.compare(file.size() - 17, 17, "baker-project.dta") == 0;
        if (!directive && !layer) return out;
        std::vector<std::string> candidates{c.token.front() == '/' ? c.token : dir + c.token};
        if (directive)
            for (const std::string& d : loaded.include_dirs)
                candidates.push_back(d + "/" + c.token);
        for (const std::string& candidate : candidates) {
            const std::string path = canonical_path(candidate);
            if (exists(path)) return {{path, 0}};
        }
        return out;
    }
    if (c.quiet) return out;

    const auto at = [&out](const Definition& d) { out.push_back({d.file, d.line}); };
    if (!c.stack.empty() && c.stack.back().open == '{' && c.index == 0) {
        if (const FunctionInfo* f = function_named(reference, c.token); f && !f->file.empty()) {
            out.push_back({canonical_path(f->file), f->line});
            return out;
        }
        if (const Definition* d = defined(loaded.functions, c.token)) at(*d);
        return out;
    }
    if (const Definition* d = defined(loaded.macros, c.token)) {
        at(*d);
        return out;
    }
    if (!c.stack.empty() && c.stack.back().tag == "requires" && c.index > 0) {
        if (const Definition* d = defined(loaded.packs, c.token)) at(*d);
        return out;
    }

    std::vector<std::string> tags = c.data_path();
    if (c.index == 0) {
        tags = c.parent_path();
        if (c.stack.size() > 1 && tags.empty()) return out;
        tags.push_back(c.token);
    }
    if (tags.empty() || !loaded.config) return out;
    // The line the cursor is on, to leave out the contributor it is.
    const int line =
        1 + static_cast<int>(std::count(text.begin(),
                                        text.begin() + static_cast<std::ptrdiff_t>(c.begin), '\n'));
    if (auto chain = loaded.config->explain(baker::Path(path_text(tags))))
        for (const baker::ExplainEntry& e : *chain) {
            if (e.source.file.str().empty()) continue;
            const std::string f = canonical_path(std::string(e.source.file.str()));
            if (f == file && e.source.line == line) continue;
            out.push_back({f, e.source.line});
        }
    return out;
}

namespace {

// A snippet's text for `s`, with its special characters escaped.
std::string snippet_text(std::string_view s) {
    std::string out;
    for (const char ch : s) {
        if (ch == '$' || ch == '}' || ch == '\\') out += '\\';
        out += ch;
    }
    return out;
}

// What choosing field `name` at `tags` (its own path) writes after it: the value the stack has
// there now, else the tweak's code default, else the middle of its range, else its type's
// zero. Empty when nothing fits.
std::string value_for(const Loaded& loaded, const Reference& reference,
                      const std::vector<std::string>& tags, const baker::shape::Field* field) {
    if (const baker::Array* a = array_at(loaded, tags)) {
        std::string v;
        for (size_t i = 1; i < a->size(); ++i) {
            if ((*a)[i].try_array()) break;  // its metadata, or arrays of its own
            v += (v.empty() ? "" : " ") + baker::to_string((*a)[i]);
        }
        if (!v.empty()) return v;
    }
    const Hints h = hints_at(loaded, reference, tags);
    if (h.tweak && !h.tweak->default_text.empty()) return h.tweak->default_text;
    const bool integer = (field && field->type == baker::shape::Type::Int) ||
                         (h.tweak && h.tweak->type.find("int") != std::string::npos);
    if (h.ranged) {
        const double mid = (h.min + h.max) / 2;
        return integer ? std::to_string(static_cast<long long>(mid)) : number(mid);
    }
    if (integer) return "0";
    if (field && field->type == baker::shape::Type::Float) return "0.0";
    if (field && field->type == baker::shape::Type::Bool) return "0";
    if (field && field->type == baker::shape::Type::String) return "\"\"";
    return {};
}

bool symbol_like(const Token& t) {
    if (t.quote || t.text.empty()) return false;
    const char c0 = t.text.front();
    if (c0 == '$' || c0 == '#' || (c0 >= '0' && c0 <= '9')) return false;
    if ((c0 == '-' || c0 == '+' || c0 == '.') && t.text.size() > 1 &&
        ((t.text[1] >= '0' && t.text[1] <= '9') || t.text[1] == '.'))
        return false;
    return true;
}

// The symbols the project's files give an array tagged `tag` whose parent `shape` governs
// (or, with no shape, whose parent is tagged as `parent`'s last), by how often, most first.
std::vector<std::pair<std::string, int>> symbols_in_use(
    const Loaded& loaded, const std::vector<std::string>& parent, const std::string& tag,
    const std::string& file, const FileRead& here, size_t offset, const ReadFile& read) {
    const baker::shape::Shape* shape = governing(loaded.shapes, parent);
    std::map<std::string, int> counts;
    std::vector<std::string> files = loaded.files;
    if (std::find(files.begin(), files.end(), file) == files.end()) files.push_back(file);
    for (const std::string& f : files) {
        std::string own;
        Structure own_structure;
        if (f != file) {
            if (!read(f, own)) continue;
            own_structure = read_structure(own);
        }
        const Structure& s = f == file ? here.structure : own_structure;
        for (size_t b = 0; b < s.brackets.size(); ++b) {
            const Bracket& br = s.brackets[b];
            if (br.open != '(' || br.tag < 0 || s.tokens[br.tag].text != tag) continue;
            if (f == file && br.begin < offset && offset <= br.end) continue;  // being typed
            const size_t first = static_cast<size_t>(br.tag) + 1;
            if (first >= s.tokens.size() || s.tokens[first].bracket != static_cast<int>(b) ||
                !symbol_like(s.tokens[first]))
                continue;
            if (br.parent < 0) continue;
            const std::vector<std::string> where = s.data_path(br.parent);
            if (where.empty()) continue;
            if (shape ? governing(loaded.shapes, where) != shape : where.back() != parent.back())
                continue;
            ++counts[std::string(s.tokens[first].text)];
        }
    }
    std::vector<std::pair<std::string, int>> out(counts.begin(), counts.end());
    std::stable_sort(out.begin(), out.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    return out;
}

}  // namespace

Completions complete(const FileRead& here, size_t offset, const Loaded& loaded,
                     const Reference& reference, const std::string& file, const ReadFile& read) {
    const std::string_view text = here.text;
    const Cursor c = scan(here.structure, text, offset);
    Completions out;
    if (c.quiet) return out;
    out.begin = c.begin;
    out.end = offset;  // replace what is typed up to the cursor
    const std::string typed(text.substr(c.begin, offset - c.begin));
    const auto offer = [&](Completion item) {
        if (item.label.compare(0, typed.size(), typed) == 0) out.items.push_back(std::move(item));
    };

    // The method of a call on one of the game's objects: its type's methods.
    if (!c.stack.empty() && c.stack.back().open == '{' && c.index == 1) {
        const std::vector<const FunctionInfo*> methods = methods_of(reference, c.stack.back().tag);
        if (!methods.empty()) {
            for (const FunctionInfo* m : methods) {
                if (m->status == "deprecated") continue;
                offer({m->name, Kind::Method,
                       m->signatures.empty() ? std::string() : m->signatures.front(),
                       function_markdown(*m)});
            }
            return out;
        }
    }
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
            if (parent.empty()) return out;
            // Choosing one writes a value after it, as a placeholder to change.
            const auto with_value = [&](Completion item, const baker::shape::Field* f) {
                std::vector<std::string> tags = parent;
                tags.push_back(item.label);
                if (const std::string v = value_for(loaded, reference, tags, f); !v.empty()) {
                    item.snippet = snippet_text(item.label) + " ${1:" + snippet_text(v) + "}";
                } else if (const Hints h = hints_at(loaded, reference, tags); !h.one_of.empty()) {
                    // A choice of the symbols it may be.
                    item.snippet = snippet_text(item.label) + " ${1|";
                    for (size_t i = 0; i < h.one_of.size(); ++i)
                        item.snippet += (i ? "," : "") + snippet_text(h.one_of[i]);
                    item.snippet += "|}";
                }
                offer(std::move(item));
            };
            const baker::shape::Shape* s = governing(loaded.shapes, parent);
            if (s)
                for (const baker::shape::Field& f : s->fields)
                    with_value({std::string(f.name.str()),
                                Kind::Field,
                                type_name(f),
                                describe(*s, f),
                                {},
                                {}},
                               &f);
            // The game's tweaks here that no shape declares.
            for (const TweakInfo& t : reference.tweaks) {
                if (t.tags.size() != parent.size() + 1 ||
                    !std::equal(parent.begin(), parent.end(), t.tags.begin()))
                    continue;
                if (s && s->field(baker::Symbol(t.tags.back()))) continue;
                with_value({t.tags.back(),
                            Kind::Field,
                            t.type,
                            "the game's tweak `" + t.path + "`, code default `" + t.default_text +
                                "`" + (t.desc.empty() ? "" : "\n\n" + t.desc),
                            {},
                            {}},
                           nullptr);
            }
            return out;
        }
        return out;
    }
    // A value. In (requires ...) of a pack: the packs.
    if (!c.stack.empty() && c.stack.back().tag == "requires") {
        for (const Definition& p : loaded.packs)
            offer({p.name, Kind::Module, "pack",
                   "declared at " + where(baker::Symbol(p.file), p.line)});
        return out;
    }
    // A symbol field's value: the values the project gives the same field, most used first.
    if (read && c.index == 1 && !c.stack.empty() && c.stack.back().open == '(' &&
        !c.stack.back().tag.empty()) {
        std::vector<std::string> parent = c.data_path();
        if (parent.size() >= 2) {
            const Hints h = hints_at(loaded, reference, parent);
            parent.pop_back();
            int rank = 0;
            // The symbols it may be, in the order they are declared.
            for (const std::string& symbol : h.one_of) {
                char sort[16];
                std::snprintf(sort, sizeof sort, "0%05d", rank++);
                offer({symbol,
                       Kind::EnumMember,
                       h.field ? "one of shape `" + std::string(h.shape->name.str()) + "`'s"
                               : "one of the listed symbols",
                       {},
                       {},
                       sort});
            }
            if (!h.one_of.empty()) return out;  // a value outside the list is an error
            for (const auto& [symbol, uses] :
                 symbols_in_use(loaded, parent, c.stack.back().tag, file, here, offset, read)) {
                char sort[16];
                std::snprintf(sort, sizeof sort, "0%05d", rank++);
                offer({symbol,
                       Kind::EnumMember,
                       std::to_string(uses) + (uses == 1 ? " use" : " uses") + " in the project",
                       {},
                       {},
                       sort});
            }
        }
    }
    for (const Definition& d : loaded.macros)
        offer({d.name,
               Kind::Constant,
               "macro",
               "defined at " + where(baker::Symbol(d.file), d.line),
               {},
               "1" + d.name});
    return out;
}

}  // namespace baker_lsp
