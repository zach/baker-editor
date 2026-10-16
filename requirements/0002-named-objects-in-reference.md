# 0002: The reference lists the objects a Context names

## The need

`write_reference_json` should list the objects a host has added to its Context under a
name (`ctx.add(object, "clock")`): each one's name and its type, as the reference already
names types. With it, a tool reading the reference knows that `{clock thread_task ...}` is a
call on an object of type `Scheduler`, not a call to an undefined function named `clock`.

## Why

A tool that checks data without running the game (an editor's language server, a CI
checker, an agent's pre-flight check) learns the game's vocabulary from the reference. It
has the functions and the types, but not the named objects, so every call on one looks like
a typo. Knowing the object's type also lets the tool check the method's argument count from
the type's methods, which the reference already lists.

## Constraints

- Written by the same call that writes the rest of the reference, in a new field of
  `baker.reference/1` (an added field, so a minor version), sorted by name.
- Objects added and removed while the game runs come and go; the reference lists those
  present when it is written, which for a build step is after the game's setup.
- No cost outside writing the reference.

## Today

Met by Baker at 2b38a4d (D173 to D175); the server uses it and its workaround is gone.

## Not included

This note contains no code, identifiers, structure or documentation from any other product,
and nothing confidential.
