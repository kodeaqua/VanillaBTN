"""Exercise the disc importer with a tiny synthetic ISO, never game data."""

import os
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.psx_disc import SECTOR, USER, extract

ROOT = Path(__file__).resolve().parents[2]


def record(name: bytes, lba: int, size: int, flags: int = 0) -> bytes:
    """The ISO directory fields used by psx_disc.extract."""
    entry = bytearray(33 + len(name) + (len(name) % 2 == 0))
    entry[0] = len(entry)
    struct.pack_into("<I", entry, 2, lba)
    struct.pack_into("<I", entry, 10, size)
    entry[25] = flags
    struct.pack_into("<H", entry, 28, 1)
    entry[32] = len(name)
    entry[33:33 + len(name)] = name
    return bytes(entry)


def fixture(path: Path) -> None:
    """One root directory, an empty archive, its 33 offsets, and an EXE name."""
    pvd = bytearray(USER)
    pvd[:6] = b"\x01CD001"
    pvd[156:190] = record(b"\x00", 17, USER, 2)
    directory = (record(b"A_FILE.BIN;1", 18, 0)
                 + record(b"A_FILE.HDT;1", 18, 33 * 4)
                 + record(b"SLUS_011.15;1", 19, 8))
    sectors = [bytes(USER) for _ in range(20)]
    sectors[16] = bytes(pvd)
    sectors[17] = directory.ljust(USER, b"\0")
    sectors[18] = bytes(33 * 4).ljust(USER, b"\0")
    sectors[19] = b"PS-X EXE".ljust(USER, b"\0")
    path.write_bytes(b"".join(bytes(24) + data + bytes(SECTOR - 24 - USER)
                              for data in sectors))


class ImportGameTest(unittest.TestCase):
    def test_extract_synthetic_disc(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            disc = root / "synthetic.bin"
            fixture(disc)
            extract(disc, root / "extracted")
            self.assertEqual((root / "extracted" / "A_FILE.HDT").read_bytes(), bytes(132))
            self.assertEqual((root / "extracted" / "SLUS_011.15").read_bytes(), b"PS-X EXE")

    @unittest.skipUnless(os.name == "nt", "PowerShell importer runs on Windows")
    def test_windows_import_with_spaced_path(self):
        with tempfile.TemporaryDirectory(prefix="VanillaBTN import ") as temp:
            root = Path(temp)
            disc = root / "synthetic disc.bin"
            fixture(disc)
            result = subprocess.run(
                ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(ROOT / "tools" / "import-game.ps1"), "-Disc", str(disc),
                 "-Out", str(root / "imported data")],
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0)
            self.assertEqual((root / "imported data" / "members" / "00_gf.bin").read_bytes(), b"")


if __name__ == "__main__":
    unittest.main()
