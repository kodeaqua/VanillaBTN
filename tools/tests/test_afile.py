"""Check archive boundaries with synthetic data, without using game assets."""

import struct
import tempfile
import unittest
from pathlib import Path

from tools.afile import Member, SECTOR, pack, unpack


class AFileTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.members = [Member(0, "first.bin", "original", "first"),
                        Member(1, "second.bin", "built", "second")]
        self.original = self.root / "original"
        self.built = self.root / "built"
        self.original.mkdir()
        self.built.mkdir()
        (self.original / "first.bin").write_bytes(b"one")
        (self.built / "second.bin").write_bytes(b"two")
        self.bin_path = self.root / "A_FILE.BIN"
        self.hdt_path = self.root / "A_FILE.HDT"
        self.out = self.root / "unpacked"

    def test_pack_and_unpack(self):
        pack(self.members, self.original, self.built, self.bin_path, self.hdt_path)
        unpack(self.members, self.bin_path, self.hdt_path, self.out)
        self.assertEqual((self.out / "first.bin").read_bytes(), b"one" + bytes(SECTOR - 3))
        self.assertEqual((self.out / "second.bin").read_bytes(), b"two" + bytes(SECTOR - 3))

    def test_rejects_invalid_offsets_without_writing_files(self):
        self.bin_path.write_bytes(bytes(2 * SECTOR))
        for offsets in (b"bad", struct.pack("<3I", 0, 3 * SECTOR, 2 * SECTOR),
                        struct.pack("<3I", 0, 1, 2 * SECTOR)):
            with self.subTest(offsets=offsets), self.assertRaises(SystemExit):
                self.hdt_path.write_bytes(offsets)
                unpack(self.members, self.bin_path, self.hdt_path, self.out)
            self.assertFalse(self.out.exists())


if __name__ == "__main__":
    unittest.main()
