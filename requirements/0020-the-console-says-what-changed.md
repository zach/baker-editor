# 0020: The console says when the stack changed, and where it listens

## The need

- **A cheap "has anything changed" question.** A console command that answers with the
  Config's epoch (`Config::epoch()`, which since 0.2.0 moves on every reload, layer change and
  live edit): for example `{config.epoch}` giving an integer. A tool polls it, and asks for
  more only when it moved. Equally good: the epoch in every reply, so any request tells.
- **Optionally, the project file names the console's port for tools,** as it names the
  reference and the tool binary: `(console 4555)`. `read_project` returns it; a game may use
  it as its default and keep its own command line; a tool knows which port is which game's.

## Why

A tool showing the running game's values beside the data (an editor's inline values, a
tuning panel, a dashboard) needs to know when to look again. A person changes a value from
the game's own panel or console, or a reload lands, and nothing in the editor moved: the
tool cannot know but by asking for the values again, every time, for everything on screen.

With an epoch, the cost of keeping up is one small reply per poll, and the refresh happens
only when something changed.

The port: a workspace holding several games (Baker's stacks hold seven) runs them on
different ports, and each tool must be told, game by game, which port is which; the project
file is where tools already learn the rest of the setup.

## Constraints

- No cost when nothing asks: the epoch is already kept.
- Answered between frames like every console command; nothing pushed, so a tool that does
  not poll costs nothing.
- The port in the project file is optional, and a game's own command line wins.

## Today

Met by Baker 0.5.0: `{config.epoch}`, and `(console PORT)` in the project file
(`Project::console_port`). The editor's server polls the epoch where the game binds it, and
takes a project's port from its project file unless `baker.console.ports` names one; the
stacks name a port each (Duel 4556), so hover finds each game with no setting.

## Not included

This note contains no code, identifiers, structure or documentation from any other
product, and nothing confidential.
