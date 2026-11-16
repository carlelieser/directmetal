import unittest

from fixture import FixtureTestCase

STUBBED_CONTEXT = 'void f() { DM_STUB("ID3D11Device1::CreateDeferredContext1"); }'


class ExclusionTest(FixtureTestCase):
    def exclude(self, *lines: str) -> None:
        self.fixture.write("out_of_scope.tsv", "\n".join(["# key\treason", "", *lines]) + "\n")

    def test_reports_excluded_keys_and_interfaces_with_reason(self) -> None:
        self.exclude("D3D11CreateDevice\tWine provides it", "ID3D11Device1\tvideo only")
        self.fixture.write("src/context.cpp", STUBBED_CONTEXT)
        self.run_ok()
        methods = self.fixture.methods()
        function = methods["D3D11CreateDevice"]
        self.assertEqual(
            (function["state"], function["exclusion_reason"]), ("excluded", "Wine provides it")
        )
        context = methods["ID3D11Device1::CreateDeferredContext1"]
        self.assertEqual((context["state"], context["location"]), ("excluded", "context.cpp:1"))
        self.assertIsNone(methods["ID3D11Device::GetType"]["exclusion_reason"])

    def test_shares_are_computed_over_in_scope_entries(self) -> None:
        self.exclude("D3D11CreateDevice\tWine provides it")
        self.fixture.write("src/device.cpp", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }')
        self.fixture.write("logs/run.log", "ID3D11Device::CreateBuffer\nD3D11CreateDevice\n")
        self.run_ok("--call-log-dir", str(self.fixture.logs))
        report = (self.fixture.output / "d3d11.md").read_text(encoding="utf-8")
        self.assertIn("| Implemented | 1 | 25.0% |", report)
        self.assertIn("| Tested | 1 | 25.0% |", report)
        self.assertIn("| Excluded | 1 | — |", report)
        self.assertIn("| In scope | 4 | |", report)
        self.assertIn("| `D3D11CreateDevice` | excluded | yes | Wine provides it |", report)
        summary = (self.fixture.output / "SUMMARY.md").read_text(encoding="utf-8")
        self.assertIn("| [d3d11](d3d11.md) | 1 | 0 | 3 | 1 | 1 | 5 |", summary)

    def test_requirement_percentages_use_in_scope_entries(self) -> None:
        self.exclude("D3D11CreateDevice\tWine provides it")
        self.fixture.write("src/device.cpp", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }')
        self.run_ok("--require", "d3d11:implemented>=25%")

    def test_rejects_unknown_exclusion_with_file_and_line(self) -> None:
        self.exclude("ID3D11Device::Bogus\tnot real")
        stderr = self.run_failing()
        self.assertIn("read exclusions", stderr)
        self.assertIn("out_of_scope.tsv:3", stderr)
        self.assertIn("declares no method 'Bogus'", stderr)

    def test_rejects_exclusion_without_reason(self) -> None:
        self.exclude("D3D11CreateDevice")
        self.assertIn("out_of_scope.tsv:3: expected KEY<TAB>REASON", self.run_failing())

    def test_rejects_excluding_an_implemented_method(self) -> None:
        self.exclude("ID3D11Device\tlegacy")
        self.fixture.write("src/device.cpp", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }')
        stderr = self.run_failing()
        self.assertIn("'ID3D11Device::CreateBuffer' is implemented at device.cpp:1", stderr)

    def test_rejects_missing_exclusions_file(self) -> None:
        self.fixture.exclusions.unlink()
        self.assertIn("read exclusions", self.run_failing())


if __name__ == "__main__":
    unittest.main()
