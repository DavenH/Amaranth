import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from create_saxophone_reference_audit import prepare_media


class ReservedPlaybackBoundary(unittest.TestCase):
    def test_excluding_a_reserved_source_removes_old_media_without_reading_the_original(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "media").mkdir()
            stale = output / "media/iowa-reserved.wav"
            stale.write_bytes(b"old playback copy")
            original = output / "original.aiff"
            original.write_bytes(b"original stays untouched")
            tracks = [{"id": "iowa:reserved", "available": False, "_path": original}]
            with patch("create_saxophone_reference_audit.file_digest", side_effect=AssertionError("Reserved source read")), \
                    patch("create_saxophone_reference_audit.subprocess.run", side_effect=AssertionError("Reserved source decoded")):
                prepare_media(tracks, output)
            self.assertFalse(stale.exists())
            self.assertIsNone(tracks[0]["url"])
            self.assertEqual(original.read_bytes(), b"original stays untouched")


if __name__ == "__main__":
    unittest.main()
