import struct
import tempfile
import unittest
from pathlib import Path

from tools import afile


class ArchiveUnpackTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        self.archive = root / "archive.bin"
        self.header = root / "archive.hdt"
        self.out = root / "out"
        self.members = [afile.Member(i, f"{i}.dat", "original", f"{i}.dat") for i in range(3)]
        self.first = b"a" * afile.SECTOR
        self.last = b"z" * afile.SECTOR
        self.archive.write_bytes(self.first + self.last)

    def test_rejects_descending_offsets_without_writing_members(self):
        self.header.write_bytes(
            struct.pack("<4I", 0, 2 * afile.SECTOR, afile.SECTOR, 2 * afile.SECTOR)
        )
        with self.assertRaisesRegex(SystemExit, "offsets are out of order"):
            afile.unpack(self.members, self.archive, self.header, self.out)
        self.assertFalse(self.out.exists())

    def test_rejects_a_truncated_offset_without_writing_members(self):
        self.header.write_bytes(
            struct.pack("<4I", 0, afile.SECTOR, afile.SECTOR, 2 * afile.SECTOR)[:-1]
        )
        with self.assertRaisesRegex(SystemExit, "truncated offset"):
            afile.unpack(self.members, self.archive, self.header, self.out)
        self.assertFalse(self.out.exists())

    def test_preserves_empty_members(self):
        self.header.write_bytes(struct.pack("<4I", 0, afile.SECTOR, afile.SECTOR, 2 * afile.SECTOR))
        afile.unpack(self.members, self.archive, self.header, self.out)
        self.assertEqual((self.out / "0.dat").read_bytes(), self.first)
        self.assertEqual((self.out / "1.dat").read_bytes(), b"")
        self.assertEqual((self.out / "2.dat").read_bytes(), self.last)
