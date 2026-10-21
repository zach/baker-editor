#include "tweaks.hpp"

#include <baker/parse.hpp>
#include <baker/std_interop.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>

namespace baker_lsp {

template <class T>
struct GameTweaks::HeldTweak : Held {
    HeldTweak(baker::Symbol path, T fallback, const baker::TweakMeta& meta,
              baker::TweakRegistry& registry)
        : tweak(path, std::move(fallback), meta, registry) {}
    baker::Tweak<T> tweak;
};

namespace {

// The code's default, as the reference writes it in Baker text: its first value.
std::optional<baker::Node> default_of(const std::string& text) {
    auto read = baker::read_string("(default " + text + ")", "the reference");
    if (!read || (*read)->size() == 0) return std::nullopt;
    const baker::Array* a = (**read)[0].try_array();
    if (!a || a->size() < 2) return std::nullopt;
    return (*a)[1];
}

}  // namespace

GameTweaks::GameTweaks(const std::vector<TweakInfo>& tweaks) {
    for (const TweakInfo& t : tweaks) {
        const std::optional<baker::Node> d = default_of(t.default_text);
        baker::TweakMeta meta;
        if (t.ranged) meta.range = {t.min, t.max};
        meta.step = t.step;
        meta.desc = strings_.emplace_back(t.desc);
        meta.kind = strings_.emplace_back(t.kind);
        meta.dev_only = t.dev_only;
        if (!t.one_of.empty()) {
            std::vector<std::string_view>& list = lists_.emplace_back();
            for (const std::string& s : t.one_of) list.push_back(strings_.emplace_back(s));
            meta.one_of = list;
        }
        const baker::Symbol path(t.path);
        const auto number = [&]() { return d ? d->try_float().value_or(0) : 0.0; };
        const auto integer = [&]() { return d ? d->try_int().value_or(0) : int64_t{0}; };
        std::unique_ptr<Held> made;
        const std::string& type = t.type;
        if (type == "float32")
            made = std::make_unique<HeldTweak<float>>(path, static_cast<float>(number()), meta,
                                                      registry_);
        else if (type == "float")
            made = std::make_unique<HeldTweak<double>>(path, number(), meta, registry_);
        else if (type == "int8")
            made = std::make_unique<HeldTweak<int8_t>>(path, static_cast<int8_t>(integer()), meta,
                                                       registry_);
        else if (type == "uint8")
            made = std::make_unique<HeldTweak<uint8_t>>(path, static_cast<uint8_t>(integer()), meta,
                                                        registry_);
        else if (type == "int16")
            made = std::make_unique<HeldTweak<int16_t>>(path, static_cast<int16_t>(integer()), meta,
                                                        registry_);
        else if (type == "uint16")
            made = std::make_unique<HeldTweak<uint16_t>>(path, static_cast<uint16_t>(integer()),
                                                         meta, registry_);
        else if (type == "int32")
            made = std::make_unique<HeldTweak<int32_t>>(path, static_cast<int32_t>(integer()), meta,
                                                        registry_);
        else if (type == "uint32")
            made = std::make_unique<HeldTweak<uint32_t>>(path, static_cast<uint32_t>(integer()),
                                                         meta, registry_);
        else if (type == "int")
            made = std::make_unique<HeldTweak<int64_t>>(path, integer(), meta, registry_);
        else if (type == "uint64")
            made = std::make_unique<HeldTweak<uint64_t>>(path, static_cast<uint64_t>(integer()),
                                                         meta, registry_);
        else if (type == "bool")
            made = std::make_unique<HeldTweak<bool>>(path, integer() != 0, meta, registry_);
        else if (type == "symbol")
            made = std::make_unique<HeldTweak<baker::Symbol>>(
                path, d && d->try_sym() ? *d->try_sym() : baker::Symbol(), meta, registry_);
        else if (type == "string")
            made = std::make_unique<HeldTweak<std::string>>(
                path, d && d->try_str() ? std::string(*d->try_str()) : std::string(), meta,
                registry_);
        if (made) {
            tweaks_.push_back(std::move(made));
            checked_.push_back(t.path);
        } else {
            left_out_.push_back(t.path + " (" + type + ")");
        }
    }
    std::sort(checked_.begin(), checked_.end());
}

GameTweaks::~GameTweaks() {
    tweaks_.clear();  // each leaves the registry before it goes
}

bool GameTweaks::checks(const std::string& path) const {
    return std::binary_search(checked_.begin(), checked_.end(), path);
}

std::vector<Problem> GameTweaks::check(const Loaded& loaded) const {
    std::vector<Problem> out;
    if (!loaded.config || tweaks_.empty()) return out;
    const baker::Ref<baker::Array> root = loaded.config->root();
    if (!root) return out;
    const baker::TweakFindings found = baker::check_tweaks(*root, registry_);
    const auto add = [&out](const baker::Error& e, Severity severity) {
        Problem p = problem_of(e, severity);
        // A tweak the data does not set has no place in the files to show.
        if (p.file.empty()) return;
        // Each message starts with what it is about: 'gravity' is ..., 'world.gravity' is ...
        if (p.message.starts_with('\''))
            if (const size_t end = p.message.find('\'', 1); end != std::string::npos) {
                const std::string name = p.message.substr(1, end - 1);
                p.key = name.substr(name.rfind('.') + 1);
            }
        out.push_back(std::move(p));
    };
    for (const baker::Error& e : found.errors) add(e, Severity::Error);
    for (const baker::Error& e : found.warnings) add(e, Severity::Warning);
    return out;
}

}  // namespace baker_lsp
