import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_fresh  # noqa: E402
import sources  # noqa: E402


class CheckFreshTest(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root)
        self.write("tests/api/programs/manifest.tsv", "program\nclear\n")
        self.write("tests/api/programs/clear.cpp", "int main() {}\n")
        self.write("tests/api/harness/output.cpp", "// harness\n")
        self.write(
            "tests/api/harness/include/directmetal/api_harness/harness.hpp", "#pragma once\n"
        )

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode())

    def record(self) -> None:
        digests = sources.source_digests(self.root, "clear")
        self.write("tests/reference/clear/reference.json", json.dumps({"sources": digests}))

    def check(self) -> int:
        return check_fresh.main(["--root", str(self.root)])

    def test_passes_when_sources_match_reference(self) -> None:
        self.record()
        self.assertEqual(self.check(), 0)

    def test_fails_when_program_changes(self) -> None:
        self.record()
        self.write("tests/api/programs/clear.cpp", "int main() { return 1; }\n")
        self.assertEqual(self.check(), check_fresh.EXIT_STALE)

    def test_fails_when_harness_file_is_added(self) -> None:
        self.record()
        self.write("tests/api/harness/process_native.cpp", "// new\n")
        added = "tests/api/harness/process_native.cpp"
        self.assertEqual(
            check_fresh.staleness(self.root, "clear"),
            [f"clear: {added} was added after the reference was recorded"],
        )

    def test_fails_when_harness_file_is_removed(self) -> None:
        self.record()
        (self.root / "tests/api/harness/output.cpp").unlink()
        self.assertEqual(self.check(), check_fresh.EXIT_STALE)

    def test_ignores_line_ending_changes(self) -> None:
        self.record()
        self.write("tests/api/programs/clear.cpp", "int main() {}\r\n")
        self.assertEqual(self.check(), 0)

    def test_fails_when_program_has_no_reference(self) -> None:
        self.assertEqual(
            check_fresh.staleness(self.root, "clear"),
            ["clear: no reference at tests/reference/clear/reference.json"],
        )

    def test_rejects_reference_without_sources(self) -> None:
        self.write("tests/reference/clear/reference.json", "{}")
        self.assertEqual(self.check(), check_fresh.EXIT_INVALID_INPUT)


if __name__ == "__main__":
    unittest.main()
