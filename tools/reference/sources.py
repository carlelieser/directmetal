"""Locates API test programs and the sources a Windows reference was recorded from."""

import hashlib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PROGRAMS_DIR = Path("tests", "api", "programs")
HARNESS_DIR = Path("tests", "api", "harness")
REFERENCE_DIR = Path("tests", "reference")
MANIFEST = PROGRAMS_DIR / "manifest.tsv"
MANIFEST_HEADER = ["program"]


class ReferenceToolError(Exception):
    pass


def read_manifest(root: Path) -> list[str]:
    path = root / MANIFEST
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ReferenceToolError(f"reading manifest {path} failed: {error}") from error
    if not lines or lines[0].split("\t") != MANIFEST_HEADER:
        raise ReferenceToolError(f"manifest {path} must start with the header {MANIFEST_HEADER}")
    programs = [line.split("\t")[0] for line in lines[1:] if line.strip()]
    if len(set(programs)) != len(programs):
        raise ReferenceToolError(f"manifest {path} lists a program twice")
    return programs


def source_files(root: Path, program: str) -> list[Path]:
    """Returns, relative to root, the program source followed by every harness file."""
    program_source = PROGRAMS_DIR / f"{program}.cpp"
    if not (root / program_source).is_file():
        raise ReferenceToolError(f"program {program} has no source {root / program_source}")
    harness = sorted(
        path.relative_to(root) for path in (root / HARNESS_DIR).rglob("*") if path.is_file()
    )
    return [program_source, *harness]


def file_digest(path: Path) -> str:
    # Line endings are normalized so a Windows checkout with autocrlf hashes like any other.
    content = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(content).hexdigest()


def source_digests(root: Path, program: str) -> dict[str, str]:
    """Maps each source path, in POSIX form, to the SHA-256 of its normalized content."""
    return {path.as_posix(): file_digest(root / path) for path in source_files(root, program)}


def reference_path(root: Path, program: str) -> Path:
    return root / REFERENCE_DIR / program / "reference.json"


def tolerance_path(root: Path, program: str) -> Path:
    return root / REFERENCE_DIR / program / "tolerance.json"
