# 0023: Errors a tool can tell apart without reading their words

## The need

- **Declaring what is already known.** `Context::declare_type` and `declare_object` refuse
  a name that already exists, a base they do not know, and a method `declare` would refuse,
  all with `ErrorCode::Type`. A tool needs to tell "already there" from the rest: a code of
  its own (or `Refused`), or a query that says whether a type is known
  (`Context::has_type(name)`, as `has_func` does for functions).
- **Tweak findings with their subject as fields.** `check_tweaks`'s errors and warnings name
  the tweak and the key in their message (`'world.gravity' is 30, outside its tweak's range`;
  `'gravty' in 'world' is read by no tweak`); the error's `path` is set for some and not
  others. Each finding should carry the tweak's path, and for a stray key the key and its
  group, as fields (the error's path, or new ones), so a tool locates and groups them.

## Why

A tool acting on an error (an editor deciding what to underline, or whether a refusal is
expected; CI grouping findings; an agent fixing them) today matches the message's words,
which change with any rewording, silently. Baker's console replies already carry a code; the
C++ errors a tool gets directly are where the same is missing.

## Constraints

- Messages unchanged for people; the fields are added.
- No cost on a path without an error.

## Today

Met by Baker 0.5.0: `Context::has_type`, `ErrorCode::Refused` for a name already taken, and each
`check_tweaks` finding's path as its subject. The editor's server reads no error's words now:
it asks `has_type`, and locates and filters tweak findings by their path.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
