#include "highlight.hpp"

#include <algorithm>
#include <set>

namespace baker_lsp {

std::vector<SemanticToken> semantic_tokens(
    const FileRead& read, const Loaded* loaded, const Reference& reference,
    const std::vector<std::pair<int, std::string>>& shadowed) {
    const Structure& s = read.structure;
    std::set<std::string_view> macros, functions, game, objects;
    if (loaded) {
        for (const Definition& d : loaded->macros) macros.insert(d.name);
        for (const Definition& d : loaded->functions) functions.insert(d.name);
    }
    for (const FunctionInfo& f : reference.all) game.insert(f.name);
    for (const std::string& o : reference.objects) objects.insert(o);
    std::set<std::string_view> tweak_names;  // each tweak path's last part, to look up only those
    for (const TweakInfo& t : reference.tweaks)
        if (!t.tags.empty()) tweak_names.insert(t.tags.back());
    const std::set<std::pair<int, std::string>> faded(shadowed.begin(), shadowed.end());

    std::vector<SemanticToken> out;
    const auto add = [&out](const Token& t, TokenType type, uint32_t modifiers = 0) {
        out.push_back({t.begin, t.end, type, modifiers});
    };
    for (const Token& t : s.tokens) {
        switch (t.kind) {
            case TokenKind::Directive: add(t, TokenType::Keyword); continue;
            case TokenKind::Variable: add(t, TokenType::Variable); continue;
            case TokenKind::Macro: add(t, TokenType::Macro); continue;
            case TokenKind::Symbol: break;
            default: continue;
        }
        const Bracket* br = t.bracket >= 0 ? &s.brackets[static_cast<size_t>(t.bracket)] : nullptr;
        if (br && br->open == '{') {
            const std::string_view head = s.tag(t.bracket);
            if (t.index == 0) {
                if (functions.count(t.text))
                    add(t, TokenType::Function);
                else if (objects.count(t.text))
                    add(t, TokenType::Variable, kDefaultLibrary);
                else if (game.count(t.text))
                    add(t, TokenType::Function, kDefaultLibrary);
                continue;
            }
            if (t.index == 1 && head == "func") {
                add(t, TokenType::Function, kDeclaration);
                continue;
            }
            if (t.index == 1 && objects.count(head)) {
                add(t, TokenType::Method, kDefaultLibrary);
                continue;
            }
        }
        if (br && br->open == '[' && br->parent < 0 && t.index == 0) {  // [NAME value ...]
            add(t, TokenType::Macro, kDeclaration);
            continue;
        }
        if (macros.count(t.text)) {
            add(t, TokenType::Macro);
            continue;
        }
        if (br && br->open == '(' && t.index == 0) {
            uint32_t modifiers = 0;
            if (tweak_names.count(t.text)) {
                const std::vector<std::string> path = s.data_path(t.bracket);
                if (!path.empty() && tweak_at(reference, path)) modifiers |= kTweak;
            }
            if (!faded.empty() &&
                faded.count({read.lines.position(t.begin).first + 1, std::string(t.text)}))
                modifiers |= kShadowed;
            if (modifiers) add(t, TokenType::Property, modifiers);
        }
    }
    return out;
}

}  // namespace baker_lsp
