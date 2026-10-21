#pragma once
// Everything Baker says about how a value is meant to be: its metadata written beside it in
// the data, `(width 2 (range 0 10) (kind meters))`; the shape field that governs it; and the
// game's tweak at its path (reference.hpp), whose code gives a default and metadata too.
// The data's metadata wins over the shape's, and both over the tweak's code, as Baker's own
// tweaks read them.
#include "diagnose.hpp"
#include "reference.hpp"

#include <baker/array.hpp>
#include <baker/shape.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace baker_lsp {

struct Hints {
    bool ranged = false;
    double min = 0, max = 0;
    double step = 0;
    std::string kind, desc, note;
    std::vector<std::string> one_of;  // the symbols the value may be, in order; empty: any
    const baker::shape::Shape* shape = nullptr;  // the shape whose field governs the value
    const baker::shape::Field* field = nullptr;
    const TweakInfo* tweak = nullptr;
    bool written = false;  // metadata is written beside the value in the data
};

// Whether `tag` names metadata: range, step, kind, desc, note, group or one_of (for text that
// has not been read; a read value is split by Baker, split_metadata).
bool is_meta_tag(std::string_view tag);

// Whether `a` is a value with metadata after it, as Baker reads a tweak's (split_metadata):
// (width 2 (range 0 10)), not (ball (kind ball) (pos 1 2)), whose `kind` is data.
bool has_meta(const baker::Array& a);

// The hints for the value at the data path `tags`, from the last check's stack.
Hints hints_at(const Loaded& loaded, const Reference& reference,
               const std::vector<std::string>& tags);

// Only the metadata written beside the value in the data, as the stack merges it.
Hints written_at(const Loaded& loaded, const std::vector<std::string>& tags);

// Whether Baker presents the value at `tags` as a color (presented_kind): `kind color`, or 3 or
// 4 numbers ranged 0 to 1 with no kind, by its metadata, else the shape's or tweak's in `h`.
bool is_color(const Loaded& loaded, const Hints& h, const std::vector<std::string>& tags);

// The merged array at `tags`, or null.
const baker::Array* array_at(const Loaded& loaded, const std::vector<std::string>& tags);

// Values outside their range where only the running game would enforce it: metadata in the
// data, and the game's tweaks. A shape's range is check's, and is not repeated here, nor is a
// tweak's that `baker_checks` says Baker's check_tweaks holds the data to (tweaks.hpp).
std::vector<Problem> range_problems(
    const Loaded& loaded, const Reference& reference,
    const std::function<bool(const std::string& tweak)>& baker_checks = {});

}  // namespace baker_lsp
