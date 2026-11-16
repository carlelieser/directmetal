"""Generates docs/api/<group>.{md,json} from headers fetched by fetch_headers.sh."""

import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from inventory.directx_parser import parse_directx_group  # noqa: E402
from inventory.metal_parser import parse_metal_group  # noqa: E402
from inventory.model import Inventory  # noqa: E402
from inventory.render import GENERATED_NOTE, summary_lines, to_json, to_markdown  # noqa: E402

TOOL_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOL_DIR.parent.parent
HEADERS_DIR = REPO_ROOT / "third_party" / "api-headers"
OUTPUT_DIR = REPO_ROOT / "docs" / "api"
DIRECTX_GROUPS = ["d3d9", "d3d10", "d3d11", "d3d12", "dxgi", "common"]


def directx_provenance(group: str) -> str:
    with open(TOOL_DIR / "sources.tsv", encoding="utf-8") as handle:
        rows = [
            row for row in csv.reader(handle, delimiter="\t") if row and not row[0].startswith("#")
        ]
    origins = sorted(
        {f"`{repo}@{revision[:12]}`" for name, repo, revision, _ in rows if name == group}
    )
    files = sorted(Path(path).name for name, _, _, path in rows if name == group)
    return f"Headers: {', '.join(f'`{name}`' for name in files)} from {', '.join(origins)}."


def metal_provenance() -> str:
    version = (HEADERS_DIR / "metal" / "SDK_VERSION").read_text(encoding="utf-8").strip()
    frameworks = sorted(path.name for path in (HEADERS_DIR / "metal").iterdir() if path.is_dir())
    return f"Headers: {', '.join(frameworks)} frameworks from the macOS {version} SDK."


def write_inventory(inventory: Inventory, provenance: str) -> None:
    (OUTPUT_DIR / f"{inventory.group}.md").write_text(
        to_markdown(inventory, provenance), encoding="utf-8"
    )
    (OUTPUT_DIR / f"{inventory.group}.json").write_text(
        to_json(inventory, provenance), encoding="utf-8"
    )


def write_index(inventories: list[Inventory]) -> None:
    lines = [GENERATED_NOTE, "", "# API inventory summary", ""]
    for inventory in inventories:
        lines += [
            f"## [{inventory.group}]({inventory.group}.md)",
            "",
            "| Kind | Count |",
            "| --- | ---: |",
        ]
        lines += [*summary_lines(inventory), ""]
    (OUTPUT_DIR / "SUMMARY.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    if not HEADERS_DIR.is_dir():
        raise SystemExit(f"missing {HEADERS_DIR}; run tools/api/fetch_headers.sh first")
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    inventories = []
    for group in DIRECTX_GROUPS:
        inventory = parse_directx_group(HEADERS_DIR / group)
        write_inventory(inventory, directx_provenance(group))
        inventories.append(inventory)
    metal = parse_metal_group(HEADERS_DIR / "metal")
    write_inventory(metal, metal_provenance())
    write_index([*inventories, metal])


if __name__ == "__main__":
    main()
