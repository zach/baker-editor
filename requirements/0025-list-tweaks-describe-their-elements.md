# 0025: A list tweak describes its elements

## The need

A tweak whose value is a list should say, in the reference, what each element is, so that a
tool can make the tweak from its description (`DescribedTweaks`) and `check_tweaks` holds
the data to it as the game's own `check` does:

- **A variable-length list** (`Convert<std::vector<T>>`): its element type, by name, as the
  reference names any type (`float32`, `symbol`, or the game's own `vec2`). The count comes
  from the data; each element is held to the type.
- **A fixed-size list of the game's own type** (`Convert<std::array<T, N>>` with a T of
  several values, such as `std::array<vec2, 4>`): N, and the element type. Note 0019 gave a
  fixed-size list of one-value elements its flat `values` (N of T's); a T of several values
  nests in each element, `((0 0) (1 0) (1 1) (0 1))`, so it cannot be listed flat, and the
  reference writes `values: null` for it today.

For example, as fields beside `type` in `baker.tweaks/1.x`:
`"type": "list", "element": "float32"` and
`"type": "fixed-size list", "element": "vec2", "count": 4`; the element type's own
values come from where the reference already describes the game's types. A
`TweakDescription` takes the same two fields, and `DescribedTweaks::add` makes a tweak that
reads and checks each element by them.

## Why

A list is a common tweak: a wave's spawn counts, a palette, a patrol's points, a set of tags.
A tool that checks data without running the game (an editor, a CI step, an agent's
pre-flight check) cannot hold such a value to its type today, because the reference does not
say what the elements are: an element of the wrong type, `(spawns 3 four 5)`, or a point
given one number, passes the tool and fails in the game.

## Constraints

- Compile-time, from the converter's template, at no cost to a game.
- A list of a T that describes nothing (no name a tool knows) stays as today: the tweak is
  not made, and a tool knows to leave it out.
- A list's range and `one_of` metadata, if a game gives them, apply to each element.
- A reader of the current schema keeps reading it: the fields are added.

## Today

None of Baker's stacks has such a tweak yet (their two fixed-size lists are of `float32`,
which note 0019 covers). The editor's server leaves out each tweak it cannot make, checks only
the range written beside its value, and does not take its key for a stray; the README lists it
as a limit, and plan E's step E2 waits on this note.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
