import subprocess
import unittest

from fixture import FixtureTestCase


class RequirementTest(FixtureTestCase):
    def setUp(self) -> None:
        super().setUp()
        self.fixture.write("src/device.cpp", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }')
        self.fixture.write("logs/run.log", "ID3D11Device::CreateBuffer\nD3D11CreateDevice\n")

    def check(self, *requirements: str) -> subprocess.CompletedProcess:
        arguments = ["--call-log-dir", str(self.fixture.logs)]
        for requirement in requirements:
            arguments += ["--require", requirement]
        return self.fixture.run(*arguments)

    def test_passes_when_every_threshold_holds(self) -> None:
        result = self.check("d3d11:implemented>=1", "d3d11:tested>=40%", "d3d11:stub<=0")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_fails_naming_each_unmet_threshold(self) -> None:
        result = self.check("d3d11:implemented>=2", "d3d11:tested>=50%", "d3d11:missing<=3")
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("d3d11:implemented>=2 (actual 1)", result.stderr)
        self.assertIn("d3d11:tested>=50% (actual 40.0%)", result.stderr)
        self.assertIn("d3d11:missing<=3 (actual 4)", result.stderr)
        self.assertTrue((self.fixture.output / "status.json").is_file())

    def test_rejects_malformed_requirement(self) -> None:
        self.assertIn(
            "parse requirement 'd3d11:tested>'", self.run_failing("--require", "d3d11:tested>")
        )
        self.assertIn("metric must be one of", self.run_failing("--require", "d3d11:passing>=1"))
        self.assertIn("unknown group 'd3d13'", self.run_failing("--require", "d3d13:tested>=1"))


if __name__ == "__main__":
    unittest.main()
