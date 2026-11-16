import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from mark_builtin import BUILTIN_SIGNATURE, ImageError, main, mark_builtin  # noqa: E402

DOS_STUB = b"This program cannot be run in DOS mode.\r\r\n$".ljust(0x40, b"\0")


def pe_image(nt_offset: int = 0x80) -> bytes:
    header = bytearray(b"MZ".ljust(0x40, b"\0"))
    struct.pack_into("<I", header, 0x3C, nt_offset)
    image = bytearray(bytes(header) + DOS_STUB + bytes(range(64)))
    image[nt_offset : nt_offset + 4] = b"PE\0\0"
    return bytes(image)


class MarkBuiltinTest(unittest.TestCase):
    def test_writes_signature_after_dos_header(self) -> None:
        marked = mark_builtin(pe_image())
        self.assertEqual(marked[0x40:0x60], BUILTIN_SIGNATURE)
        self.assertTrue(marked[0x40:0x60].startswith(b"Wine builtin DLL\0"))

    def test_preserves_every_other_byte(self) -> None:
        image = pe_image()
        marked = mark_builtin(image)
        self.assertEqual(len(marked), len(image))
        self.assertEqual(marked[:0x40], image[:0x40])
        self.assertEqual(marked[0x60:], image[0x60:])

    def test_is_idempotent(self) -> None:
        marked = mark_builtin(pe_image())
        self.assertEqual(mark_builtin(marked), marked)

    def test_rejects_nt_headers_inside_signature(self) -> None:
        with self.assertRaisesRegex(ImageError, "no room"):
            mark_builtin(pe_image(nt_offset=0x50))

    def test_rejects_missing_dos_header(self) -> None:
        with self.assertRaisesRegex(ImageError, "DOS header"):
            mark_builtin(b"\x7fELF" + bytes(0x80))

    def test_rejects_missing_pe_signature(self) -> None:
        image = bytearray(pe_image())
        image[0x80:0x84] = b"NE\0\0"
        with self.assertRaisesRegex(ImageError, "no PE signature"):
            mark_builtin(bytes(image))


class MainTest(unittest.TestCase):
    def test_writes_marked_copy(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory, "in.dll"), Path(directory, "out.dll")
            source.write_bytes(pe_image())
            self.assertEqual(main([str(source), str(target)]), 0)
            self.assertEqual(target.read_bytes(), mark_builtin(pe_image()))
            self.assertEqual(source.read_bytes(), pe_image())

    def test_fails_without_output_for_invalid_image(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source, target = Path(directory, "in.dll"), Path(directory, "out.dll")
            source.write_bytes(b"not a pe image")
            self.assertEqual(main([str(source), str(target)]), 1)
            self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main()
