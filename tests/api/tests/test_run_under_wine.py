import os
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from run_under_wine import main  # noqa: E402


def run_with_stale_log(directory: Path, extra_arguments: list[str]) -> int:
    call_log_dir = directory / "call-logs"
    call_log_dir.mkdir()
    (call_log_dir / "old-1-0123456789abcdef.log").write_text("D3D11CreateDevice\n")
    arguments = ["--arch", "win64", "--program", str(directory / "missing.exe")]
    arguments += ["--work-dir", str(directory / "work"), "--call-log-dir", str(call_log_dir)]
    environment = {"DIRECTMETAL_WINE": str(directory)}
    with mock.patch.dict(os.environ, environment, clear=True), redirect_stderr(StringIO()):
        return main(arguments + extra_arguments)


def fake_wine(directory: Path) -> None:
    wine = directory / "bin" / "wine"
    wine.parent.mkdir()
    wine.write_text("#!/bin/sh\nexit 1\n")
    wine.chmod(0o755)


class CallLogDirectoryTest(unittest.TestCase):
    def test_missing_wine_leaves_no_old_logs(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            directory = Path(name)
            self.assertEqual(run_with_stale_log(directory, []), 1)
            self.assertEqual(list((directory / "call-logs").iterdir()), [])

    def test_missing_unix_library_leaves_no_old_logs(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            directory = Path(name)
            fake_wine(directory)
            unix_library = ["--unix-library", str(directory / "missing.so")]
            self.assertEqual(run_with_stale_log(directory, unix_library), 1)
            self.assertEqual(list((directory / "call-logs").iterdir()), [])


if __name__ == "__main__":
    unittest.main()
