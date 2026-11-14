"""Validate the release packaging script and assembled structure."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent


class ReleasePackageTest(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "win32", "Windows PowerShell packaging requires Windows")
    def test_package_release_assembly(self):
        script = REPO_ROOT / "tools" / "package-release.ps1"
        self.assertTrue(script.exists(), "package-release.ps1 must exist")

        with tempfile.TemporaryDirectory() as temp_dir:
            cmd = [
                "powershell",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(script),
                "-Configuration",
                "Release",
                "-OutDir",
                temp_dir,
                "-SkipBuild",
                "-NoZip",
            ]
            res = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, f"package-release failed: {res.stderr}\n{res.stdout}")

            # Check that package directory exists
            package_dirs = list(Path(temp_dir).glob("VanillaBTN-v*-win64"))
            self.assertEqual(len(package_dirs), 1, "Expected 1 package directory")
            pkg = package_dirs[0]

            # Check contents
            exe = pkg / "vanillabtn.exe"
            readme = pkg / "README.txt"
            license_file = pkg / "LICENSE.txt"
            tools_dir = pkg / "tools"
            disc_dir = pkg / "disc"

            self.assertTrue(exe.is_file(), "vanillabtn.exe missing in package")
            self.assertTrue(readme.is_file(), "README.txt missing in package")
            self.assertTrue(license_file.is_file(), "LICENSE.txt missing in package")
            self.assertTrue((tools_dir / "import-game.ps1").is_file(), "import-game.ps1 missing in tools")
            self.assertTrue((tools_dir / "psx_disc.py").is_file(), "psx_disc.py missing in tools")
            self.assertTrue((tools_dir / "afile.py").is_file(), "afile.py missing in tools")
            self.assertTrue((disc_dir / "a_file.txt").is_file(), "a_file.txt missing in disc")

            readme_text = readme.read_text(encoding="utf-8")
            self.assertIn("VanillaBTN", readme_text)
            self.assertIn("QUICK START", readme_text)
            self.assertIn("DEFAULT CONTROLS", readme_text)


if __name__ == "__main__":
    unittest.main()
