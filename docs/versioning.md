# Versioning

VanillaBTN uses `MAJOR.MINOR.PATCH` for its development version. The initial
MAJOR value is `0`, chosen by the developer. Run `python tools/version.py`
from a Git checkout to print the version of the current commit. The version is
calculated from committed history, so no generated version file needs editing
on every task.

## Increment rules

- **MAJOR** is set only by the developer. After an explicit developer decision,
  change `VERSION_MAJOR` in a dedicated atomic commit. That commit starts the
  new `MAJOR.0.0` line.
- **MINOR** rises automatically when a larger feature is complete. The agent
  marks the final atomic commit of that feature with the last line
  `Feature-Complete: true` in its commit message. The version calculator then
  adds one to MINOR and resets PATCH to `0`. The feature still follows the PR
  review and merge rules in `AGENTS.md`.
- **PATCH** rises for each other atomic, non-merge commit after the latest
  MAJOR baseline. Merge commits do not count as task commits. A MINOR or MAJOR
  transition takes precedence and resets PATCH to `0`.

For example, `0.0.3` becomes `0.0.4` after a routine commit, then `0.1.0` on
a completed larger feature, then `0.1.1` on the next routine commit. Agents
decide when a larger feature is complete; only the developer chooses MAJOR.

This is a project-specific per-commit version policy. [SemVer 2.0.0](https://semver.org/)
normally reserves PATCH increments for compatible bug fixes, while VanillaBTN
also increments PATCH for documentation and other atomic commits. Do not treat
these development versions as a claim that every PATCH contains only a bug fix.

## Source ZIPs without Git metadata

A source snapshot without `.git` cannot supply a real patch or minor history.
`tools/package-release.ps1` therefore labels such packages
`<VERSION_MAJOR>.0.0+source`. This is an **unversioned source build**, not a
claim that it corresponds to a release tag. `tools/version.py` itself remains
strictly commit-derived and unchanged.
