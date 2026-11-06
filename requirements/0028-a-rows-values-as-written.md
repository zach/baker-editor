# 0028: A layer's effect gives each row's values as a list, and the list a verb changes

## The need

What a layer changes, row by row (`layer_effect`, and with it a layer check's effect,
`explain` and `diff`), gives each value so that a tool can tell how many values there are,
and gives a verb's list below as the list the verb changes:

- **Values, not one packed node.** Today a row's value, its value below and a verb's operands
  each come as one node: one value is itself, several are an array. So `(aim (0 0))`, one value
  that is an array, and `(aim 0 0)`, two values, come back as the same node, `(0 0)`; and
  `(moves #append (jab2 (startup 3)))`, one operand, comes back as an array of what reads as
  two values, `jab2` and `(startup 3)`. Each entry should give its values as a list of nodes
  (or say whether the node is packed), the same for the value, the value below, and the
  operands. The text form a tool prints may stay as it is.
- **The list below a verb is the list it changes.** For a list whose elements are records,
  `(moves (jab ...) (heavy ...))` below `(moves #append (jab2 ...))`, the entry's `below` is an
  empty list and has no source, though the verb appends to `jab ... heavy` and the merged list
  is `jab ... heavy jab2`. `below` should be the list the verb applies to, with the source of
  the row that wrote it, as it is for a list of numbers or symbols.

## Why

A verb row and a pin are worth showing with the list they change: an editor's hint on a row,
`(jab heavy) #append jab2 -> (jab heavy jab2)`; a review of an agent's layer that says what a
row adds; a tool that writes a pin, `(@was ...)`, with the value below; a tool that restates
a verb as the list it makes, or writes a list that extends the one below as an `#append`. Each
needs the values one by one, and the list below as the verb sees it. With one packed node, a
tool that writes a pin for a one-tuple value writes the tuple's parts, and one that counts a
row's operands counts a record's fields; with an empty `below` for a list of records, the
most common list a game appends to (moves, items, waves) shows nothing below it.

## Constraints

- What exists stays: the packed node may stay beside the list for a tool that prints it.
- No extra work for a layer with no verbs: the lists are the nodes the entry is made from.
- The same for every place a row's value is reported: `layer_effect`, a layer check's
  effect, `explain`, `diff`, and the console's replies that carry them.

## Today

This integration reads a row's values from its layer's own tree, walking the entry's path
there, and skips the verb that comes first; for a list of records below an `#append`, it
takes the list below to be the merged list without the operands the row appends, since an
`#append` adds after the values below. It cannot give that list's source, and a `#drop` on a
list of records shows nothing below it. A value below that is one array is still read as its
parts when a pin is written; a check of the project after the edit refuses the pin then.

Met by Baker 0.8.0: `RowValues`, a row's values one node each, in `LayerEffectEntry::values`
and `below`, `ExplainEntry::values` and `DiffEntry::a` and `b`; under a verb, `below` is the
list it changes, records included, with `below_source`. The editor reads them as they come,
and its reading of a layer's own tree and its list below an `#append` are gone.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
