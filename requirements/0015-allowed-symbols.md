# 0015: A field's allowed symbols

## The need

A shape field, or a tweak's metadata, should be able to list the symbols its value may be:
for example a move's `button` is one of `light heavy kick special`. `check` then reports a
value outside the list as it reports one outside a range, and the list is in what tools
read: `set.shapes()` and `baker.shapes/1` for a shape field, `baker.tweaks/1` for a tweak.

## Why

A symbol field is the data's enumeration: a button, a damage type, an AI behaviour, a
surface material. Today its allowed values live in prose (the field's `desc`) or only in
the code that switches on them, so:

- a typo (`(button lite)`) passes `check` and fails, or silently does nothing, in the game;
- an editor can offer only the values the data already uses, so the first use of a value
  is never offered, and a typo used twice is offered as if it were valid;
- an agent tuning the data must read the code to learn the choices.

## Constraints

- Declared once, beside the range and the rest of the field's metadata, and merged by layers
  as the rest of a shape is (a later layer may add a symbol).
- `check`'s cost unchanged for fields without a list; a list is checked by lookup, not by
  scanning text.
- The list's order kept as written, so a tool can offer the symbols in the author's order.

## Today

The editor's server completes a symbol field from the values the project's files already
give it, ranked by use: in Duel, `(button ` offers `kick heavy light special`. It cannot
flag a value outside the intended set, and it offers nothing for a field no file uses yet.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
