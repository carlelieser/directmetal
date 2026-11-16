import json
import unittest

from fixture import FixtureTestCase


class ReportOutputTest(FixtureTestCase):
    def test_rejects_inventory_with_wrong_shape(self) -> None:
        cases = [
            ({"functions": {}}, "missing 'interfaces'"),
            ({"functions": [], "interfaces": {}}, "'functions' must be an object"),
            ({"functions": {}, "interfaces": {"IFoo": {"base": None}}}, "'IFoo' missing 'members'"),
            ({"functions": {}, "interfaces": {"IFoo": {"base": 1, "members": []}}}, "'IFoo' base"),
            ({"functions": {}, "interfaces": {"IFoo": {"base": None, "members": [{}]}}}, "member"),
        ]
        for document, expected in cases:
            with self.subTest(expected=expected):
                self.fixture.write("inventory/d3d12.json", json.dumps(document))
                stderr = self.run_failing()
                self.assertIn("load inventory", stderr)
                self.assertIn("d3d12.json", stderr)
                self.assertIn(expected, stderr)

    def test_rejects_unwritable_output(self) -> None:
        self.fixture.write("blocked", "")
        stderr = self.run_failing("--output", str(self.fixture.root / "blocked"))
        self.assertIn("write report", stderr)
        self.assertIn("blocked", stderr)

    def test_markdown_report_lists_counts_and_methods(self) -> None:
        self.fixture.write("src/device.cpp", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }')
        self.fixture.write("logs/run.log", "ID3D11Device::CreateBuffer\n")
        self.run_ok("--call-log-dir", str(self.fixture.logs))
        report = (self.fixture.output / "d3d11.md").read_text(encoding="utf-8")
        self.assertIn("| Implemented | 1 | 20.0% |", report)
        self.assertIn("| Missing | 4 | 80.0% |", report)
        self.assertIn("| Tested | 1 | 20.0% |", report)
        self.assertIn("| `CreateBuffer` | implemented | yes | `device.cpp:1` |", report)
        summary = (self.fixture.output / "SUMMARY.md").read_text(encoding="utf-8")
        self.assertIn("| [d3d11](d3d11.md) | 1 | 0 | 4 | 1 | 5 |", summary)


if __name__ == "__main__":
    unittest.main()
