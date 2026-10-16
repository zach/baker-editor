#include "reference.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <fstream>
#include <string_view>

namespace baker_lsp {
namespace {

// The types every Context knows (docs/reference-json.md); the game's own are not, here.
bool baker_type(std::string_view t) {
    static constexpr std::string_view kTypes[] = {
        "int",    "int8",   "int16", "int32",       "uint8",     "uint16",
        "uint32", "uint64", "float", "float32",     "bool",      "symbol",
        "string", "array",  "value", "unevaluated", "$variable", "values..."};
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
    std::vector<baker::OverloadDescription> overloads;
    bool widened = false;

    // `widen`: a type that is not Baker's own becomes "value".
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
                ps.push_back(
                    {keep(std::move(type)), keep(p.value("kind", "value")), p.value("raw", false)});
            }
            overloads.push_back({std::span<const baker::ParamDescription>(ps.data(), ps.size()),
                                 keep(o.value("result", "value"))});
        }
    }
    std::string_view keep(std::string s) { return text.emplace_back(std::move(s)); }
};

}  // namespace

bool load_reference(baker::Context& ctx, const std::string& path, Reference& out,
                    std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot read the reference " + path;
        return false;
    }
    nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
    // The stacks' `reference --json` wraps it: {reference, tweaks, shapes}.
    if (doc.is_object() && doc.contains("reference")) doc = doc["reference"];
    if (!doc.is_object() || doc.value("schema", "").rfind("baker.reference/1", 0) != 0 ||
        !doc.contains("functions") || !doc["functions"].is_array()) {
        error = path + " is not a baker.reference/1 document";
        return false;
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
        if (o.is_object() && !o.value("name", "").empty())
            out.objects.push_back(o.value("name", ""));
    return true;
}

}  // namespace baker_lsp
