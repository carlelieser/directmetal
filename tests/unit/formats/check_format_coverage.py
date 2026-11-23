"""Diffs the format table names against the DXGI_FORMAT and D3DFORMAT API inventories."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

# FOURCC extensions outside D3DFORMAT that the D3D9 table also covers (formats.hpp).
D3D9_EXTENSIONS = {"INTZ", "DF24", "DF16", "RAWZ", "ATI1", "ATI2", "NULL"}


def inventory_names(path: Path, enum: str, prefix: str, excluded: str) -> set[str]:
    try:
        members = json.loads(path.read_text())["enums"][enum]["members"]
    except (OSError, KeyError, json.JSONDecodeError) as error:
        raise RuntimeError(f"reading enum {enum} from {path} failed: {error}") from error
    # The inventory parser splits MAKEFOURCC arguments into stray one-letter members.
    return {name for name in members if name.startswith(prefix) and name != excluded}


def table_names(program: Path) -> dict[str, set[str]]:
    result = subprocess.run([program], capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"running {program} failed with status {result.returncode}")
    tables: dict[str, set[str]] = {"dxgi": set(), "d3d9": set()}
    for line in result.stdout.splitlines():
        table, name = line.split(" ", 1)
        tables[table].add(name)
    return tables


def report(label: str, expected: set[str], actual: set[str]) -> bool:
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    print(f"{label}: {len(expected)} expected, {len(actual)} in table")
    for name in missing:
        print(f"  missing from table: {name}")
    for name in extra:
        print(f"  not in inventory: {name}")
    return not missing and not extra


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--names-program", type=Path, required=True)
    parser.add_argument("--dxgi-json", type=Path, required=True)
    parser.add_argument("--d3d9-json", type=Path, required=True)
    args = parser.parse_args()

    tables = table_names(args.names_program)
    dxgi = inventory_names(args.dxgi_json, "DXGI_FORMAT", "DXGI_FORMAT_", "DXGI_FORMAT_FORCE_UINT")
    d3d9 = inventory_names(args.d3d9_json, "D3DFORMAT", "D3DFMT_", "D3DFMT_FORCE_DWORD")
    is_complete = report("DXGI_FORMAT", dxgi, tables["dxgi"])
    is_complete &= report("D3DFORMAT", d3d9 | D3D9_EXTENSIONS, tables["d3d9"])
    return 0 if is_complete else 1


if __name__ == "__main__":
    sys.exit(main())
