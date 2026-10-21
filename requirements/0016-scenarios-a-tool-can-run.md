# 0016: Scenarios and objectives a tool can find and run

## The need

A tool should be able to find a game's scenarios and objectives, run one scenario, and
read the result, without knowing the game:

- **The project file names how to run the game's tools:** the binary that answers the tool
  commands (`check`, `eval`, ...), relative to the project file like every other path, and
  the files or directories that hold scenarios and objectives. For example
  `(tool "build/bin/stacks_duel")` and `(scenarios "sim")` `(objectives "metrics")`.
  `read_project` returns them; a game ignores them.
- **Running scenarios is a tool command of the host kit**, as `check` and `eval` are: by
  name, with seeds, the game supplying how to set up and step itself. Its result is a Baker
  JSON schema, versioned as `check`'s is: each scenario run with its seed, steps, pass or
  fail, and errors; each objective's value, verdict and error; and the status the exit code
  means.
- **The result is located:** each scenario and each objective says the file and line it is
  declared at, and an error during a run says where it happened, as `check`'s problems do.

## Why

Scenarios with objectives are a game's tests: "the easy CPU wins about half the time
against a newcomer", "no combo does more than 35% of a fighter's health". Tools want to run
them as tests:

- an editor's test view lists each scenario, runs it, and marks the objective that failed,
  on the line that declares it;
- CI runs them on every change to the data and reports which objective broke;
- an agent tuning data runs the scenarios its change affects before handing the change to
  a person.

Today each game decides where scenarios live, how they are run, and what the result looks
like. A tool can only support one game's convention, and must be told where its binary is.

## Constraints

- No cost to a game that does not use scenarios: the command, and the files, are optional.
- A run is deterministic for a seed, so a tool can rerun a failure.
- The game owns stepping and measuring; Baker owns finding, running by name, judging an
  objective's pass command, and the result's format.
- Paths in the project file are build outputs and data files; an absent binary is not an
  error for `read_project`, and a tool reports a missing one in its own terms.

## Today

Met by Baker 0.3.0: `(tool ...)`, `(scenarios ...)` and `(objectives ...)` in the project
file, the host kit's `sim`, and `baker.sim/1`, located. The editor's test view runs Duel's
eight scenarios with it and shows a failed objective on its declaration and its scenario.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
