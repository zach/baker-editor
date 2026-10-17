# Working on baker-editor

This repository is Baker's editor support (plan B in baker-dev `dev/integration-plans.md` §3).
It uses Baker as a library user would, and baker-dev's `INTEGRATIONS.md` is the policy:

- **Use Baker through its public headers,** at the release `CMakeLists.txt` pins
  (`zach/baker`). A note Baker has built but not yet released waits for the next release. A workaround for a Baker limit lives here until Baker
  ships the feature.
- **A need goes to Baker as a requirement note,** in `requirements/`, from
  `requirements/0000-template.md`, in Baker's terms: what a host needs, why as a general use
  case, and the constraints. The owner takes the note's text to the Baker session.
- **Do not change baker-dev from this session.** Baker work happens in a session there.
- **Ask the owner each open question with AskUserQuestion,** one question per decision, with
  the recommended option first.

How the work is done:

- Format with `clang-format-18` (`.clang-format` is Baker's); CI checks it.
- Before each commit, build and run the tests (`ctest --test-dir build`), and once under
  ASan+UBSan (`-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"`).
- The test data in `server/tests/data/` is a small project of its own. Baker's stacks
  (baker-dev `stacks/`) are the acceptance corpus; `tools/lsp_client.py` runs the server on
  them.
- Record measurements in `README.md`, regressions and noise included.
