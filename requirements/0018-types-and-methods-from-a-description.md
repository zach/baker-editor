# 0018: Declaring the game's types and methods from their description

## The need

A tool should be able to declare, from the reference (`baker.reference/1.1`'s `types` and
`objects`), each of the game's types with its methods and their overloads, and each named
object with its type, so that `check` holds a call on an object to its type's methods as it
holds a call to a function to its declared overloads (`Context::declare`, note 0003):

- a call on a named object, `{scheduler delay_task 2 {...}}`, is checked against the type's
  methods (and its bases'): a method the type does not have is an error, and a method's
  argument count and literal argument types are checked as a function's are;
- a parameter of a type of the game's own (a `vec2`) is checked for the shape of its value,
  given how the reference describes the type's values (as note 0017 asks for a tweak of such
  a type), so `{player.aim 1}` where a `vec2` is wanted is an error.

## Why

A tool that checks data and script without the game (an editor's language server, a CI
checker, an agent's pre-flight check) has the reference but not the game's code. Today a
call on a named object is not checked at all (a tool can only accept every call on it, or
report every one as undefined), and a parameter of a game type takes any value. Typos in a
method name, or a wrong argument count, pass the tool and fail in the game.

## Constraints

- A tool-time feature: nothing added to a game that does not use it, and nothing on the
  per-call path of native methods.
- What the reference already writes is enough to declare from; anything new it needs (the
  shape of a type's values) is an added field, a minor version.
- A declared method is never run, or it fails when run, as the tool chooses, as a declared
  function does.
- A description `check` cannot express is an error naming the type or method, not a silent
  fallback.

## Today

Met by Baker 0.4.0: `Context::declare_type`, `Context::declare_object`, and
`ParamDescription::values` (`baker.reference/1.2`). The editor's server declares the game's
types and named objects; in Duel, `{scheduler delay_task}`, `{scheduler dely_task 2 ...}`
and `{scheduler cancel soon}` are each an error, as the game's `check` reports them.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
