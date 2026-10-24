#include "reference.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <baker/tools/described.hpp>
#include <deque>
#include <fstream>
#include <string_view>

namespace baker_lsp {
namespace {

// The types Context::declare knows (docs/reference-json.md); the game's own are not.
bool baker_type(std::string_view t) {
    static constexpr std::string_view kTypes[] = {
        "int",    "int8",        "int16",     "int32",     "uint8",  "uint16", "uint32",
        "uint64", "float",       "float32",   "bool",      "symbol", "string", "array",
        "value",  "unevaluated", "$variable", "values...", "name"};
    return std::find(std::begin(kTypes), std::end(kTypes), t) != std::end(kTypes);
}

baker::Status status_of(const std::string& s) {
    if (s == "stub") return baker::Status::Stub;
    if (s == "provisional") return baker::Status::Provisional;
    if (s == "deprecated") return baker::Status::Deprecated;
    return baker::Status::Normal;
}

// One function's overloads as descriptions. `text` keeps the strings the views point into.
struct Described {
    std::deque<std::string> text;
    std::vector<std::vector<baker::ParamDescription>> params;
    std::deque<std::vector<std::string_view>> values;  // each parameter's value types
    std::vector<baker::OverloadDescription> overloads;
    bool widened = false;

    // `widen`: a type that is not Baker's own becomes "value", for a reference that does not
    // say what its values are.
    Described(const nlohmann::json& f, bool widen) {
        const nlohmann::json list = f.value("overloads", nlohmann::json::array());
        params.reserve(list.size());
        for (const nlohmann::json& o : list) {
            std::vector<baker::ParamDescription>& ps = params.emplace_back();
            for (const nlohmann::json& p : o.value("params", nlohmann::json::array())) {
                std::string type = p.value("type", "value");
                if (widen && !baker_type(type)) {
                    type = "value";
                    widened = true;
                }
                // Baker's type of each of a game type's values (1.2), so check holds a literal
                // argument to them.
                std::vector<std::string_view>& vs = values.emplace_back();
                for (const nlohmann::json& v : p.value("values", nlohmann::json::array()))
                    if (v.is_string()) vs.push_back(keep(v.get<std::string>()));
                ps.push_back({.type = keep(std::move(type)),
                              .kind = keep(p.value("kind", "value")),
                              .raw = p.value("raw", false),
                              .values = std::span<const std::string_view>(vs.data(), vs.size())});
            }
            overloads.push_back(
                {.params = std::span<const baker::ParamDescription>(ps.data(), ps.size()),
                 .result = keep(o.value("result", "value"))});
        }
    }
    std::string_view keep(std::string s) { return text.emplace_back(std::move(s)); }
};

// {name symbol [int] values...}: an optional parameter in brackets.
std::string signature(const std::string& name, const nlohmann::json& overload) {
    std::string s = "{" + name;
    for (const nlohmann::json& p : overload.value("params", nlohmann::json::array())) {
        const std::string type = p.value("type", "value");
        const std::string kind = p.value("kind", "value");
        s += kind == "optional" ? " [" + type + "]" : " " + type;
    }
    s += "}";
    const std::string result = overload.value("result", "value");
    if (result != "value" && result != "nothing") s += " -> " + result;
    return s;
}

}  // namespace

namespace {

// Declares the reference's types with their methods (declare_type), each after its bases:
// a type whose bases are not known yet waits for a later pass. A type the Context already
// has (one of Baker's own) is left as it is.
void declare_types(baker::Context& ctx, const nlohmann::json& doc, Reference& out) {
    const nlohmann::json types =
        doc.value("types", nlohmann::json::array());  // `waiting` points in
    std::vector<const nlohmann::json*> waiting;
    for (const nlohmann::json& t : types)
        if (t.is_object() && !t.value("name", "").empty()) waiting.push_back(&t);
    for (bool progress = true; progress && !waiting.empty();) {
        progress = false;
        for (auto it = waiting.begin(); it != waiting.end();) {
            const nlohmann::json& t = **it;
            const std::string name = t.value("name", "");
            std::vector<baker::Symbol> bases;
            bool ready = true;
            for (const nlohmann::json& b : t.value("bases", nlohmann::json::array()))
                if (b.is_string()) {
                    bases.emplace_back(b.get<std::string>());
                    const bool waits =
                        std::any_of(waiting.begin(), waiting.end(), [&b](const nlohmann::json* w) {
                            return w->value("name", "") == b.get<std::string>();
                        });
                    ready = ready && !waits;
                }
            if (!ready) {
                ++it;
                continue;
            }
            std::deque<Described> described;
            std::vector<baker::tools::MethodDescription> methods;
            for (const nlohmann::json& m : t.value("methods", nlohmann::json::array())) {
                if (!m.is_object() || m.value("name", "").empty()) continue;
                Described& d = described.emplace_back(m, false);
                methods.push_back({baker::Symbol(m.value("name", "")),
                                   std::span<const baker::OverloadDescription>(
                                       d.overloads.data(), d.overloads.size())});
            }
            // A type the Context already has is Baker's own, as fully known as a declared one.
            if (baker::tools::has_type(ctx, baker::Symbol(name))) {
                out.declared_types.push_back(name);
            } else if (auto r = baker::tools::declare_type(
                           ctx, baker::Symbol(name),
                           std::span<const baker::Symbol>(bases.data(), bases.size()),
                           std::span<const baker::tools::MethodDescription>(methods.data(),
                                                                            methods.size()))) {
                out.declared_types.push_back(name);
            } else {
                out.failed.push_back("type " + name + ": " + std::string(r.error().message()));
            }
            it = waiting.erase(it);
            progress = true;
        }
    }
    for (const nlohmann::json* t : waiting)
        out.failed.push_back("type " + t->value("name", "") + ": its bases are not declared");
}

void read_tweaks(const nlohmann::json& doc, Reference& out) {
    for (const nlohmann::json& t : doc.value("tweaks", nlohmann::json::array())) {
        if (!t.is_object() || t.value("path", "").empty()) continue;
        TweakInfo& info = out.tweaks.emplace_back();
        info.path = t.value("path", "");
        for (size_t start = 0; start <= info.path.size();) {
            size_t dot = info.path.find('.', start);
            if (dot == std::string::npos) dot = info.path.size();
            info.tags.push_back(info.path.substr(start, dot - start));
            start = dot + 1;
        }
        info.type = t.value("type", "");
        info.default_text = t.value("default", "");
        info.bound = t.value("bound", false);
        if (const nlohmann::json r = t.value("range", nlohmann::json());
            r.is_array() && r.size() == 2 && r[0].is_number() && r[1].is_number()) {
            info.min = r[0].get<double>();
            info.max = r[1].get<double>();
            info.ranged = info.min < info.max;
        }
        info.step = t.value("step", 0.0);
        info.desc = t.value("desc", "");
        info.kind = t.value("kind", "");
        info.group = t.value("group", "");
        info.dev_only = t.value("dev_only", false);
        for (const nlohmann::json& o : t.value("one_of", nlohmann::json::array()))
            if (o.is_string()) info.one_of.push_back(o.get<std::string>());
        if (const nlohmann::json v = t.value("values", nlohmann::json()); v.is_array())
            for (const nlohmann::json& x : v)
                if (x.is_string()) info.values.push_back(x.get<std::string>());
    }
}

}  // namespace

std::vector<const FunctionInfo*> methods_of(const Reference& r, const std::string& object) {
    std::vector<const FunctionInfo*> out;
    const auto type = r.object_types.find(object);
    if (type == r.object_types.end()) return out;
    std::vector<std::string> todo{type->second}, seen;
    while (!todo.empty()) {  // the type, then its bases, each once
        const std::string name = todo.front();
        todo.erase(todo.begin());
        if (std::find(seen.begin(), seen.end(), name) != seen.end()) continue;
        seen.push_back(name);
        for (const TypeInfo& t : r.types) {
            if (t.name != name) continue;
            for (const FunctionInfo& m : t.methods) {
                const bool shadowed = std::any_of(
                    out.begin(), out.end(), [&m](const auto* o) { return o->name == m.name; });
                if (!shadowed) out.push_back(&m);
            }
            todo.insert(todo.end(), t.bases.begin(), t.bases.end());
        }
    }
    return out;
}

const TweakInfo* tweak_at(const Reference& r, const std::vector<std::string>& tags) {
    for (const TweakInfo& t : r.tweaks)
        if (t.tags == tags) return &t;
    return nullptr;
}

bool load_reference(baker::Context& ctx, const std::string& path, Reference& out,
                    std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot read the reference " + path;
        return false;
    }
    nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
    // The stacks' `reference --json` wraps it: {reference, tweaks, shapes}.
    if (doc.is_object() && doc.contains("reference")) {
        read_tweaks(doc.value("tweaks", nlohmann::json::object()), out);
        doc = doc["reference"];
    }
    if (!doc.is_object() || doc.value("schema", "").rfind("baker.reference/1", 0) != 0 ||
        !doc.contains("functions") || !doc["functions"].is_array()) {
        error = path + " is not a baker.reference/1 document";
        return false;
    }
    for (const nlohmann::json& f : doc["functions"]) {
        if (!f.is_object() || f.value("name", "").empty()) continue;
        FunctionInfo& info = out.all.emplace_back();
        info.name = f.value("name", "");
        info.doc = f.value("doc", "");
        info.status = f.value("status", "normal");
        info.tier = f.value("tier", "native");
        info.file = f.value("file", "");
        info.line = f.value("line", 0);
        for (const nlohmann::json& o : f.value("overloads", nlohmann::json::array()))
            info.signatures.push_back(signature(info.name, o));
    }
    for (const nlohmann::json& f : doc["functions"]) {
        if (!f.is_object() || f.value("tier", "") != "native") continue;  // script: the data's
        const std::string name = f.value("name", "");
        if (name.empty() || ctx.has_func(baker::Symbol(name))) continue;
        Described d(f, false);
        auto r = ctx.declare(baker::Symbol(name), std::span<const baker::OverloadDescription>(
                                                      d.overloads.data(), d.overloads.size()));
        if (!r && r.error().code() == baker::ErrorCode::Type) {
            d = Described(f, true);  // a type of the game's own
            if (d.widened)
                r = ctx.declare(baker::Symbol(name), std::span<const baker::OverloadDescription>(
                                                         d.overloads.data(), d.overloads.size()));
        }
        if (!r) {
            out.failed.push_back(name + ": " + std::string(r.error().message()));
            continue;
        }
        r->status(status_of(f.value("status", "normal")));
        if (const std::string doc_text = f.value("doc", ""); !doc_text.empty()) r->doc(doc_text);
        ++out.functions;
        out.overloads += static_cast<int>(d.overloads.size());
        if (d.widened) ++out.widened;
    }
    for (const nlohmann::json& o : doc.value("objects", nlohmann::json::array()))
        if (o.is_object() && !o.value("name", "").empty()) {
            out.objects.push_back(o.value("name", ""));
            if (!o.value("type", "").empty()) out.object_types[o.value("name", "")] = o["type"];
        }
    for (const nlohmann::json& t : doc.value("types", nlohmann::json::array())) {
        if (!t.is_object() || t.value("name", "").empty()) continue;
        TypeInfo& type = out.types.emplace_back();
        type.name = t.value("name", "");
        for (const nlohmann::json& b : t.value("bases", nlohmann::json::array()))
            if (b.is_string()) type.bases.push_back(b.get<std::string>());
        for (const nlohmann::json& m : t.value("methods", nlohmann::json::array())) {
            if (!m.is_object() || m.value("name", "").empty()) continue;
            FunctionInfo& info = type.methods.emplace_back();
            info.name = m.value("name", "");
            info.doc = m.value("doc", "");
            info.status = m.value("status", "normal");
            info.tier = "native";
            for (const nlohmann::json& o : m.value("overloads", nlohmann::json::array()))
                info.signatures.push_back(signature(type.name + " " + info.name, o));
        }
    }
    declare_types(ctx, doc, out);
    for (const auto& [name, type] : out.object_types) {
        const baker::Symbol object(name);
        // Baker's own (its scheduler), or a function of the game's.
        if (ctx.has_func(object) || ctx.object(object).type() == baker::Type::Object) continue;
        if (auto r = baker::tools::declare_object(ctx, object, baker::Symbol(type)))
            ++out.declared_objects;
        else
            out.failed.push_back("object " + name + ": " + std::string(r.error().message()));
    }
    return true;
}

}  // namespace baker_lsp
