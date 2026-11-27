import errno
import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import psx_disc


class DiscImportTest(unittest.TestCase):
    def test_extract_checks_file_extent_before_writing(self):
        root = psx_disc.PVD_LBA + 1
        pvd = bytearray(psx_disc.USER)
        pvd[:6] = b"\x01CD001"
        struct.pack_into("<I", pvd, 158, root)
        struct.pack_into("<I", pvd, 166, psx_disc.USER)
        record = bytearray(34)
        record[0] = len(record)
        record[32:] = b"\x01A"
        struct.pack_into("<I", record, 2, root + 1)
        struct.pack_into("<I", record, 10, 1)
        image = psx_disc.Image()
        for _ in range(psx_disc.PVD_LBA):
            image.empty_form2_sector(psx_disc.SM_FORM2)
        image.form1_sector(pvd, psx_disc.DATA_LAST)
        image.form1_sector(record, psx_disc.DATA_LAST)
        with tempfile.TemporaryDirectory() as temp:
            disc = Path(temp) / "input.bin"
            out = Path(temp) / "extracted"
            disc.write_bytes(image.data)
            with self.assertRaisesRegex(SystemExit, "extent exceeds"):
                psx_disc.extract(disc, out)
            self.assertFalse((out / "A").exists())
            image.form1_sector(b"x", psx_disc.DATA_LAST)
            disc.write_bytes(image.data)
            psx_disc.extract(disc, out)
            self.assertEqual((out / "A").read_bytes(), b"x")

    def test_directory_record_must_fit_declared_directory_size(self):
        image = psx_disc.Image()
        image.form1_sector(bytes([34]) + bytes(31) + bytes([1, 65]), psx_disc.DATA_LAST)
        self.assertEqual(len(list(psx_disc._records(image.data, 0, 34))), 1)
        with self.assertRaisesRegex(SystemExit, "Invalid ISO directory record"):
            list(psx_disc._records(image.data, 0, 33))

    def test_prepare_cue_preserves_hardlinked_input(self):
        with tempfile.TemporaryDirectory() as temp:
            disc = Path(temp) / "input.bin"
            disc.write_bytes(b"synthetic")
            cue = Path(temp) / "output.cue"
            cue.hardlink_to(disc)
            with self.assertRaisesRegex(SystemExit, "separate file"):
                psx_disc.prepare_cue(disc, cue)
            self.assertEqual(disc.read_bytes(), b"synthetic")

    def test_cue_resolves_a_relative_track_with_spaces(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp).resolve()
            cue = folder / "input.cue"
            cue.write_text(
                'REM comment\nFILE "tracks\\data track.BIN" BINARY\n'
                '  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n',
                encoding="utf-8-sig",
            )
            self.assertEqual(psx_disc.cue_binary(cue), folder / "tracks" / "data track.BIN")

    def test_cue_rejects_paths_and_layouts_the_extractor_cannot_follow(self):
        with tempfile.TemporaryDirectory() as temp:
            cue = Path(temp) / "input.cue"
            for filename, mode, index in (
                ("../data.bin", "MODE2/2352", "00:00:00"),
                ("data.bin", "AUDIO", "00:00:00"),
                ("data.bin", "MODE2/2352", "00:02:00"),
            ):
                with self.subTest(filename=filename, mode=mode, index=index):
                    cue.write_text(
                        f'FILE "{filename}" BINARY\nTRACK 01 {mode}\nINDEX 01 {index}\n',
                        encoding="utf-8",
                    )
                    with self.assertRaises(SystemExit):
                        psx_disc.cue_binary(cue)

    def test_import_checks_reference_before_creating_output(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            image = psx_disc.Image()
            for _ in range(psx_disc.PVD_LBA):
                image.empty_form2_sector(psx_disc.SM_FORM2)
            image.form1_sector(b"\x01CD001", psx_disc.DATA_LAST)
            disc = folder / "data.bin"
            disc.write_bytes(image.data)

            out = folder / "imported"
            with self.assertRaisesRegex(SystemExit, "does not match the USA reference SHA-1"):
                psx_disc.main(["import", str(disc), "--out", str(out)])
            self.assertFalse(out.exists())

    def test_failed_extraction_can_be_retried_without_damaging_destination(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            image = psx_disc.Image()
            for _ in range(psx_disc.PVD_LBA):
                image.empty_form2_sector(psx_disc.SM_FORM2)
            image.form1_sector(b"\x01CD001", psx_disc.DATA_LAST)
            disc = folder / "data.bin"
            disc.write_bytes(image.data)
            digest = hashlib.sha1(image.data, usedforsecurity=False).hexdigest()
            (folder / "disc").mkdir()
            (folder / "disc" / "disc.sha1").write_text(f"{digest}  disc.bin\n", encoding="ascii")

            write_bytes = Path.write_bytes

            def fail_during_write(path, data):
                write_bytes(path, data[:1])
                raise OSError(errno.ENOSPC, "simulated full disk")

            with patch.object(psx_disc, "__file__", str(folder / "tools" / "psx_disc.py")):
                for existing in (False, True):
                    with self.subTest(existing=existing):
                        out = folder / ("existing" if existing else "new")
                        extracted = out / "license_data.dat"
                        if existing:
                            out.mkdir()
                            extracted.write_bytes(b"previous data")
                            (out / "keep.txt").write_text("unrelated", encoding="utf-8")
                        args = ["import", str(disc), "--out", str(out)]
                        with patch.object(Path, "write_bytes", fail_during_write):
                            with self.assertRaisesRegex(SystemExit, "simulated full disk"):
                                psx_disc.main(args)
                        if existing:
                            self.assertEqual(extracted.read_bytes(), b"previous data")
                        else:
                            self.assertFalse(out.exists())
                        self.assertEqual(psx_disc.main(args), 0)
                        expected_size = psx_disc.LICENSE_SECTORS * psx_disc.RAW
                        self.assertEqual(extracted.stat().st_size, expected_size)
                        if existing:
                            kept = (out / "keep.txt").read_text(encoding="utf-8")
                            self.assertEqual(kept, "unrelated")
