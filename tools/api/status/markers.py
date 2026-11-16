"""Finds DM_IMPL / DM_STUB markers in front-end sources."""

import re
from dataclasses import dataclass
from pathlib import Path

from .catalog import Catalog
from .errors import StatusError

MARKER = re.compile(r'\bDM_(IMPL|STUB|STUB_RETURN|STUB_VOID)\s*\(\s*"([^"]*)"')
LEXEME = re.compile(
    r"""(?P<literal>"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*')|/\*.*?\*/|//[^\n]*""", re.DOTALL
)
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inl", ".mm"}
IMPLEMENTED = "implemented"
STUB = "stub"
KINDS = {"IMPL": IMPLEMENTED, "STUB": STUB, "STUB_RETURN": STUB, "STUB_VOID": STUB}


@dataclass(frozen=True)
class Marker:
    key: str
    kind: str
    location: str


def blank_comments(text: str) -> str:
    """Replaces comments with spaces so line numbers stay correct; literals are kept."""
    return LEXEME.sub(blank_unless_literal, text)


def blank_unless_literal(match: re.Match) -> str:
    if match["literal"] is not None:
        return match.group(0)
    return re.sub(r"[^\n]", " ", match.group(0))


def read_source(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise StatusError(f"scan front-end source {path}: {error}") from error


def file_markers(path: Path, relative: str, catalog: Catalog) -> list[Marker]:
    text = blank_comments(read_source(path))
    markers = []
    for match in MARKER.finditer(text):
        line = text.count("\n", 0, match.start()) + 1
        key = match.group(2)
        reason = catalog.unknown_reason(key)
        if reason is not None:
            raise StatusError(f"scan front-end source {path}:{line}: {reason}")
        markers.append(Marker(key, KINDS[match.group(1)], f"{relative}:{line}"))
    return markers


def scan_sources(root: Path, catalog: Catalog) -> list[Marker]:
    if not root.is_dir():
        raise StatusError(f"scan front-end sources: directory not found: {root}")
    markers = []
    for path in sorted(root.rglob("*")):
        if path.suffix in SOURCE_SUFFIXES and path.is_file():
            relative = path.relative_to(root).as_posix()
            markers += file_markers(path, relative, catalog)
    return markers
