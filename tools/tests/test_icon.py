"""Validate the Windows icon, manifest, and resource files."""

import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent.parent


class WindowsResourceTest(unittest.TestCase):
    def test_ico_resolutions_and_mode(self):
        ico_path = REPO_ROOT / "native" / "res" / "vanillabtn.ico"
        self.assertTrue(ico_path.exists(), "vanillabtn.ico must exist")
        with Image.open(ico_path) as im:
            sizes = getattr(im, "info", {}).get("sizes", set())
            expected_sizes = {(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)}
            self.assertTrue(
                expected_sizes.issubset(sizes),
                f"Expected icon sizes {expected_sizes} but found {sizes}",
            )
            self.assertEqual(im.mode, "RGBA", "Icon should retain RGBA alpha channel")

    def test_manifest_xml_and_dpi_awareness(self):
        manifest_path = REPO_ROOT / "native" / "res" / "vanillabtn.manifest"
        self.assertTrue(manifest_path.exists(), "vanillabtn.manifest must exist")
        tree = ET.parse(manifest_path)
        root = tree.getroot()
        self.assertTrue(root.tag.endswith("assembly"))

        content = manifest_path.read_text(encoding="utf-8")
        self.assertIn("PerMonitorV2", content)
        self.assertIn("UTF-8", content)

    def test_rc_defines_icon_and_version(self):
        rc_path = REPO_ROOT / "native" / "res" / "vanillabtn.rc"
        self.assertTrue(rc_path.exists(), "vanillabtn.rc must exist")
        content = rc_path.read_text(encoding="utf-8")
        self.assertIn('1 ICON "vanillabtn.ico"', content)
        self.assertIn("VS_VERSION_INFO", content)
        self.assertIn("VanillaBTN", content)


if __name__ == "__main__":
    unittest.main()
