"""Fails when an API test program or the harness changed since its Windows reference was recorded.

Regenerate a stale reference with `gh workflow run reference.yml --ref <branch>` and
`tools/reference/update.py --run <run id>`.
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sources import (  # noqa: E402
    REPO_ROOT,
    ReferenceToolError,
    read_manifest,
    reference_path,
    source_digests,
)

EXIT_STALE = 1
EXIT_INVALID_INPUT = 2


def recorded_digests(path: Path) -> dict[str, str]:
    try:
        sources = json.loads(path.read_text(encoding="utf-8")).get("sources")
    except (OSError, ValueError) as error:
        raise ReferenceToolError(f"reading reference {path} failed: {error}") from error
    if not isinstance(sources, dict):
        raise ReferenceToolError(f"reference {path} has no sources object")
    return sources


def staleness(root: Path, program: str) -> list[str]:
    path = reference_path(root, program)
    if not path.is_file():
        return [f"{program}: no reference at {path.relative_to(root).as_posix()}"]
    recorded = recorded_digests(path)
    current = source_digests(root, program)
    return [
        f"{program}: {source} {describe_change(recorded.get(source), current.get(source))}"
        for source in sorted(recorded.keys() | current.keys())
        if recorded.get(source) != current.get(source)
    ]


def describe_change(recorded: str | None, current: str | None) -> str:
    if recorded is None:
        return "was added after the reference was recorded"
    if current is None:
        return "was removed after the reference was recorded"
    return "changed after the reference was recorded"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    options = parser.parse_args(argv)
    try:
        problems = [
            problem
            for program in read_manifest(options.root)
            for problem in staleness(options.root, program)
        ]
    except ReferenceToolError as error:
        print(f"check_fresh: {error}", file=sys.stderr)
        return EXIT_INVALID_INPUT
    for problem in problems:
        print(f"check_fresh: {problem}", file=sys.stderr)
    return EXIT_STALE if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
