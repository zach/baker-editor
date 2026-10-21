#pragma once
// The game's tweaks, made again from its reference (baker.tweaks/1.1) as Baker tweaks of the
// same types, so Baker's own check_tweaks holds the data to them as the game's `check` does:
// a value its tweak cannot take (the wrong type, outside its range or one_of) is an error, a
// tweak the data does not set or a key beside a group's tweaks that none reads is a warning.
//
// Baker's types only: a tweak of a type of the game's own (vec2) cannot be made without the
// game's code, and is left to the server's range rule (hints.hpp).
#include "diagnose.hpp"
#include "reference.hpp"

#include <baker/tweak.hpp>

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace baker_lsp {

class GameTweaks {
public:
    explicit GameTweaks(const std::vector<TweakInfo>& tweaks);
    ~GameTweaks();
    GameTweaks(const GameTweaks&) = delete;
    GameTweaks& operator=(const GameTweaks&) = delete;

    // Whether Baker checks the tweak at `path` (as declared: duel.buffer).
    [[nodiscard]] bool checks(const std::string& path) const;
    // The tweaks of a game's own type, left out.
    [[nodiscard]] const std::vector<std::string>& left_out() const { return left_out_; }

    // check_tweaks over the stack's merged tree.
    std::vector<Problem> check(const Loaded& loaded) const;

private:
    struct Held {
        virtual ~Held() = default;
    };
    template <class T>
    struct HeldTweak;

    baker::TweakRegistry registry_;
    std::deque<std::string> strings_;                  // the metadata's text, which the tweaks view
    std::deque<std::vector<std::string_view>> lists_;  // their one_of lists
    std::vector<std::unique_ptr<Held>> tweaks_;
    std::vector<std::string> checked_, left_out_;
};

}  // namespace baker_lsp
