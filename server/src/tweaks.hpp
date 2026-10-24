#pragma once
// The game's tweaks, made again from its reference (baker.tweaks/1.2) with Baker's
// DescribedTweaks, so Baker's own check_tweaks holds the data to them as the game's `check`
// does: a value its tweak cannot take (the wrong type, outside its range or one_of) is an
// error, a key beside a group's tweaks that none reads is a warning.
//
// A tweak Baker cannot make from its description (a game type the reference gives no value
// types for, as one written before 1.2) is left to the server's range rule (hints.hpp).
#include "diagnose.hpp"
#include "reference.hpp"

#include <baker/tools/described.hpp>
#include <baker/tweak.hpp>

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
    // The tweaks Baker could not make, each with why.
    [[nodiscard]] const std::vector<std::string>& left_out() const { return left_out_; }

    // check_tweaks over the stack's merged tree.
    std::vector<Problem> check(const Loaded& loaded) const;

private:
    baker::TweakRegistry registry_;
    baker::tools::DescribedTweaks described_{registry_};
    std::vector<std::string> checked_, left_out_;
    std::vector<std::string> left_out_paths_;  // sorted: their keys are not strays
};

}  // namespace baker_lsp
