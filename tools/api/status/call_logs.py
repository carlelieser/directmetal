"""Reads the API call logs written by test runs."""

from pathlib import Path

from .catalog import Catalog
from .errors import StatusError

LOG_PATTERN = "*.log"


def read_lines(path: Path) -> list[str]:
    """Returns the complete lines; a final line without a newline was cut off mid-write."""
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise StatusError(f"read call log {path}: {error}") from error
    return text.split("\n")[:-1]


def file_calls(path: Path, catalog: Catalog) -> set[str]:
    calls = set()
    for number, line in enumerate(read_lines(path), start=1):
        key = line.strip()
        if not key:
            continue
        reason = catalog.unknown_reason(key)
        if reason is not None:
            raise StatusError(f"read call log {path}:{number}: {reason}")
        calls.add(key)
    return calls


def read_call_logs(directories: list[Path], catalog: Catalog) -> set[str]:
    calls: set[str] = set()
    for directory in directories:
        if not directory.is_dir():
            raise StatusError(f"read call logs: directory not found: {directory}")
        for path in sorted(directory.rglob(LOG_PATTERN)):
            if path.is_file():
                calls |= file_calls(path, catalog)
    return calls
