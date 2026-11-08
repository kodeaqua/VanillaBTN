#!/usr/bin/env python3
"""Print VanillaBTN's development version from its Git history."""

import re
import subprocess
from pathlib import Path


def git(repo: Path, *args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=repo, check=True, capture_output=True, text=True
    ).stdout.strip()


def version(repo: Path) -> str:
    # A developer changes VERSION_MAJOR in a dedicated commit to start a new
    # major line. That commit is the 0.0 baseline for this major version.
    anchor = git(repo, "log", "-1", "--format=%H", "HEAD", "--", "VERSION_MAJOR")
    if not anchor:
        raise ValueError("VERSION_MAJOR has no committed baseline")
    major = git(repo, "show", f"{anchor}:VERSION_MAJOR")
    if not re.fullmatch(r"0|[1-9][0-9]*", major):
        raise ValueError("VERSION_MAJOR must contain one non-negative integer")

    minor = patch = 0
    commits = git(
        repo, "rev-list", "--reverse", "--topo-order", "--no-merges", f"{anchor}..HEAD"
    )
    for commit in commits.splitlines():
        message = git(repo, "show", "-s", "--format=%B", commit)
        if message.splitlines()[-1].strip() == "Feature-Complete: true":
            minor += 1
            patch = 0
        else:
            patch += 1
    return f"{major}.{minor}.{patch}"


if __name__ == "__main__":
    print(version(Path(__file__).resolve().parent.parent))
