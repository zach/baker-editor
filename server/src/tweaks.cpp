#include "tweaks.hpp"

#include <algorithm>

namespace baker_lsp {

GameTweaks::GameTweaks(const std::vector<TweakInfo>& tweaks) {
    for (const TweakInfo& t : tweaks) {
        // Views for the call: DescribedTweaks keeps copies.
        std::vector<std::string_view> values(t.values.begin(), t.values.end());
        std::vector<std::string_view> one_of(t.one_of.begin(), t.one_of.end());
        baker::TweakDescription d;
        d.path = t.path;
        d.type = t.type;
        d.values = values;
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
    const auto add = [this, &out](const baker::Error& e, Severity severity) {
        Problem p = problem_of(e, severity);
        // A tweak the data does not set has no place in the files to show.
        if (p.file.empty()) return;
        // Each message starts with what it is about: 'gravity' is ..., 'world.gravity' is ...
        if (p.message.starts_with('\''))
            if (const size_t end = p.message.find('\'', 1); end != std::string::npos) {
                const std::string name = p.message.substr(1, end - 1);
                p.key = name.substr(name.rfind('.') + 1);
                // 'color' in 'cube' is read by no tweak: unless cube.color is a tweak this
                // server could not make, which the game's own check would know.
                const std::string in = "' in '";
                if (p.message.compare(end, in.size(), in) == 0)
                    if (const size_t g = p.message.find('\'', end + in.size());
                        g != std::string::npos) {
                        const std::string group =
                            p.message.substr(end + in.size(), g - end - in.size());
                        if (std::binary_search(left_out_paths_.begin(), left_out_paths_.end(),
                                               group + "." + name))
                            return;
                    }
            }
        out.push_back(std::move(p));
    };
    for (const baker::Error& e : found.errors) add(e, Severity::Error);
    for (const baker::Error& e : found.warnings) add(e, Severity::Warning);
    return out;
}

}  // namespace baker_lsp
