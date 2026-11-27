import tempfile
import unittest
from pathlib import Path

from tools import psx_disc


class DiscImportTest(unittest.TestCase):
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
