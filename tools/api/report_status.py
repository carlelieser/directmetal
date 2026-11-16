"""Reports per-method implementation status and test coverage of the DirectX front ends."""

import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from status.call_logs import read_call_logs  # noqa: E402
from status.catalog import load_catalog  # noqa: E402
from status.errors import StatusError  # noqa: E402
from status.exclusions import read_exclusions  # noqa: E402
from status.markers import scan_sources  # noqa: E402
from status.render import summary_markdown, to_json, to_markdown  # noqa: E402
from status.report import GroupStatus, apply_exclusions, build_report  # noqa: E402
from status.requirements import failures, parse_requirement  # noqa: E402

TOOL_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOL_DIR.parent.parent
CALL_LOG_ENV = "DIRECTMETAL_CALL_LOG_DIR"
EXIT_REQUIREMENT_UNMET = 1
EXIT_INVALID_INPUT = 2


def parse_arguments(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=REPO_ROOT / "docs" / "api")
    parser.add_argument("--sources", type=Path, default=REPO_ROOT / "src" / "frontends")
    parser.add_argument("--out-of-scope", type=Path, default=TOOL_DIR / "out_of_scope.tsv")
    parser.add_argument("--output", type=Path, default=REPO_ROOT / "out" / "api-status")
    parser.add_argument(
        "--call-log-dir",
        type=Path,
        action="append",
        help=f"directory of test call logs; repeatable (default: ${CALL_LOG_ENV} if set)",
    )
    parser.add_argument(
        "--require",
        action="append",
        default=[],
        metavar="GROUP:METRIC>=N[%]",
        help="exit 1 unless the threshold holds; also accepts <=; repeatable",
    )
    return parser.parse_args(argv)


def call_log_dirs(arguments: argparse.Namespace) -> list[Path]:
    if arguments.call_log_dir:
        return arguments.call_log_dir
    from_environment = os.environ.get(CALL_LOG_ENV)
    return [Path(from_environment)] if from_environment else []


def write_report(statuses: list[GroupStatus], output: Path) -> None:
    files = {f"{status.group}.md": to_markdown(status) for status in statuses}
    files["SUMMARY.md"] = summary_markdown(statuses)
    files["status.json"] = to_json(statuses)
    try:
        output.mkdir(parents=True, exist_ok=True)
        for name, text in files.items():
            (output / name).write_text(text, encoding="utf-8")
    except OSError as error:
        raise StatusError(f"write report to {output}: {error}") from error


def run(arguments: argparse.Namespace) -> int:
    requirements = [parse_requirement(text) for text in arguments.require]
    catalog = load_catalog(arguments.inventory)
    markers = scan_sources(arguments.sources, catalog)
    calls = read_call_logs(call_log_dirs(arguments), catalog)
    exclusions = read_exclusions(arguments.out_of_scope, catalog)
    statuses = apply_exclusions(build_report(catalog, markers, calls), exclusions)
    write_report(statuses, arguments.output)
    unmet = failures(requirements, statuses)
    for message in unmet:
        print(message, file=sys.stderr)
    return EXIT_REQUIREMENT_UNMET if unmet else 0


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        return run(arguments)
    except StatusError as error:
        print(f"report_status: {error}", file=sys.stderr)
        return EXIT_INVALID_INPUT


if __name__ == "__main__":
    sys.exit(main())
