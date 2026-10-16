# 0008: Every type the reference writes can be declared

## The need

`Context::declare` should accept every parameter and result type that `write_reference_json`
writes for a Context's functions, at least for Baker's own functions. Today the reference
writes `name` for the path parameter of Baker's config tools (`config.get`, `config.explain`,
`config.diff`, ...), and `declare` refuses it as an unknown type, so a tool cannot declare
those functions from the reference as they are written.

## Why

The reference is the description a tool declares functions from, to check data without the
game. A round trip that fails for Baker's own functions means every such tool keeps its own
list of exceptions. Every one of the stacks binds the nine config tools that take `name`,
and an agent's data and scenarios call them.

## Constraints

- What `declare` accepts for `name` checks an argument the way the config tools read it (a
  symbol or a string), so `check` gives the same errors for the declared function as for the
  native one.
- The same holds for any type Baker's own functions use that the reference names: a test that
  declares every function of a Context with Baker's tools bound, from its own reference, and
  compares `check`'s errors, keeps it so.

## Today

Met by Baker at 31408f6 (D176): the config tools' `name` is declared and checked; the server's
fallback to `value` is left for the game's own types.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
