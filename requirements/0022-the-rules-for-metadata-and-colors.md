# 0022: Baker's rules for metadata and colors, for tools

## The need

Two rules Baker applies when it reads a tweak's metadata from the data should be callable on
any value array:

- **Which arrays are metadata.** Given an array such as `(width 2 (range 0 10) (desc "w"))`,
  its values (`2`) and its metadata (range, step, kind, desc, note, one_of), as Baker reads
  them for a tweak; and, for `(ball (kind ball) (pos 1 2))`, that `kind` there is data, not
  metadata, if that is Baker's rule.
- **What is a color.** The kind a value is reported as, with Baker's rule that "a 3- or
  4-element value ranged 0..1" with no kind is a color, applied to a value and its metadata
  rather than only to a registered tweak's `kind()`.

## Why

A tool presenting data (an editor's color swatches and hints, a tuning panel built from the
data alone, an agent describing a value) must decide both for every value it shows. Baker
decides them for tweaks, but a tool repeating the rules for the data's own values gets them
slightly differently, as the editor's server has (it treats an array as a value with
metadata when it has a value before its arrays and every array is a metadata tag; it
does not know whether that matches Baker's own reading).

## Constraints

- The same functions Baker uses for tweaks, so they cannot disagree.
- No allocation for an array with no metadata; no cost to a game that does not call them.

## Today

Met by Baker 0.5.0: `split_metadata` and `presented_kind`. The editor's server uses both in place
of its own rules; Baker's metadata is the trailing arrays only, which `(w (range 0 10))` now
counts as metadata where the server's rule had not.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
