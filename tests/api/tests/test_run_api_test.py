import json
import os
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import run_api_test  # noqa: E402

RESULT = {"create_device": "0x00000000", "cases": [{"name": "case", "map": "0x00000000"}]}
# Fake programs print RESULT, extended with whatever the body adds, unless the body exits first.
PROGRAM = """#!{python}
import json, os, signal, sys, time
document = json.loads({result!r})
{body}
print(json.dumps(document))
"""


class Run:
    def __init__(self, status: int, stdout: str, stderr: str) -> None:
        self.status = status
        self.stdout = stdout
        self.stderr = stderr


class RunnerTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.root)
        reference_dir = self.root / "reference" / "program"
        reference_dir.mkdir(parents=True)
        self.reference = reference_dir / "reference.json"
        self.reference.write_text(json.dumps({"metadata": {}, "result": RESULT}))
        tolerance = {"max_channel_difference": {}}
        (reference_dir / "tolerance.json").write_text(json.dumps(tolerance))

    def program(self, body: str = "", name: str = "program") -> Path:
        path = self.root / name
        text = PROGRAM.format(python=sys.executable, result=json.dumps(RESULT), body=body)
        path.write_text(text)
        path.chmod(0o755)
        return path

    def run_runner(self, arguments: list[str], environment: dict[str, str] | None = None) -> Run:
        stdout, stderr = StringIO(), StringIO()
        variables = {"PATH": os.environ.get("PATH", ""), **(environment or {})}
        with mock.patch.dict(os.environ, variables, clear=True):
            with redirect_stdout(stdout), redirect_stderr(stderr):
                status = run_api_test.main(arguments)
        return Run(status, stdout.getvalue(), stderr.getvalue())

    def run_native(self, program: Path, extra: list[str] | None = None) -> Run:
        arguments = ["--mode", "native", "--program", str(program)]
        return self.run_runner(arguments + ["--reference", str(self.reference)] + (extra or []))


class NativeModeTest(RunnerTestCase):
    def test_passes_when_output_matches_reference(self) -> None:
        run = self.run_native(self.program())
        self.assertEqual(run.status, 0, run.stderr)
        self.assertEqual(json.loads(run.stdout), RESULT)

    def test_prints_each_difference_on_mismatch(self) -> None:
        body = 'document["create_device"] = "0x80004001"; document["cases"][0]["map"] = "0x1"'
        run = self.run_native(self.program(body))
        self.assertEqual(run.status, 1)
        self.assertIn("2 differences from", run.stderr)
        self.assertIn('create_device: expected "0x00000000", got "0x80004001"', run.stderr)
        self.assertIn('cases[case].map: expected "0x00000000", got "0x1"', run.stderr)

    def test_fails_on_abort_even_after_matching_output(self) -> None:
        body = "print(json.dumps(document), flush=True); os.abort()"
        run = self.run_native(self.program(body))
        self.assertEqual(run.status, 1)
        self.assertIn("abnormal exit: terminated by SIGABRT", run.stderr)

    def test_fails_on_nonzero_exit_with_matching_output(self) -> None:
        body = "print(json.dumps(document)); sys.exit(3)"
        run = self.run_native(self.program(body))
        self.assertEqual(run.status, 1)
        self.assertIn("abnormal exit: status 0x3, expected 0", run.stderr)

    def test_prints_differences_after_abnormal_exit(self) -> None:
        body = 'print(json.dumps({"create_device": "0x80004001"})); sys.exit(1)'
        run = self.run_native(self.program(body))
        self.assertEqual(run.status, 1)
        self.assertIn("abnormal exit: status 0x1", run.stderr)
        self.assertIn('create_device: expected "0x00000000", got "0x80004001"', run.stderr)

    def test_fails_when_stdout_is_not_json(self) -> None:
        run = self.run_native(self.program('print("not json"); sys.exit(0)'))
        self.assertEqual(run.status, 1)
        self.assertIn("stdout is not one JSON document", run.stderr)

    def test_enables_metal_validation_in_assert_mode(self) -> None:
        names = list(run_api_test.VALIDATION_ENVIRONMENT)
        body = f"document['cases'][0]['map'] = ' '.join(os.environ.get(n, '-') for n in {names!r})"
        run = self.run_native(self.program(body))
        self.assertIn('got "1 1 assert"', run.stderr)

    def test_passes_without_reference_on_clean_exit(self) -> None:
        arguments = ["--mode", "native", "--program", str(self.program("sys.exit(0)"))]
        self.assertEqual(self.run_runner(arguments).status, 0)

    def test_timeout_is_an_error_not_a_failure(self) -> None:
        with mock.patch.object(run_api_test, "TIMEOUT_SECONDS", 1):
            run = self.run_native(self.program("time.sleep(30)"))
        self.assertEqual(run.status, 99)
        self.assertIn("timed out after 1 s", run.stderr)


class SetupTest(RunnerTestCase):
    def test_skip_reports_reason_without_running(self) -> None:
        marker = self.root / "ran"
        program = self.program(f"open({str(marker)!r}, 'w')")
        run = self.run_native(program, ["--skip", "front end not built"])
        self.assertEqual(run.status, 77)
        self.assertIn("skipped: front end not built", run.stderr)
        self.assertFalse(marker.exists())

    def test_missing_program_is_a_setup_error(self) -> None:
        run = self.run_native(self.root / "missing")
        self.assertEqual(run.status, 99)
        self.assertIn("missing build output", run.stderr)

    def test_missing_reference_is_a_setup_error(self) -> None:
        self.reference.unlink()
        run = self.run_native(self.program())
        self.assertEqual(run.status, 99)

    def test_missing_tolerance_is_a_setup_error(self) -> None:
        self.reference.with_name("tolerance.json").unlink()
        run = self.run_native(self.program())
        self.assertEqual(run.status, 99)


ABORT_TEXT = "failed assertion `Copy From Buffer Validation"


class ExpectAbortTest(RunnerTestCase):
    def run_expecting_abort(self, body: str) -> Run:
        arguments = ["--mode", "native", "--program", str(self.program(body))]
        return self.run_runner(arguments + ["--expect-abort-stderr", ABORT_TEXT])

    def test_passes_on_abort_with_expected_message(self) -> None:
        run = self.run_expecting_abort(
            f"sys.stderr.write({ABORT_TEXT!r}); sys.stderr.flush(); os.abort()"
        )
        self.assertEqual(run.status, 0, run.stderr)
        self.assertIn("aborted as expected: terminated by SIGABRT", run.stderr)

    def test_passes_on_failed_exit_with_expected_message(self) -> None:
        run = self.run_expecting_abort(f"sys.stderr.write({ABORT_TEXT!r}); sys.exit(1)")
        self.assertEqual(run.status, 0, run.stderr)

    def test_fails_on_abort_for_another_reason(self) -> None:
        run = self.run_expecting_abort("sys.stderr.write('segmentation fault'); os.abort()")
        self.assertEqual(run.status, 1)
        self.assertIn("terminated by SIGABRT without", run.stderr)

    def test_fails_on_clean_exit(self) -> None:
        run = self.run_expecting_abort(f"sys.stderr.write({ABORT_TEXT!r})")
        self.assertEqual(run.status, 1)
        self.assertIn("exited cleanly", run.stderr)

    def test_timeout_is_an_error(self) -> None:
        with mock.patch.object(run_api_test, "TIMEOUT_SECONDS", 1):
            run = self.run_expecting_abort(f"sys.stderr.write({ABORT_TEXT!r}); time.sleep(30)")
        self.assertEqual(run.status, 99)

    def test_excludes_reference(self) -> None:
        arguments = ["--mode", "native", "--program", str(self.program())]
        arguments += ["--reference", str(self.reference), "--expect-abort-stderr", ABORT_TEXT]
        with self.assertRaises(SystemExit), redirect_stderr(StringIO()):
            run_api_test.main(arguments)


# Stands in for wine, winepath and wineserver: records how it was called, then runs the staged
# program (a script) directly.
FAKE_WINE = """#!/bin/sh
case "$(basename "$0")" in
    winepath) echo "Z:$2"; exit 0 ;;
    wineserver) exit 0 ;;
esac
run_dir="$(dirname "$WINEPREFIX")"
env > "$run_dir/environment"
ls "$WINEDLLPATH/x86_64-unix" > "$run_dir/unix-dir"
exec "$@"
"""


class WineModeTest(RunnerTestCase):
    def setUp(self) -> None:
        super().setUp()
        bin_dir = self.root / "wine" / "bin"
        bin_dir.mkdir(parents=True)
        for name in ("wine", "winepath", "wineserver"):
            (bin_dir / name).write_text(FAKE_WINE)
            (bin_dir / name).chmod(0o755)
        self.unix_library = self.root / "canary.so"
        self.unix_library.write_text("")
        self.work_dir = self.root / "work"

    def run_wine(self, program: Path, extra: list[str] | None = None) -> Run:
        arguments = ["--mode", "wine", "--arch", "win64", "--program", str(program)]
        arguments += ["--unix-library", str(self.unix_library), "--work-dir", str(self.work_dir)]
        arguments += ["--reference", str(self.reference)] + (extra or [])
        return self.run_runner(arguments, {"DIRECTMETAL_WINE": str(self.root / "wine")})

    def run_dirs(self) -> list[Path]:
        return list(self.work_dir.iterdir())

    def test_passes_and_removes_run_dir(self) -> None:
        run = self.run_wine(self.program(), ["--call-log-dir", str(self.root / "logs")])
        self.assertEqual(run.status, 0, run.stderr)
        self.assertEqual(self.run_dirs(), [])

    def test_keeps_run_dir_and_reports_wine_environment_on_failure(self) -> None:
        run = self.run_wine(
            self.program("sys.exit(5)"), ["--call-log-dir", str(self.root / "logs")]
        )
        self.assertEqual(run.status, 1)
        self.assertIn("abnormal exit: status 0x5", run.stderr)
        [run_dir] = self.run_dirs()
        self.assertIn(f"kept {run_dir}", run.stderr)
        self.assertEqual((run_dir / "unix-dir").read_text(), "directmetal.so\n")
        environment = (run_dir / "environment").read_text().splitlines()
        for name, value in run_api_test.VALIDATION_ENVIRONMENT.items():
            self.assertIn(f"{name}={value}", environment)
        self.assertIn(
            "WINEDLLOVERRIDES=d3d11=n;directmetal=b;mscoree,mshtml=;winedbg.exe=", environment
        )
        self.assertIn(f"DIRECTMETAL_CALL_LOG_DIR=Z:{(self.root / 'logs').resolve()}", environment)

    def test_replaces_kept_run_dir(self) -> None:
        self.assertEqual(self.run_wine(self.program("sys.exit(5)")).status, 1)
        [kept] = self.run_dirs()
        self.assertEqual(self.run_wine(self.program("sys.exit(5)")).status, 1)
        [latest] = self.run_dirs()
        self.assertNotEqual(kept, latest)

    def test_expected_abort_removes_run_dir(self) -> None:
        program = self.program(f"sys.stderr.write({ABORT_TEXT!r}); sys.exit(1)")
        arguments = ["--mode", "wine", "--arch", "win64", "--program", str(program)]
        arguments += ["--unix-library", str(self.unix_library), "--work-dir", str(self.work_dir)]
        arguments += ["--expect-abort-stderr", ABORT_TEXT]
        run = self.run_runner(arguments, {"DIRECTMETAL_WINE": str(self.root / "wine")})
        self.assertEqual(run.status, 0, run.stderr)
        self.assertEqual(self.run_dirs(), [])

    def test_missing_stand_in_unix_library_is_not_a_pass(self) -> None:
        self.unix_library.unlink()
        program = self.program(f"sys.stderr.write({ABORT_TEXT!r}); sys.exit(1)")
        arguments = ["--mode", "wine", "--arch", "win64", "--program", str(program)]
        arguments += ["--unix-library", str(self.unix_library), "--work-dir", str(self.work_dir)]
        arguments += ["--expect-abort-stderr", ABORT_TEXT]
        run = self.run_runner(arguments, {"DIRECTMETAL_WINE": str(self.root / "wine")})
        self.assertEqual(run.status, 99)

    def test_missing_unix_library_is_a_setup_error(self) -> None:
        self.unix_library.unlink()
        self.assertEqual(self.run_wine(self.program()).status, 99)

    def test_unwritable_work_dir_is_a_setup_error(self) -> None:
        self.work_dir.write_text("a file, not a directory")
        self.assertEqual(self.run_wine(self.program()).status, 99)

    def test_missing_wine_is_a_setup_error(self) -> None:
        shutil.rmtree(self.root / "wine")
        self.assertEqual(self.run_wine(self.program()).status, 99)


if __name__ == "__main__":
    unittest.main()
