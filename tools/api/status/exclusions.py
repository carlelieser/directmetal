"""Reads the reviewed list of out-of-scope API entries."""

from pathlib import Path

from .catalog import Catalog
from .errors import StatusError

COLUMN_SEPARATOR = "\t"
COMMENT_PREFIX = "#"


def read_rows(path: Path) -> list[tuple[int, str]]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as error:
        raise StatusError(f"read exclusions {path}: {error}") from error
    return [
        (number, line)
        for number, line in enumerate(lines, start=1)
        if line.strip() and not line.startswith(COMMENT_PREFIX)
    ]


def row_keys(name: str, catalog: Catalog) -> tuple[list[str], str | None]:
    """Returns the keys a row excludes, or no keys and the reason `name` is unknown."""
    interface_keys = catalog.interface_keys(name)
    if interface_keys is not None:
        return interface_keys, None
    reason = catalog.unknown_reason(name)
    return ([], reason) if reason is not None else ([name], None)


def read_exclusions(path: Path, catalog: Catalog) -> dict[str, str]:
    """Maps each excluded key to the reason it is out of scope."""
    exclusions: dict[str, str] = {}
    for number, line in read_rows(path):
        name, _, reason = (column.strip() for column in line.partition(COLUMN_SEPARATOR))
        if not name or not reason:
            raise StatusError(f"read exclusions {path}:{number}: expected KEY<TAB>REASON")
        keys, unknown = row_keys(name, catalog)
        if unknown is not None:
            raise StatusError(f"read exclusions {path}:{number}: {unknown}")
        exclusions.update(dict.fromkeys(keys, reason))
    return exclusions
