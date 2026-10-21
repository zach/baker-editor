# 0019: A fixed-size list describes its values

## The need

Baker's own converter for a fixed-size list (`Convert<std::array<T, N>>`, `std_interop.hpp`)
should declare its value types, N of T's, as a game's own type declares them in its
converter (`values[] = {"float32", "float32"}`). The reference then writes them
(`"values": ["float32", "float32", "float32"]` in `baker.tweaks/1.2`, and on a parameter in
`baker.reference/1.2`), and a tool can register the tweak with `DescribedTweaks` and declare
the parameter, so `check_tweaks` and `check` hold the data to it as the game does.

Optionally, a variable-length list (`Convert<std::vector<T>>`) could say its element type,
so a tool checks each value's type even though the count comes from the data. 0.4.0 leaves
list tweaks out, which may stay so.

## Why

A fixed-size list is a common tweak: a color as `std::array<float, 3>`, a position, a size.
Today the reference writes `"type": "fixed-size list", "values": null` for it, because the
converter declares no values, so a tool that checks without the game cannot make the tweak:

- in Baker's stacks, Cube's `cube.color` and `scene.clear` (`std::array<float, 3>`) are not
  checked by the editor's server, though the game's own `check` checks them: `(color 0.9
  0.5)` or `(color red 0.5 0.2)` passes in the editor and fails in the game;
- without the tweak, `check_tweaks` takes the list's key for a stray, `'color' in 'cube' is
  read by no tweak`, which the server now has to suppress by hand.

## Constraints

- The value types follow from the template: N of T's own (T's single value type, or T's
  values in turn for a T of the game's own), at compile time, with no cost to a game.
- The reference's `values` for such a tweak or parameter is then an array of N names; a
  reader of 1.2 already reads it.
- A T that declares no values leaves the list's `values` null, as today.

## Today

Met by Baker 0.5.0: `Convert<std::array<T, N>>` declares N of T's value type for Baker's own
single-value types. In Cube, `cube.color` is now made from its description and checked:
`(color 0.9 0.5)` is an error in the editor, as in the game.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
