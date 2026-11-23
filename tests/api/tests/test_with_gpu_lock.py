import os
import shutil
import stat
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[3] / "tools" / "with_gpu_lock.sh"
DEFAULT_LOCK = Path("/Users/Shared/directmetal/gpu.lock")
TIMEOUT_SECONDS = 30


def environment(lock_file: Path | None) -> dict[str, str]:
    variables = {
        name: value for name, value in os.environ.items() if name != "DIRECTMETAL_GPU_LOCK"
    }
    if lock_file is not None:
        variables["DIRECTMETAL_GPU_LOCK"] = str(lock_file)
    return variables


def permissions(path: Path) -> int:
    return stat.S_IMODE(path.stat().st_mode)


class GpuLockTest(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.root)

    def locked(self, lock_file: Path, command: str) -> subprocess.Popen:
        # The umask of a restrictive account must not keep other accounts from opening the lock.
        shell = f'umask 077; exec "$0" sh -c {command!r}'
        return subprocess.Popen(
            ["sh", "-c", shell, str(SCRIPT)],
            env=environment(lock_file),
            stderr=subprocess.DEVNULL,
        )

    def test_creates_lock_readable_by_other_accounts(self) -> None:
        lock_file = self.root / "missing" / "gpu.lock"
        self.assertEqual(self.locked(lock_file, "true").wait(TIMEOUT_SECONDS), 0)
        self.assertEqual(permissions(lock_file), 0o644)
        self.assertEqual(permissions(lock_file.parent), 0o755)

    def test_locks_through_read_only_lock_file(self) -> None:
        lock_file = self.root / "gpu.lock"
        lock_file.write_text("")
        lock_file.chmod(0o444)
        self.assertEqual(self.locked(lock_file, "true").wait(TIMEOUT_SECONDS), 0)

    def test_serializes_concurrent_runs(self) -> None:
        lock_file, log = self.root / "gpu.lock", self.root / "log"
        first = self.locked(
            lock_file, f"echo first-start >> {log}; sleep 1; echo first-end >> {log}"
        )
        deadline = time.monotonic() + TIMEOUT_SECONDS
        while not (log.exists() and log.read_text()) and time.monotonic() < deadline:
            time.sleep(0.05)
        second = self.locked(lock_file, f"echo second-start >> {log}; echo second-end >> {log}")
        self.assertEqual(first.wait(TIMEOUT_SECONDS), 0)
        self.assertEqual(second.wait(TIMEOUT_SECONDS), 0)
        expected = ["first-start", "first-end", "second-start", "second-end"]
        self.assertEqual(log.read_text().split(), expected)

    def test_defaults_to_lock_shared_by_all_accounts(self) -> None:
        # The default lock may be held by the run executing this test, so only the announcement
        # is awaited; the waiting process is then stopped.
        process = subprocess.Popen(
            [str(SCRIPT), "true"], env=environment(None), stderr=subprocess.PIPE, text=True
        )
        try:
            announcement = process.stderr.readline().strip()
        finally:
            process.kill()
            process.wait(TIMEOUT_SECONDS)
            process.stderr.close()
        self.assertEqual(announcement, f"with_gpu_lock.sh: waiting for {DEFAULT_LOCK}")
        self.assertEqual(permissions(DEFAULT_LOCK) & 0o044, 0o044)


if __name__ == "__main__":
    unittest.main()
