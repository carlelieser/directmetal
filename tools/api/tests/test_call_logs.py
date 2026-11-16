import unittest

from fixture import CALL_LOG_ENV, FixtureTestCase


class CallLogTest(FixtureTestCase):
    def test_merges_call_logs_across_files_and_directories(self) -> None:
        self.fixture.write("logs/run1/1234.log", "ID3D11Device::CreateBuffer\n\n")
        self.fixture.write("other/5678.log", "  D3D11CreateDevice  \nIUnknown::AddRef\n")
        self.run_ok(
            "--call-log-dir",
            str(self.fixture.logs),
            "--call-log-dir",
            str(self.fixture.root / "other"),
        )
        methods = self.fixture.methods()
        self.assertTrue(methods["ID3D11Device::CreateBuffer"]["is_tested"])
        self.assertTrue(methods["D3D11CreateDevice"]["is_tested"])
        self.assertFalse(methods["ID3D11Device::GetType"]["is_tested"])
        self.assertTrue(self.fixture.methods("common")["IUnknown::AddRef"]["is_tested"])

    def test_ignores_unterminated_final_call_log_line(self) -> None:
        self.fixture.write("logs/crashed.log", "ID3D11Device::GetType\nID3D11Dev")
        self.fixture.write("logs/partial.log", "D3D11CreateDevice")
        self.run_ok("--call-log-dir", str(self.fixture.logs))
        methods = self.fixture.methods()
        self.assertTrue(methods["ID3D11Device::GetType"]["is_tested"])
        self.assertFalse(methods["D3D11CreateDevice"]["is_tested"])

    def test_reads_call_log_directory_from_environment(self) -> None:
        self.fixture.write("logs/run.log", "ID3D11Device::GetType\n")
        self.run_ok(environment={CALL_LOG_ENV: str(self.fixture.logs)})
        self.assertTrue(self.fixture.methods()["ID3D11Device::GetType"]["is_tested"])

    def test_rejects_unknown_method_in_call_log_with_file_and_line(self) -> None:
        self.fixture.write("logs/run.log", "ID3D11Device::CreateBuffer\nID3D11Device::Bogus\n")
        stderr = self.run_failing("--call-log-dir", str(self.fixture.logs))
        self.assertIn("read call log", stderr)
        self.assertIn("run.log:2", stderr)
        self.assertIn("'ID3D11Device' declares no method 'Bogus'", stderr)

    def test_rejects_unknown_interface_and_function(self) -> None:
        self.fixture.write("logs/run.log", "IFoo::Bar\n")
        self.assertIn(
            "unknown interface 'IFoo'", self.run_failing("--call-log-dir", str(self.fixture.logs))
        )
        self.fixture.write("logs/run.log", "CreateNothing\n")
        self.assertIn(
            "unknown exported function 'CreateNothing'",
            self.run_failing("--call-log-dir", str(self.fixture.logs)),
        )

    def test_rejects_missing_directories(self) -> None:
        missing = str(self.fixture.root / "absent")
        self.assertIn(f"directory not found: {missing}", self.run_failing("--sources", missing))
        self.assertIn(
            f"directory not found: {missing}", self.run_failing("--call-log-dir", missing)
        )


if __name__ == "__main__":
    unittest.main()
