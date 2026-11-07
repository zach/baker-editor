#include "features.hpp"

#include "hints.hpp"
#include "positions.hpp"
#include "structure.hpp"
#include "words.hpp"

#include <baker/std_interop.hpp>
#include <baker/tools/layers.hpp>

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
            case Type::Curve: return "curve";
            case Type::Asset: return "(asset " + std::string(f.asset.str()) + ")";
            case Type::Key: {  // as the shape writes it: (key_of weapons)
                std::string table;
                for (const baker::Symbol part : f.table)
                    table += (table.empty() ? "" : " ") + std::string(part.str());
                return "(key_of " + table + ")";
            }
        }
        return "any";
    };
    if (f.type == Type::List) {  // with its count, as the shape writes it: (list float 1 4)
        std::string count;
        if (f.min_count == f.max_count)
            count = " " + std::to_string(f.min_count);
        else if (f.max_count != UINT32_MAX)
            count = " " + std::to_string(f.min_count) + " " + std::to_string(f.max_count);
        return "(list " + name(f.element) + count + ")";
    }
    return name(f.type);
}

// A tweak's type as hover shows it: a list's with its elements, `fixed-size list of 4 vec2`.
std::string tweak_type(const TweakInfo& t) {
    if (t.element.type.empty()) return t.type;
    return t.type + " of " + (t.length ? std::to_string(t.length) + " " : "") + t.element.type;
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

// A markdown link to `file`'s `line`, named as where() names it.
std::string link(baker::Symbol file, int line) {
    if (file.str().empty()) return where(file, line);
    return "[" + where(file, line) + "](" + uri_from_path(canonical_path(std::string(file.str()))) +
           (line > 0 ? "#L" + std::to_string(line) : "") + ")";
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
    s += "; shape `" + std::string(shape.name.str()) + "` (" + link(shape.file, shape.line) + ")";
    if (!f.desc.empty()) s += "\n\n" + std::string(f.desc);
    if (!f.note.empty()) s += "\n\n*" + std::string(f.note) + "*";
    return s;
}

const char* standing_name(baker::tools::Standing s) {
    switch (s) {
        case baker::tools::Standing::Wins: return "wins";
        case baker::tools::Standing::Shadowed: return "shadowed";
        case baker::tools::Standing::Removes: return "removes it";
        case baker::tools::Standing::Disabled: return "disabled layer";
        case baker::tools::Standing::Composes: return "composes";  // #append, #drop
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

// The path of the value `tags` names, or the value its metadata describes: up to the first
// metadata tag, so units/knight/health/@range is units/knight/health.
std::vector<std::string> value_tags(std::vector<std::string> tags) {
    const auto meta = std::find_if(tags.begin(), tags.end(), [](const std::string& t) {
        return t.size() > 1 && t.front() == '@';
    });
    tags.erase(meta, tags.end());
    return tags;
}

// Whether the cursor is in a #shape's form.
bool in_shape(const Cursor& c) {
    return !c.stack.empty() && c.stack.front().declared == "#shape";
}

// The shape the cursor's token names: in a #shape, its own name, a field's type or a list's
// element type, or (key_of TABLE)'s table, by the keyed shape at it; or explain's #shape.NAME.
const baker::shape::Shape* shape_named(const Cursor& c, const Loaded& loaded) {
    std::string_view t = c.token;
    if (t.substr(0, 7) == "#shape.") {
        t.remove_prefix(7);
        return loaded.shapes.find(baker::Symbol(t.substr(0, t.find('.'))));
    }
    if (!in_shape(c) || c.stack.back().open != '(' || t.empty()) return nullptr;
    const Frame& top = c.stack.back();
    if (top.tag == "key_of" && c.index == 1) {
        const std::string_view table = t.substr(t.rfind('.') + 1);  // items.weapons: weapons
        for (const baker::shape::Shape& s : loaded.shapes.shapes())
            if (s.keyed && !s.at.empty() && s.at.begin()[s.at.size() - 1].str() == table) return &s;
        return nullptr;
    }
    const bool named = c.stack.size() == 1 && c.index == 0;
    const bool type = (c.stack.size() == 2 && c.index == 1) || (top.tag == "list" && c.index == 1);
    return named || type ? loaded.shapes.find(baker::Symbol(t)) : nullptr;
}

// Hover's markdown for shape `s`: where it is declared, what it governs, its fields.
std::string shape_markdown(const baker::shape::Shape& s) {
    std::string m = "`#shape " + std::string(s.name.str()) + "` (" + link(s.file, s.line) + ")";
    std::string at;
    for (const baker::Symbol part : s.at) at += (at.empty() ? "" : " ") + std::string(part.str());
    if (s.keyed)
        m += ": every child of `" + at + "`";
    else
        m += ": every array tagged `" + std::string(s.name.str()) + "`" +
             (at.empty() ? "" : " under `" + at + "`");
    for (size_t i = 0; i < s.fields.size(); ++i)
        m += (i ? ", `" : "\n\nfields `") + std::string(s.fields[i].name.str()) + "`";
    return m;
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

    // A metadata or shape word: (@range 0 10).
    if (!c.stack.empty() && c.stack.back().open == '(' && c.index == 0 && c.token.size() > 1 &&
        c.token.front() == '@') {
        const Word* w = word_named(c.token);
        h.markdown = w ? word_markdown(*w)
                       : "`" + c.token + "` is not a word Baker reads: `check` warns of it";
        return h;
    }
    // A shape, by name: (key_of units), a field's type, #shape.fighter.
    if (const baker::shape::Shape* s = shape_named(c, loaded)) {
        h.markdown = shape_markdown(*s);
        return h;
    }
    // A type in a #shape's field: (health int), (list float 3), (key_of units).
    if (in_shape(c) && c.stack.back().open == '(' &&
        ((c.stack.size() == 2 && c.index == 1) || (c.stack.size() > 2 && c.index == 0)))
        for (const TypeWord& t : type_words())
            if (t.name == c.token) {
                h.markdown = std::string(t.doc);
                return h;
            }

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
    if (c.index == 0) {  // the tag of an array being named, or a segment of a row's key
        tags = c.named_path();
        if (tags.empty()) return std::nullopt;
    }
    if (tags.empty()) return std::nullopt;
    const std::string path = path_text(tags);
    if (const std::vector<std::string> value = value_tags(tags); !value.empty())
        h.path = path_text(value);
    h.markdown = "`" + path + "`";

    if (tags.size() >= 2) {
        const std::vector<std::string> parent(tags.begin(), tags.end() - 1);
        if (const baker::shape::Shape* s = governing(loaded.shapes, parent))
            if (const baker::shape::Field* f = s->field(baker::Symbol(tags.back())))
                h.markdown += "\n\n" + describe(*s, *f);
    }
    if (const baker::shape::Shape* s = governing(loaded.shapes, tags))
        h.markdown += "\n\ngoverned by `#shape " + std::string(s->name.str()) + "` (" +
                      link(s->file, s->line) + ")";

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
        std::string m = "the game's tweak `" + t->path + "`: `" + tweak_type(*t) +
                        "`, code default `" + t->default_text + "`";
        if (t->ranged) m += ", " + number(t->min) + " to " + number(t->max);
        if (t->step > 0) m += ", step " + number(t->step);
        if (!t->kind.empty()) m += ", " + t->kind;
        for (size_t i = 0; i < t->one_of.size(); ++i) m += (i ? " " : ", one of ") + t->one_of[i];
        if (t->dev_only) m += "; dev only: a shipping build keeps the default";
        m += t->bound ? "; the data sets it" : "; the data does not set it: the default stands";
        if (!t->desc.empty() && t->desc != written.desc) m += "\n\n" + t->desc;
        h.markdown += "\n\n" + m;
    }

    // A row that composes the list below it: the list, its verb, and what it makes.
    for (const LayerRow& r : loaded.rows) {
        if (r.path != h.path || !r.composes()) continue;
        h.markdown += "\n\n" + r.layer + " (" + where(baker::Symbol(r.file), r.line) + "): `" +
                      (r.below.empty() ? "" : r.below + " ") + r.op + " " + r.operands + "`";
        if (!r.below.empty() && !r.below_file.empty())
            h.markdown += ", the list from " + where(baker::Symbol(r.below_file), r.below_line);
        if (!r.result.empty()) h.markdown += " \u2192 `" + r.result + "`";
        if (r.stale)
            h.markdown +=
                "\n\n*its `(@was ...)` pin is not the list below: the layers below "
                "have moved since it was written*";
    }
    if (loaded.config)
        if (auto chain = baker::tools::explain(*loaded.config, baker::Path(path));
            chain && !chain->empty()) {
            h.markdown += "\n\n| layer | value | where | |\n|---|---|---|---|";
            for (const baker::tools::ExplainEntry& e : *chain)
                h.markdown +=
                    "\n| " + std::string(e.layer.str()) + " | `" + shown(e.values) + "` | " +
                    where(e.source.file, e.source.line) + " | " + standing_name(e.standing) +
                    (e.op.str().empty() ? "" : " `" + std::string(e.op.str()) + "`") + " |";
            h.markdown += "\n\n*from the files*";
        }
    return h;
}

std::string data_path_at(const FileRead& read, size_t offset) {
    const Cursor c = scan(read.structure, read.text, offset);
    if (c.quiet) return {};
    std::vector<std::string> tags = c.data_path();
    if (c.index == 0 && !c.token.empty()) {
        tags = c.named_path();
        if (tags.empty()) return {};
    }
    tags = value_tags(std::move(tags));
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
    if (const baker::shape::Shape* s = shape_named(c, loaded)) {  // its #shape
        if (!s->file.str().empty())
            out.push_back({canonical_path(std::string(s->file.str())), s->line});
        return out;
    }

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
        tags = c.named_path();
        if (tags.empty()) return out;
    }
    if (tags.empty() || !loaded.config) return out;
    // The line the cursor is on, to leave out the contributor it is.
    const int line =
        1 + static_cast<int>(std::count(text.begin(),
                                        text.begin() + static_cast<std::ptrdiff_t>(c.begin), '\n'));
    if (auto chain = baker::tools::explain(*loaded.config, baker::Path(path_text(tags))))
        for (const baker::tools::ExplainEntry& e : *chain) {
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
    if (h.tweak && !h.tweak->default_text.empty()) {
        // The reference writes a value of several parts in parentheses, (0 0) for a vec2 and
        // ((0 0) (1 1)) for a list of them; the data writes its parts, (aim 0 0).
        const std::string& d = h.tweak->default_text;
        if (d.size() >= 2 && d.front() == '(' && d.back() == ')') return d.substr(1, d.size() - 2);
        return d;
    }
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
            if (br.open != '(' || br.tag < 0 || s.name(static_cast<int>(b)) != tag) continue;
            if (f == file && br.begin < offset && offset <= br.end) continue;  // being typed
            const size_t first = static_cast<size_t>(br.tag) + 1;
            if (first >= s.tokens.size() || s.tokens[first].bracket != static_cast<int>(b) ||
                !symbol_like(s.tokens[first]))
                continue;
            // Where it is: its parent's path, or a row's key but its last segment.
            std::vector<std::string> where;
            if (s.is_row(static_cast<int>(b))) {
                where = s.data_path(static_cast<int>(b));
                if (!where.empty()) where.pop_back();
            } else if (br.parent >= 0) {
                where = s.data_path(br.parent);
            }
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

// A word as completion offers it: written whole, `(@range 0 1)`, where a value is typed, or
// after the `(` already typed.
Completion word_item(const Word& w, bool paren) {
    std::string snippet = std::string(w.name) + std::string(w.args);
    if (paren) snippet = "(" + snippet + ")";
    return {std::string(w.name),
            Kind::Keyword,
            "(" + std::string(w.name) + (w.takes.empty() ? "" : " ") + std::string(w.takes) + ")",
            std::string(w.doc),
            snippet,
            "~" + std::string(w.name)};  // after a shape's fields and the macros
}

// How many values each array tagged `field` holds, below `a` (at `path`), whose parent `shape`
// governs: by count, how many hold it.
void count_uses(const baker::Array& a, std::vector<std::string> path,
                const baker::shape::Shape& shape, baker::Symbol field,
                const baker::shape::Set& shapes, std::map<size_t, int>& uses) {
    const bool governed = !path.empty() && governing(shapes, path) == &shape;
    for (const baker::Node& n : a) {
        const baker::Array* child = n.try_array();
        if (!child || child->tag().empty()) continue;
        if (governed && child->tag() == field) {
            ++uses[child->values().size()];
            continue;
        }
        path.emplace_back(child->tag().str());
        count_uses(*child, path, shape, field, shapes, uses);
        path.pop_back();
    }
}

// Whether the cursor is in a project file's layer's condition, (project (layers (local
// "local.dta" (when (and PC |))))): in (when ...), or in a (not ...), (and ...) or (or ...) in it.
bool in_condition(const Cursor& c) {
    const auto& st = c.stack;
    if (st.size() < 4 || st[0].tag != "project" || st[1].tag != "layers" || st[3].tag != "when")
        return false;
    for (size_t i = 0; i < st.size(); ++i)
        if (st[i].open != '(' || (i > 3 && !st[i].tag.empty() && st[i].tag != "not" &&
                                  st[i].tag != "and" && st[i].tag != "or"))
            return false;
    return true;
}

// Completion in a layer's condition: the defines it may test, and not, and, or.
void complete_condition(const Cursor& c, const Loaded& loaded,
                        const std::function<void(Completion)>& offer) {
    const Frame& top = c.stack.back();
    const auto words = [&](bool paren) {
        for (const auto& [word, snippet] :
             {std::pair<const char*, const char*>{"not", "not ${1:NAME}"},
              {"and", "and ${1:A} ${2:B}"},
              {"or", "or ${1:A} ${2:B}"}})
            offer({word,
                   Kind::Keyword,
                   "condition",
                   {},
                   paren ? "(" + std::string(snippet) + ")" : std::string(snippet),
                   "~"});
    };
    if (c.index == 0) {  // (when (| : what the condition is
        if (c.stack.size() > 4) words(false);
        return;
    }
    // One condition in (when ...) and (not ...), any number in (and ...) and (or ...).
    if ((top.tag == "when" || top.tag == "not") && c.index != 1) return;
    for (const auto& [name, from] : loaded.defines)
        offer({name,
               Kind::Constant,
               from == "not defined" ? "not defined now" : "defined, by the " + from,
               {},
               {},
               from == "not defined" ? "1" + name : "0" + name});
    words(true);
}

// Completion in a #shape (NAME ...): the shape's words beside its fields, a field's type after
// its name and its words after the type, a rule's words, a list's element types, and the keyed
// tables in (key_of ...).
void complete_shape(const Cursor& c, const std::string& typed, const Loaded& loaded,
                    const std::function<void(Completion)>& offer) {
    const size_t depth = c.stack.size();
    const Frame& top = c.stack.back();
    const bool at = !typed.empty() && typed.front() == '@';
    const auto words = [&](Place place, bool paren) {
        for (const Word* w : words_at(place)) offer(word_item(*w, paren));
    };
    // The types, or a list's element types (no list of lists), and the shapes.
    const auto types = [&](bool element) {
        for (const TypeWord& t : type_words())
            if (!(element && t.name == "list"))
                offer({std::string(t.name), Kind::Keyword, "type", std::string(t.doc),
                       std::string(t.snippet)});
        for (const baker::shape::Shape& s : loaded.shapes.shapes())
            offer({std::string(s.name.str()), Kind::Struct, "shape",
                   "declared at " + where(s.file, s.line)});
    };
    if (top.open != '(') return;
    if (top.tag == "key_of" && c.index >= 1) {  // the keyed tables, by their last tag
        std::vector<std::string> tables;
        for (const baker::shape::Shape& s : loaded.shapes.shapes()) {
            if (!s.keyed || s.at.empty()) continue;
            const std::string table(s.at.begin()[s.at.size() - 1].str());
            if (table == "*" || std::find(tables.begin(), tables.end(), table) != tables.end())
                continue;
            tables.push_back(table);
            offer({table, Kind::Field, "keyed by shape `" + std::string(s.name.str()) + "`",
                   "declared at " + where(s.file, s.line)});
        }
        return;
    }
    if (top.tag == "list" && c.index == 1) return types(true);
    if (top.tag == "list" && c.index == 2 && depth == 3) {  // its count: what the stack holds
        const baker::shape::Shape* shape = loaded.shapes.find(baker::Symbol(c.stack[0].tag));
        std::map<size_t, int> uses;  // by count
        if (shape && loaded.config)
            if (const baker::Ref<baker::Array> root = loaded.config->root())
                count_uses(*root, {}, *shape, baker::Symbol(c.stack[1].tag), loaded.shapes, uses);
        std::vector<std::pair<size_t, int>> by_use(uses.begin(), uses.end());
        std::stable_sort(by_use.begin(), by_use.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        int rank = 0;
        for (const auto& [n, k] : by_use) {
            char sort[16];
            std::snprintf(sort, sizeof sort, "%05d", rank++);
            offer({std::to_string(n),
                   Kind::Constant,
                   "exactly " + std::to_string(n) + ": " + std::to_string(k) +
                       (k == 1 ? " list" : " lists") + " in the stack hold " + std::to_string(n),
                   {},
                   {},
                   sort});
        }
        if (uses.size() > 1) {
            const std::string range =
                std::to_string(uses.begin()->first) + " " + std::to_string(uses.rbegin()->first);
            offer({range,
                   Kind::Constant,
                   "from " + range.substr(0, range.find(' ')) + " to " +
                       range.substr(range.find(' ') + 1) + ": what the stack's lists hold",
                   {},
                   {},
                   "~"});
        }
        return;
    }
    if (depth == 1) {
        if (c.index >= 1) words(Place::Shape, true);
        return;
    }
    const Frame& parent = c.stack[depth - 2];
    if (c.index == 0) {  // naming a child: of the shape, of a field, of a rule
        if (!at && depth > 2 && parent.tag == "list" && parent.count == 2) {
            offer({"key_of", Kind::Keyword, "type", {}, "key_of ${1:table}"});
            offer({"asset", Kind::Keyword, "type", {}, "asset ${1:kind}"});
        } else if (depth == 2) {
            if (at) words(Place::Shape, false);
        } else if (depth == 3 && parent.tag == "@rule") {
            if (at) words(Place::Rule, false);
        } else if (depth == 3 && !parent.tag.empty() && parent.tag.front() != '@') {
            if (!at && parent.count == 2) {  // the type's place: (health (|
                offer({"list", Kind::Keyword, "type", {}, "list ${1:float}"});
                offer({"key_of", Kind::Keyword, "type", {}, "key_of ${1:table}"});
                offer({"asset", Kind::Keyword, "type", {}, "asset ${1:kind}"});
            } else if (at && parent.count >= 3) {
                words(Place::Field, false);
            }
        }
        return;
    }
    if (depth != 2) return;
    if (top.tag == "@rule") {
        if (c.index >= 3) words(Place::Rule, true);
    } else if (!top.tag.empty() && top.tag.front() != '@') {  // a field
        if (c.index == 1)
            types(false);
        else
            words(Place::Field, true);
    }
}

// Completion of a row's key being typed, (/fighters.kes|: the segment after the last
// separator, from the keys the merged tree has at the segments before it and the fields of
// the shape that governs them. False if the cursor is on no row's key.
bool complete_key(const Structure& s, std::string_view text, size_t offset, const Loaded& loaded,
                  Completions& out) {
    // The token the cursor ends or is in, found as scan() finds it.
    const auto at = std::lower_bound(s.tokens.begin(), s.tokens.end(), offset,
                                     [](const Token& t, size_t o) { return t.end < o; });
    if (at == s.tokens.end() || at->begin >= offset || at->index != 0 || at->bracket < 0 ||
        at->quote || s.brackets[static_cast<size_t>(at->bracket)].parent >= 0 ||
        s.brackets[static_cast<size_t>(at->bracket)].open != '(' || text[at->begin] != '/')
        return false;
    // The segments written before the cursor's, as Baker splits them.
    const std::string_view typed = text.substr(at->begin + 1, offset - at->begin - 1);
    std::vector<std::string> before;
    size_t from = 0;
    bool quoted = false;
    for (size_t i = 0; i < typed.size(); ++i) {
        if (typed[i] == '\'')
            quoted = !quoted;
        else if (!quoted && (typed[i] == '.' || typed[i] == '/')) {
            std::string_view seg = typed.substr(from, i - from);
            if (seg.size() >= 2 && seg.front() == '\'' && seg.back() == '\'')
                seg = seg.substr(1, seg.size() - 2);
            if (seg.empty()) return true;  // an empty segment: nothing names a key here
            before.emplace_back(seg);
            from = i + 1;
        }
    }
    out.begin = at->begin + 1 + from;
    out.end = offset;
    std::string_view partial = typed.substr(from);
    if (!partial.empty() && partial.front() == '\'') partial.remove_prefix(1);
    std::vector<std::string> seen;
    const auto offer = [&](std::string name, std::string detail, std::string doc) {
        if (name.compare(0, partial.size(), partial) != 0 ||
            std::find(seen.begin(), seen.end(), name) != seen.end())
            return;
        seen.push_back(name);
        std::string written = bare_segment(name) ? name : "'" + name + "'";
        out.items.push_back(
            {name, Kind::Field, std::move(detail), std::move(doc), snippet_text(written), {}});
    };
    // The keys the stack has there: a record's are followed by more, a value's end the key.
    const baker::Ref<baker::Array> root = loaded.config ? loaded.config->root() : nullptr;
    const baker::Array* here = before.empty() ? root.get() : array_at(loaded, before);
    if (here)
        for (const baker::Node& n : *here) {
            const baker::Array* child = n.try_array();
            if (!child || child->tag().str().empty() || child->tag().str().front() == '@') continue;
            std::string value;
            for (const baker::Node& v : child->values())
                value += (value.empty() ? "" : " ") + baker::to_string(v);
            offer(std::string(child->tag().str()),
                  value.size() > 40 ? value.substr(0, 40) + "…" : value, {});
        }
    if (!before.empty())
        if (const baker::shape::Shape* sh = governing(loaded.shapes, before))
            for (const baker::shape::Field& f : sh->fields)
                offer(std::string(f.name.str()), type_name(f), describe(*sh, f));
    return true;
}

}  // namespace

Completions complete(const FileRead& here, size_t offset, const Loaded& loaded,
                     const Reference& reference, const std::string& file, const ReadFile& read) {
    const std::string_view text = here.text;
    const Cursor c = scan(here.structure, text, offset);
    Completions out;
    if (c.quiet) return out;
    if (complete_key(here.structure, text, offset, loaded, out)) return out;
    out.begin = c.begin;
    out.end = offset;  // replace what is typed up to the cursor
    const std::string typed(text.substr(c.begin, offset - c.begin));
    const auto offer = [&](Completion item) {
        if (item.label.compare(0, typed.size(), typed) == 0) out.items.push_back(std::move(item));
    };
    if (in_shape(c)) {
        complete_shape(c, typed, loaded, offer);
        return out;
    }
    if (in_condition(c)) {
        complete_condition(c, loaded, offer);
        return out;
    }
    if (!c.stack.empty() && !c.stack.front().declared.empty() &&
        c.stack.back().tag != "requires")  // #pack's and #origin's forms
        return out;
    // Data's metadata: after a value, (health 100 |, or naming a child, (health 100 (@|.
    const auto metadata = [&](bool paren) {
        for (const Word* w : words_at(Place::Data)) offer(word_item(*w, paren));
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
            if (c.stack.size() >= 2 && c.stack[c.stack.size() - 2].count >= 3)
                metadata(false);  // after a value or field: (box (w 1) (|
            if (!typed.empty() && typed.front() == '@') return out;
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
    if (c.index >= 2 && !c.data_path().empty() && c.stack.back().tag.front() != '@') metadata(true);
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
