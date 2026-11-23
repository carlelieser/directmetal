"""Runs an API test program natively or under Wine with Metal validation and checks its output.

The program always runs with Metal API and shader validation in assert mode, so a validation error
aborts it. Any exit other than a clean 0 fails the test. With --reference, the JSON document the
program prints to stdout must match the reference's result within the per-format tolerances of
tolerance.json beside it (tools/reference/compare.py); every difference is printed. With
--expect-abort-stderr, the test instead passes only when the program exits abnormally and stderr
contains the given text, which proves a validation abort rather than any failure.

Under Wine (--mode wine) each run is staged by run_under_wine.py: a fresh directory under
--work-dir with the program and its app-local DLLs, a new WINEPREFIX, and directmetal.dll as a
builtin paired with --unix-library. The run directory is deleted on success and kept on failure
until the next run.

Exit status: 0 pass, 1 test failure, 77 skipped (--skip), 99 setup error or timeout. Meson reports
99 as an error whatever the test expects, so neither ever counts as a pass.
"""

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

API_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(API_DIR.parent.parent / "tools" / "reference"))
sys.path.insert(0, str(API_DIR))

import compare  # noqa: E402
import run_under_wine as wine_staging  # noqa: E402
from sources import ReferenceToolError  # noqa: E402

EXIT_PASS = 0
EXIT_FAILURE = 1
EXIT_SKIP = 77
EXIT_ERROR = 99
TIMEOUT_SECONDS = 150
# Wine pairs directmetal.dll with <dll_path>/x86_64-unix/directmetal.so, whatever the input's name.
UNIX_LIBRARY_NAME = "directmetal.so"
VALIDATION_ENVIRONMENT = {
    "MTL_DEBUG_LAYER": "1",
    "MTL_SHADER_VALIDATION": "1",
    "MTL_DEBUG_LAYER_ERROR_MODE": "assert",
}
# Without winedbg a crash ends the process instead of waiting on a debugger or dialog.
WINE_DLL_OVERRIDES = wine_staging.DLL_OVERRIDES + ";winedbg.exe="


@dataclass(frozen=True)
class Expectation:
    result: dict
    tolerances: dict[str, float]
    reference: Path


@dataclass(frozen=True)
class Outcome:
    returncode: int
    stdout: str
    stderr: str
    # Kept when the run fails so its prefix and staged files can be inspected; None natively.
    run_dir: Path | None = None
    timeout_error: str | None = None


def load_expectation(options: argparse.Namespace) -> Expectation | None:
    if options.reference is None:
        return None
    tolerance = options.tolerance or options.reference.with_name("tolerance.json")
    try:
        result = compare.reference_result(options.reference)
        tolerances = compare.read_tolerances(tolerance)
    except ReferenceToolError as error:
        raise wine_staging.SetupError(str(error)) from error
    return Expectation(result, tolerances, options.reference)


def validated_environment(base: dict[str, str]) -> dict[str, str]:
    return {**base, **VALIDATION_ENVIRONMENT}


def execute(command: list[str], environment: dict[str, str]) -> Outcome:
    try:
        result = subprocess.run(
            command,
            env=environment,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as error:
        message = f"timed out after {TIMEOUT_SECONDS} s"
        return Outcome(-signal.SIGKILL, *partial_output(error), timeout_error=message)
    except OSError as error:
        raise wine_staging.SetupError(f"starting {command[0]} failed: {error}") from error
    return Outcome(result.returncode, result.stdout, result.stderr)


def partial_output(error: subprocess.TimeoutExpired) -> tuple[str, str]:
    streams = [error.stdout or b"", error.stderr or b""]
    texts = [data if isinstance(data, str) else data.decode("utf-8", "replace") for data in streams]
    return texts[0], texts[1]


def run_native(options: argparse.Namespace) -> Outcome:
    if not options.program.is_file():
        raise wine_staging.SetupError(f"missing build output {options.program}")
    environment = validated_environment(dict(os.environ))
    if options.call_log_dir is not None:
        environment["DIRECTMETAL_CALL_LOG_DIR"] = str(options.call_log_dir.resolve())
    command = [str(options.program.resolve()), *options.program_arg]
    return execute(command, environment)


def stage_for_wine(options: argparse.Namespace) -> wine_staging.Staging:
    if not options.unix_library.is_file():
        raise wine_staging.SetupError(f"missing {options.unix_library}; build the unix target")
    files = wine_staging.TestFiles(
        options.program, options.dll, options.builtin, None, options.call_log_dir
    )
    # A kept run holds a whole prefix, so only the latest one is kept.
    wine_staging.reset_directory(options.work_dir)
    staging = wine_staging.stage(files, options.arch, options.work_dir)
    unix_dir = staging.dll_path / wine_staging.UNIX_DIR
    unix_dir.mkdir(parents=True)
    shutil.copy2(options.unix_library, unix_dir / UNIX_LIBRARY_NAME)
    return staging


def run_wine(options: argparse.Namespace) -> Outcome:
    wine = wine_staging.find_wine()
    staging = stage_for_wine(options)
    environment = validated_environment(wine_staging.wine_environment(staging))
    environment["WINEDLLOVERRIDES"] = WINE_DLL_OVERRIDES
    command = [str(wine), str(staging.app_dir / options.program.name), *options.program_arg]
    try:
        if staging.call_log_dir is not None:
            log_dir = wine_staging.windows_path(wine, environment, staging.call_log_dir)
            environment["DIRECTMETAL_CALL_LOG_DIR"] = log_dir
        outcome = execute(command, environment)
    finally:
        wine_staging.stop_wineserver(wine, environment)
    return Outcome(
        outcome.returncode, outcome.stdout, outcome.stderr, staging.root, outcome.timeout_error
    )


def signal_name(number: int) -> str:
    try:
        return signal.Signals(number).name
    except ValueError:
        return f"signal {number}"


def exit_summary(outcome: Outcome) -> str:
    if outcome.returncode < 0:
        return f"terminated by {signal_name(-outcome.returncode)}"
    return f"status {outcome.returncode:#x}"


def describe_exit(outcome: Outcome) -> str:
    expected = "" if outcome.returncode < 0 else ", expected 0"
    return f"abnormal exit: {exit_summary(outcome)}{expected}"


def abort_problems(outcome: Outcome, text: str) -> list[str]:
    if outcome.returncode == 0:
        return [f"exited cleanly, expected an abnormal exit with {text!r} on stderr"]
    if text not in outcome.stderr:
        return [f"{exit_summary(outcome)} without {text!r} on stderr"]
    return []


def parse_output(stdout: str) -> tuple[dict | None, str | None]:
    try:
        document = json.loads(stdout)
    except ValueError as error:
        return None, f"stdout is not one JSON document: {error}"
    if not isinstance(document, dict):
        return None, "stdout is not a JSON object"
    return document, None


def output_problems(stdout: str, expectation: Expectation) -> list[str]:
    actual, error = parse_output(stdout)
    if actual is None:
        return [error]
    differences = compare.differences(expectation.result, actual, expectation.tolerances)
    if not differences:
        return []
    header = f"{len(differences)} differences from {expectation.reference}:"
    return [header, *(f"  {line}" for line in differences)]


def problems(outcome: Outcome, expectation: Expectation | None) -> list[str]:
    if outcome.returncode == 0:
        return [] if expectation is None else output_problems(outcome.stdout, expectation)
    found = [describe_exit(outcome)]
    # A program that stops early still prints what it recorded, which shows where it diverged.
    if expectation is not None and outcome.stdout.strip():
        found += output_problems(outcome.stdout, expectation)
    return found


def keep_failed_run(outcome: Outcome, found: list[str]) -> None:
    for line in found:
        print(f"run_api_test: {line}", file=sys.stderr)
    if outcome.run_dir is not None:
        print(f"run_api_test: kept {outcome.run_dir}", file=sys.stderr)


def report(outcome: Outcome, expectation: Expectation | None, abort_text: str | None) -> int:
    sys.stdout.write(outcome.stdout)
    sys.stderr.write(outcome.stderr)
    if outcome.timeout_error is not None:
        keep_failed_run(outcome, [outcome.timeout_error])
        return EXIT_ERROR
    if abort_text is None:
        found = problems(outcome, expectation)
    else:
        found = abort_problems(outcome, abort_text)
    if found:
        keep_failed_run(outcome, found)
        return EXIT_FAILURE
    if abort_text is not None:
        print(f"run_api_test: aborted as expected: {exit_summary(outcome)}", file=sys.stderr)
    if outcome.run_dir is not None:
        shutil.rmtree(outcome.run_dir)
    return EXIT_PASS


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--mode", choices=["native", "wine"], required=True)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--program-arg", action="append", default=[], help="program argument")
    parser.add_argument("--reference", type=Path, help="reference.json; omit to check exit only")
    parser.add_argument("--tolerance", type=Path, help="default: tolerance.json beside reference")
    parser.add_argument(
        "--expect-abort-stderr",
        metavar="TEXT",
        help="pass only on an abnormal exit with TEXT on stderr",
    )
    parser.add_argument("--skip", metavar="REASON", help="report the test as skipped")
    wine = parser.add_argument_group("wine mode")
    wine.add_argument("--arch", choices=sorted(wine_staging.PE_DIRS))
    wine.add_argument("--dll", type=Path, action="append", default=[], help="app-local DLL")
    wine.add_argument("--builtin", type=Path, action="append", default=[], help="builtin DLL")
    wine.add_argument("--unix-library", type=Path, help="staged as directmetal.so")
    wine.add_argument("--work-dir", type=Path)
    wine.add_argument("--call-log-dir", type=Path, help="emptied, then receives call logs")
    options = parser.parse_args(arguments)
    is_wine_incomplete = options.mode == "wine" and None in (
        options.arch,
        options.unix_library,
        options.work_dir,
    )
    if is_wine_incomplete:
        parser.error("--mode wine needs --arch, --unix-library and --work-dir")
    if options.reference is not None and options.expect_abort_stderr is not None:
        parser.error("--reference and --expect-abort-stderr exclude each other")
    return options


def main(arguments: list[str]) -> int:
    options = parse_arguments(arguments)
    if options.skip is not None:
        print(f"run_api_test: skipped: {options.skip}", file=sys.stderr)
        return EXIT_SKIP
    try:
        # Emptied before anything else can fail, so a failed setup never leaves old logs for a gate.
        if options.call_log_dir is not None:
            wine_staging.reset_directory(options.call_log_dir)
        expectation = load_expectation(options)
        outcome = run_wine(options) if options.mode == "wine" else run_native(options)
    except (wine_staging.SetupError, OSError) as error:
        print(f"run_api_test: setup failed: {error}", file=sys.stderr)
        return EXIT_ERROR
    return report(outcome, expectation, options.expect_abort_stderr)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
