#include "stubs.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <optional>
#include <utility>

namespace baker_lsp {
namespace {

// A stand-in's call never runs: check only counts its parameters. Each is a Node, which
// takes any value, so argument types are not checked; only their number is.
template <class... P>
struct StandIn {
    baker::Node operator()(P...) const { return {}; }
};

template <size_t, class T>
using Same = T;

template <size_t... R, size_t... O>
baker::Expected<baker::FuncRef> add(baker::Context& ctx, baker::Symbol name, bool rest,
                                    std::index_sequence<R...>, std::index_sequence<O...>) {
    if (rest)
        return ctx.func(name, StandIn<Same<R, baker::Node>...,
                                      Same<O, std::optional<baker::Node>>..., baker::Args>{});
    return ctx.func(name,
                    StandIn<Same<R, baker::Node>..., Same<O, std::optional<baker::Node>>...>{});
}

constexpr size_t kMaxRequired = 8;
constexpr size_t kMaxOptional = 4;

using Adder = baker::Expected<baker::FuncRef> (*)(baker::Context&, baker::Symbol, bool);

template <size_t R, size_t O>
baker::Expected<baker::FuncRef> add_n(baker::Context& ctx, baker::Symbol name, bool rest) {
    return add(ctx, name, rest, std::make_index_sequence<R>{}, std::make_index_sequence<O>{});
}

template <size_t... I>
constexpr auto make_table(std::index_sequence<I...>) {
    return std::array<Adder, sizeof...(I)>{
        &add_n<I / (kMaxOptional + 1), I % (kMaxOptional + 1)>...};
}
constexpr auto kAdders =
    make_table(std::make_index_sequence<(kMaxRequired + 1) * (kMaxOptional + 1)>{});

baker::Status status_of(const std::string& s) {
    if (s == "stub") return baker::Status::Stub;
    if (s == "provisional") return baker::Status::Provisional;
    if (s == "deprecated") return baker::Status::Deprecated;
    return baker::Status::Normal;
}

}  // namespace

bool register_stubs(baker::Context& ctx, const std::string& path, StubCount& count,
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
        const baker::Symbol name(f.value("name", ""));
        if (name.str().empty() || ctx.has_func(name)) continue;
        const baker::Status status = status_of(f.value("status", "normal"));
        bool added = false;
        for (const nlohmann::json& o : f.value("overloads", nlohmann::json::array())) {
            if (!o.is_object()) continue;
            const auto min = static_cast<size_t>(o.value("min_args", 0));
            const bool rest = !o.contains("max_args") || o["max_args"].is_null();
            const size_t max = rest ? min : static_cast<size_t>(o.value("max_args", 0));
            size_t required = min, optional = max > min ? max - min : 0;
            bool any = rest;
            if (required > kMaxRequired || optional > kMaxOptional) {
                required = 0;  // more than a stand-in takes: accept any count
                optional = 0;
                any = true;
                ++count.approximate;
            }
            auto r = kAdders[required * (kMaxOptional + 1) + optional](ctx, name, any);
            if (!r) continue;  // the same count as an earlier overload of other types
            ++count.overloads;
            r->status(status);
            added = true;
        }
        if (added) ++count.functions;
    }
    return true;
}

}  // namespace baker_lsp
