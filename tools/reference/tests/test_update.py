import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import sources  # noqa: E402
import update  # noqa: E402

RUN_ID = "45186840906"
COMMIT = "ac6262bf791cd1cfdff65869660c48587ad78339"
REPOSITORY = "owner/project"
RUN = {
    "conclusion": "success",
    "headSha": COMMIT,
    "url": f"https://github.com/{REPOSITORY}/actions/runs/{RUN_ID}",
    "workflowName": "Reference",
}
METADATA = {"run_id": RUN_ID, "repository": REPOSITORY, "commit": COMMIT}


class ParseRunTest(unittest.TestCase):
    def test_adds_repository_from_url(self) -> None:
        run = update.parse_run(RUN_ID, json.dumps(RUN))
        self.assertEqual(run["repository"], REPOSITORY)

    def test_rejects_unsuccessful_run(self) -> None:
        text = json.dumps({**RUN, "conclusion": "failure"})
        with self.assertRaisesRegex(update.ReferenceToolError, "concluded failure"):
            update.parse_run(RUN_ID, text)

    def test_rejects_other_workflow(self) -> None:
        text = json.dumps({**RUN, "workflowName": "CI"})
        with self.assertRaisesRegex(update.ReferenceToolError, "is a CI run"):
            update.parse_run(RUN_ID, text)

    def test_rejects_missing_field(self) -> None:
        text = json.dumps({key: value for key, value in RUN.items() if key != "headSha"})
        with self.assertRaisesRegex(update.ReferenceToolError, "lacks headSha"):
            update.parse_run(RUN_ID, text)


class CheckMetadataTest(unittest.TestCase):
    def setUp(self) -> None:
        self.run = update.parse_run(RUN_ID, json.dumps(RUN))

    def test_accepts_metadata_of_the_run(self) -> None:
        update.check_metadata(METADATA, RUN_ID, self.run)

    def test_rejects_other_run(self) -> None:
        metadata = {**METADATA, "run_id": "1"}
        with self.assertRaisesRegex(update.ReferenceToolError, "records run 1"):
            update.check_metadata(metadata, RUN_ID, self.run)

    def test_rejects_other_commit(self) -> None:
        metadata = {**METADATA, "commit": "0" * 40}
        with self.assertRaisesRegex(update.ReferenceToolError, "records commit 0000"):
            update.check_metadata(metadata, RUN_ID, self.run)

    def test_rejects_other_repository(self) -> None:
        metadata = {**METADATA, "repository": "fork/project"}
        with self.assertRaisesRegex(update.ReferenceToolError, "records repository fork/project"):
            update.check_metadata(metadata, RUN_ID, self.run)


class DescribeUpdateTest(unittest.TestCase):
    def test_words_changes_from_previous_to_new_reference(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "reference.json")
            previous = {"metadata": METADATA, "result": {"kept": 1, "dropped": 2, "changed": 3}}
            path.write_text(json.dumps(previous))
            reference = {"metadata": METADATA, "result": {"kept": 1, "added": 4, "changed": 5}}
            self.assertEqual(
                update.describe_update(path, reference),
                ["added: added", "changed: was 3, now 5", "dropped: removed"],
            )


class BuildReferenceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root)
        self.write("tests/api/programs/clear.cpp", "int main() {}\n")
        self.write("tests/api/harness/output.cpp", "// harness\n")
        self.write("artifact/metadata.json", json.dumps(METADATA))
        digests = sources.source_digests(self.root, "clear")
        self.write("artifact/clear/sources.json", json.dumps(digests))
        self.write("artifact/clear/x64.json", json.dumps({"create_device": "0x00000000"}))
        self.write("artifact/clear/x86.json", json.dumps({"create_device": "0x00000000"}))

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def build(self) -> dict:
        return update.build_reference(self.root, self.root / "artifact", "clear")

    def test_builds_reference_from_matching_sources(self) -> None:
        reference = self.build()
        self.assertEqual(reference["result"], {"create_device": "0x00000000"})
        self.assertEqual(reference["metadata"], METADATA)

    def test_refuses_sources_that_differ_from_the_run(self) -> None:
        self.write("tests/api/programs/clear.cpp", "int main() { return 1; }\n")
        with self.assertRaisesRegex(update.ReferenceToolError, "sources in this checkout differ"):
            self.build()

    def test_refuses_x86_output_that_differs(self) -> None:
        self.write("artifact/clear/x86.json", json.dumps({"create_device": "0x80004005"}))
        with self.assertRaisesRegex(update.ReferenceToolError, "x86 output differs"):
            self.build()


if __name__ == "__main__":
    unittest.main()
