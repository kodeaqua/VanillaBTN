"""Check the version rules against an isolated Git history."""

import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.version import version


class VersionTest(unittest.TestCase):
    def test_patch_minor_and_developer_major(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)

            def git(*args):
                subprocess.run(["git", *args], cwd=repo, check=True, capture_output=True)

            git("init", "-q")
            git("config", "user.name", "Test")
            git("config", "user.email", "test@example.invalid")

            (repo / "VERSION_MAJOR").write_text("0\n")
            git("add", "VERSION_MAJOR")
            git("commit", "-qm", "chore: establish version baseline")
            self.assertEqual(version(repo), "0.0.0")

            (repo / "note.txt").write_text("first change\n")
            git("add", "note.txt")
            git("commit", "-qm", "docs: add note")
            self.assertEqual(version(repo), "0.0.1")

            (repo / "note.txt").write_text("feature complete\n")
            git("add", "note.txt")
            git(
                "commit", "-qm", "feat: finish larger feature",
                "-m", "Feature-Complete: true",
            )
            self.assertEqual(version(repo), "0.1.0")

            (repo / "note.txt").write_text("follow-up\n")
            git("add", "note.txt")
            git("commit", "-qm", "fix: correct follow-up")
            self.assertEqual(version(repo), "0.1.1")

            (repo / "VERSION_MAJOR").write_text("1\n")
            git("add", "VERSION_MAJOR")
            git("commit", "-qm", "chore: start developer-approved major version")
            self.assertEqual(version(repo), "1.0.0")


if __name__ == "__main__":
    unittest.main()
