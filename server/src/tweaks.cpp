#include "tweaks.hpp"

#include <algorithm>

namespace baker_lsp {

GameTweaks::GameTweaks(const std::vector<TweakInfo>& tweaks) {
    for (const TweakInfo& t : tweaks) {
        // Views for the call: DescribedTweaks keeps copies.
        std::vector<std::string_view> values(t.values.begin(), t.values.end());
        std::vector<std::string_view> one_of(t.one_of.begin(), t.one_of.end());
        std::vector<std::string_view> element(t.element.values.begin(), t.element.values.end());
        baker::tools::TweakDescription d;
        d.path = t.path;
        d.type = t.type;
        d.values = values;
        d.count = value_count(t.count);
        d.element = {.type = t.element.type,
                     .values = element,
                     .count = value_count(t.element.count),
                     .length = t.length};
        d.default_text = t.default_text;
        if (t.ranged) d.range = {t.min, t.max};
        d.step = t.step;
        d.desc = t.desc;
        d.kind = t.kind;
        d.group = t.group;
        d.dev_only = t.dev_only;
        d.one_of = one_of;
        if (auto r = described_.add(d))
            checked_.push_back(t.path);
        else {
            left_out_.push_back(t.path + ": " + std::string(r.error().message()));
            left_out_paths_.push_back(t.path);
        }
    }
    std::sort(checked_.begin(), checked_.end());
    std::sort(left_out_paths_.begin(), left_out_paths_.end());
}

GameTweaks::~GameTweaks() = default;

bool GameTweaks::checks(const std::string& path) const {
    return std::binary_search(checked_.begin(), checked_.end(), path);
}

std::vector<Problem> GameTweaks::check(const Loaded& loaded) const {
    std::vector<Problem> out;
    if (!loaded.config || checked_.empty()) return out;
    const baker::Ref<baker::Array> root = loaded.config->root();
    if (!root) return out;
    const baker::TweakFindings found = baker::check_tweaks(*root, registry_);
    // Each finding's path is its subject: the tweak's, or a stray key's group.key.
    const auto add = [this, &out](const baker::Error& e, Severity severity) {
        Problem p = problem_of(e, severity);
        // A tweak the data does not set has no place in the files to show.
        if (p.file.empty()) return;
        const std::string subject(e.path());
        // A stray key that is a tweak this server could not make is no stray to the game.
        if (std::binary_search(left_out_paths_.begin(), left_out_paths_.end(), subject)) return;
        p.message = std::string(e.message());  // it names its subject already
        p.key = subject.substr(subject.rfind('.') + 1);
        out.push_back(std::move(p));
    };
    for (const baker::Error& e : found.errors) add(e, Severity::Error);
    for (const baker::Error& e : found.warnings) add(e, Severity::Warning);
    return out;
}

}  // namespace baker_lsp
