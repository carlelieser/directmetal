"""Marks a PE image as a Wine builtin module.

Wine's loader treats a PE file as builtin when the bytes after its 64-byte DOS header hold the
32-byte, NUL-padded string "Wine builtin DLL". Only builtin modules get a unix library attached.
"""

import argparse
import struct
import sys
from pathlib import Path

DOS_HEADER_SIZE = 0x40
NT_HEADERS_OFFSET_FIELD = 0x3C
BUILTIN_SIGNATURE = b"Wine builtin DLL".ljust(32, b"\0")
SIGNATURE_END = DOS_HEADER_SIZE + len(BUILTIN_SIGNATURE)


class ImageError(Exception):
    pass


def nt_headers_offset(image: bytes) -> int:
    if len(image) < DOS_HEADER_SIZE or image[:2] != b"MZ":
        raise ImageError("missing DOS header")
    (offset,) = struct.unpack_from("<I", image, NT_HEADERS_OFFSET_FIELD)
    if image[offset : offset + 4] != b"PE\0\0":
        raise ImageError(f"no PE signature at e_lfanew {offset:#x}")
    return offset


def mark_builtin(image: bytes) -> bytes:
    offset = nt_headers_offset(image)
    if offset < SIGNATURE_END:
        raise ImageError(f"e_lfanew {offset:#x} leaves no room for the builtin signature")
    return image[:DOS_HEADER_SIZE] + BUILTIN_SIGNATURE + image[SIGNATURE_END:]


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("input", type=Path, help="linked PE image")
    parser.add_argument("output", type=Path, help="marked copy to write")
    return parser.parse_args(arguments)


def main(arguments: list[str]) -> int:
    options = parse_arguments(arguments)
    try:
        marked = mark_builtin(options.input.read_bytes())
    except ImageError as error:
        print(f"mark_builtin: {options.input}: {error}", file=sys.stderr)
        return 1
    options.output.write_bytes(marked)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
