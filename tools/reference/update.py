"""Imports a Windows reference from a run of the Reference workflow.

Downloads the run's reference-<run id> artifact with gh, checks that it was recorded from the
run's commit and from the sources in this checkout, writes tests/reference/<test>/reference.json
for every program in the manifest and prints how each reference changed.
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from compare import differences, read_json  # noqa: E402
from sources import (  # noqa: E402
    REPO_ROOT,
    ReferenceToolError,
    read_manifest,
    reference_path,
    source_digests,
    tolerance_path,
)

WORKFLOW_NAME = "Reference"
RUN_FIELDS = "conclusion,headSha,url,workflowName"
RUN_URL = re.compile(r"https://github\.com/(?P<repository>[^/]+/[^/]+)/actions/runs/\d+")


def gh(arguments: list[str]) -> str:
    result = subprocess.run(["gh", *arguments], capture_output=True, text=True)
    if result.returncode != 0:
        raise ReferenceToolError(f"gh {' '.join(arguments)} failed: {result.stderr.strip()}")
    return result.stdout


def parse_run(run_id: str, text: str) -> dict:
    """Checks gh's description of a run and adds the repository named by its URL."""
    try:
        run = json.loads(text)
    except ValueError as error:
        raise ReferenceToolError(f"gh run view {run_id} printed invalid JSON: {error}") from error
    missing = [field for field in RUN_FIELDS.split(",") if not isinstance(run.get(field), str)]
    if missing:
        raise ReferenceToolError(f"gh run view {run_id} lacks {', '.join(missing)}")
    if run["workflowName"] != WORKFLOW_NAME:
        raise ReferenceToolError(
            f"run {run_id} is a {run['workflowName']} run, not {WORKFLOW_NAME}"
        )
    if run["conclusion"] != "success":
        raise ReferenceToolError(f"run {run_id} concluded {run['conclusion']}, not success")
    match = RUN_URL.fullmatch(run["url"])
    if match is None:
        raise ReferenceToolError(f"run {run_id} has an unexpected URL {run['url']}")
    return {**run, "repository": match["repository"]}


def view_run(run_id: str) -> dict:
    return parse_run(run_id, gh(["run", "view", run_id, "--json", RUN_FIELDS]))


def check_metadata(metadata: dict, run_id: str, run: dict) -> None:
    expectations = [
        ("run_id", run_id, "run"),
        ("repository", run["repository"], "repository"),
        ("commit", run["headSha"], "commit"),
    ]
    for key, expected, description in expectations:
        recorded = metadata.get(key)
        if str(recorded) != expected:
            raise ReferenceToolError(
                f"artifact of run {run_id} records {description} {recorded}, expected {expected}"
            )


def build_reference(root: Path, artifact: Path, program: str) -> dict:
    recorded_sources = read_json(artifact / program / "sources.json")
    if recorded_sources != source_digests(root, program):
        raise ReferenceToolError(
            f"{program}: sources in this checkout differ from those the run recorded; "
            "check out the run's commit"
        )
    x64 = read_json(artifact / program / "x64.json")
    mismatches = differences(x64, read_json(artifact / program / "x86.json"), None)
    if mismatches:
        raise ReferenceToolError(f"{program}: x86 output differs from x64: {mismatches[0]}")
    metadata = read_json(artifact / "metadata.json")
    return {"test": program, "sources": recorded_sources, "metadata": metadata, "result": x64}


# differences() words changes as actual against expected; here the new reference is the actual.
CHANGE_WORDING = [
    (": expected ", ": was "),
    (", got ", ", now "),
    (": not in the reference", ": added"),
    (": missing", ": removed"),
]


def reword_change(change: str) -> str:
    for old, new in CHANGE_WORDING:
        change = change.replace(old, new)
    return change


def describe_update(path: Path, reference: dict) -> list[str]:
    if not path.is_file():
        return ["new reference"]
    previous = read_json(path)
    previous_metadata = previous.get("metadata")
    previous_result = previous.get("result")
    is_well_formed = isinstance(previous_metadata, dict) and isinstance(previous_result, dict)
    if not is_well_formed:
        raise ReferenceToolError(f"reference {path} lacks a metadata or result object")
    lines = [
        f"metadata.{key}: {json.dumps(previous_metadata.get(key))} -> {json.dumps(value)}"
        for key, value in sorted(reference["metadata"].items())
        if key not in {"run_id", "run_attempt", "commit", "ref"}
        and previous_metadata.get(key) != value
    ]
    lines += [
        reword_change(change) for change in differences(previous_result, reference["result"], None)
    ]
    return lines or ["results unchanged"]


def import_artifact(root: Path, artifact: Path, run_id: str, run: dict) -> None:
    check_metadata(read_json(artifact / "metadata.json"), run_id, run)
    for program in read_manifest(root):
        reference = build_reference(root, artifact, program)
        path = reference_path(root, program)
        print(f"{program}: {path.relative_to(root).as_posix()}")
        for line in describe_update(path, reference):
            print(f"  {line}")
        path.parent.mkdir(parents=True, exist_ok=True)
        text = json.dumps(reference, indent=2, sort_keys=True) + "\n"
        path.write_text(text, encoding="utf-8")
        if not tolerance_path(root, program).is_file():
            print(f"  warning: no {tolerance_path(root, program).name} beside the reference")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--run", required=True, help="Reference workflow run id")
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    options = parser.parse_args(argv)
    try:
        run = view_run(options.run)
        with tempfile.TemporaryDirectory(prefix="reference-") as directory:
            name = f"reference-{options.run}"
            gh(["run", "download", options.run, "--name", name, "--dir", directory])
            import_artifact(options.root, Path(directory), options.run, run)
    except ReferenceToolError as error:
        print(f"update: {error}", file=sys.stderr)
        return 1
    print(f"imported {run['url']}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
