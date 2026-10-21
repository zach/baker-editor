# 0017: Registering a tweak from its description

## The need

A tool should be able to register a tweak from its description, as the reference writes it
(`baker.tweaks/1.1`: path, type name, default as Baker text, range, step, kind, desc,
one_of, dev_only), for any type the description names, including a type of the game's own
(`vec2`, `color`), so that `check_tweaks` holds data to it as it holds data to the game's
own tweak. The tweak is never read by code; it exists to be checked against.

For a type of the game's own, what the check needs is the shape of its values, which the
reference could describe beside the type (how many values, of which of Baker's types), so a
described tweak converts data as the game's `Convert<T>` does.

## Why

A tool that checks data without the game (an editor's language server, a CI checker, an
agent's pre-flight check) has the game's reference but not its code. Since 0.3.0, `check`
holds data to the game's tweaks, which a tool can only repeat for tweaks it can make: with
`Tweak<T>` it can make one for each of Baker's own types, by switching on the type name, but
not for a type of the game's own. A `(aim 1)` where a `vec2` is wanted passes in the editor
and fails in the game's `check`.

The same need for functions was met by `Context::declare` (note 0003).

## Constraints

- A tool-time feature: nothing added to a game that does not use it, and nothing on the read
  path of a tweak in code.
- A description the registry cannot express (an unknown type with no shape given) is an
  error naming the tweak, not a silent fallback.
- Registering from a description should not need the tool to switch on Baker's type names
  itself: the same call for `int32`, `symbol` or `vec2`.

## Today

Met by Baker 0.4.0: `DescribedTweaks` registers a tweak from its description with one call
for every type, a game's own included when the reference (`baker.tweaks/1.2`) gives its
`values`. The editor's server makes the game's tweaks with it; a tweak Baker cannot describe
(a fixed-size list, such as Cube's `color`, whose `values` the reference leaves out) is left
out, and its key is not taken for a stray.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
