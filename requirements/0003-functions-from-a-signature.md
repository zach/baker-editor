# 0003: Registering a function from a description of its signature

## The need

A host should be able to register a function from a description of its overloads, as data
rather than as a C++ callable: for each overload, its parameters' types (as the reference
names them: `int`, `float`, `symbol`, `array`, ...), their kinds (value, raw, optional,
rest), and its result. `check` then treats calls to it exactly as calls to the native
function the description came from, counting arguments and checking literal arguments'
types. The function is never run, or it fails when run, as the host chooses.

The reference JSON already holds these descriptions; the missing piece is registering them.

## Why

A tool checking data without the game (an editor's language server, a CI job, a review bot)
has the game's reference but not its code. Today it can only stand in for a function with a
C++ callable whose parameter types are fixed at compile time, so it registers parameters of
any type and loses `check`'s type checks: `{game.spawn 3}` where a symbol is wanted passes in
the editor and fails in the game.

## Constraints

- A tool-time feature: no cost to a game that does not use it, and nothing added to the
  per-call path of native functions.
- The registered function shows in the reference and in `check`'s demand like any native
  function, with the status the description gives (a stub stays a stub).
- A description the Context cannot express (an unknown type name) is an error naming the
  function, not a silent fallback.

## Today

The server registers each overload as a stand-in taking that many `Node` parameters (up to 8
required and 4 optional), plus a rest parameter where the overload has one. Argument counts
are checked; types are not, and overloads that differ only in types collapse into one.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
