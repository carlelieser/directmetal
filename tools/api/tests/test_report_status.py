import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "report_status.py"
CALL_LOG_ENV = "DIRECTMETAL_CALL_LOG_DIR"
GROUPS = ["d3d9", "d3d10", "d3d11", "d3d12", "dxgi", "common"]


def interface(base: str, *methods: str) -> dict:
    return {"base": base, "members": [{"name": method} for method in methods]}


INVENTORY = {
    "d3d11": {
        "functions": {"D3D11CreateDevice": "d3d11.idl"},
        "interfaces": {
            "ID3D11Device": interface("IUnknown", "CreateBuffer", "CreateTexture2D", "GetType"),
            "ID3D11Device1": interface("ID3D11Device", "CreateDeferredContext1"),
        },
    },
}


class StatusFixture:
    """A temporary inventory, front-end source tree and call-log directory."""

    def __init__(self, root: Path) -> None:
        self.root = root
        self.sources = root / "src"
        self.logs = root / "logs"
        self.output = root / "out"
        self.sources.mkdir()
        self.logs.mkdir()
        for group in GROUPS:
            document = INVENTORY.get(group, {"functions": {}, "interfaces": {}})
            self.write(f"inventory/{group}.json", json.dumps(document))

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def run(self, *extra: str, environment: dict | None = None) -> subprocess.CompletedProcess:
        command = [sys.executable, "-I", str(SCRIPT), "--inventory", str(self.root / "inventory")]
        command += ["--sources", str(self.sources), "--output", str(self.output), *extra]
        env = {name: value for name, value in os.environ.items() if name != CALL_LOG_ENV}
        env.update(environment or {})
        return subprocess.run(command, capture_output=True, text=True, env=env, check=False)

    def methods(self, group: str = "d3d11") -> dict:
        document = json.loads((self.output / "status.json").read_text(encoding="utf-8"))
        return document["groups"][group]["methods"]


class FixtureTestCase(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.fixture = StatusFixture(Path(directory.name))

    def run_ok(self, *extra: str, environment: dict | None = None) -> subprocess.CompletedProcess:
        result = self.fixture.run(*extra, environment=environment)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result

    def run_failing(self, *extra: str) -> str:
        result = self.fixture.run(*extra)
        self.assertEqual(result.returncode, 2, result.stdout)
        return result.stderr


class ReportStatusTest(FixtureTestCase):
    def test_classifies_implemented_stub_and_missing(self) -> None:
        source = [
            'HRESULT Device::CreateBuffer() { DM_IMPL("ID3D11Device::CreateBuffer"); }',
            'UINT Device::GetType() { DM_STUB_RETURN("ID3D11Device::GetType", 0); }',
            'HRESULT D3D11CreateDevice() { DM_STUB("D3D11CreateDevice"); }',
            'void Device::CreateTexture2D() { DM_STUB_VOID("ID3D11Device::CreateTexture2D"); }',
        ]
        self.fixture.write("src/d3d11/device.cpp", "\n".join(source))
        self.run_ok()
        methods = self.fixture.methods()
        self.assertEqual(methods["ID3D11Device::CreateBuffer"]["state"], "implemented")
        self.assertEqual(methods["ID3D11Device::CreateBuffer"]["location"], "d3d11/device.cpp:1")
        self.assertEqual(methods["ID3D11Device::GetType"]["state"], "stub")
        self.assertEqual(methods["D3D11CreateDevice"]["state"], "stub")
        self.assertEqual(methods["ID3D11Device::CreateTexture2D"]["state"], "stub")
        self.assertEqual(methods["ID3D11Device1::CreateDeferredContext1"]["state"], "missing")

    def test_ignores_markers_in_comments(self) -> None:
        source = '// DM_IMPL("ID3D11Device::CreateBuffer")\n/* DM_STUB("D3D11CreateDevice") */\n'
        self.fixture.write("src/device.cpp", source)
        self.run_ok()
        methods = self.fixture.methods()
        self.assertEqual(methods["ID3D11Device::CreateBuffer"]["state"], "missing")
        self.assertEqual(methods["D3D11CreateDevice"]["state"], "missing")

    def test_comment_openers_inside_literals_do_not_hide_markers(self) -> None:
        source = [
            'const char* pattern = "shaders/*.metal";',
            """const char quote = '"'; const char* glob = "a/*b";""",
            "HRESULT Device::CreateBuffer() {",
            '    DM_IMPL("ID3D11Device::CreateBuffer");',
            "}",
            'const char* url = "see http://example.com"; /* trailing */',
            'void D3D11CreateDevice() { DM_STUB_VOID("D3D11CreateDevice"); }',
        ]
        self.fixture.write("src/device.cpp", "\n".join(source))
        self.run_ok()
        methods = self.fixture.methods()
        self.assertEqual(methods["ID3D11Device::CreateBuffer"]["state"], "implemented")
        self.assertEqual(methods["D3D11CreateDevice"]["location"], "device.cpp:7")

    def test_rejects_marker_that_is_not_first_statement_of_a_body(self) -> None:
        cases = {
            "helper.cpp": 'void f() { log(); DM_IMPL("ID3D11Device::CreateBuffer"); }',
            "raw.cpp": 'const char* text = R"(DM_IMPL("ID3D11Device::CreateBuffer"))";',
            "file_scope.cpp": '\nDM_STUB("D3D11CreateDevice");',
        }
        for name, source in cases.items():
            with self.subTest(name=name):
                self.fixture.write(f"src/{name}", source)
                stderr = self.run_failing()
                line = 2 if name == "file_scope.cpp" else 1
                self.assertIn(f"{name}:{line}", stderr)
                self.assertIn("first statement", stderr)
                (self.fixture.sources / name).unlink()

    def test_accepts_marker_after_brace_and_whitespace(self) -> None:
        self.fixture.write(
            "src/device.cpp", 'HRESULT f()\n{\n\t  DM_IMPL("ID3D11Device::CreateBuffer");\n}'
        )
        self.run_ok()
        self.assertEqual(
            self.fixture.methods()["ID3D11Device::CreateBuffer"]["location"], "device.cpp:3"
        )

    def test_stub_marker_outranks_implementation_in_either_order(self) -> None:
        self.fixture.write("src/a.cpp", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }')
        self.fixture.write("src/b.cpp", 'void f() { DM_STUB("ID3D11Device::CreateBuffer"); }')
        self.fixture.write("src/c.cpp", 'void f() { DM_STUB_RETURN("ID3D11Device::GetType", 0); }')
        self.fixture.write("src/d.cpp", 'void f() { DM_IMPL("ID3D11Device::GetType"); }')
        self.run_ok()
        methods = self.fixture.methods()
        create_buffer = methods["ID3D11Device::CreateBuffer"]
        self.assertEqual((create_buffer["state"], create_buffer["location"]), ("stub", "b.cpp:1"))
        get_type = methods["ID3D11Device::GetType"]
        self.assertEqual((get_type["state"], get_type["location"]), ("stub", "c.cpp:1"))

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

    def test_rejects_inherited_key_in_source_naming_declaring_interface(self) -> None:
        self.fixture.write(
            "src/device.cpp", '\n\nvoid f() { DM_IMPL("ID3D11Device1::CreateBuffer"); }'
        )
        stderr = self.run_failing()
        self.assertIn("device.cpp:3", stderr)
        self.assertIn("use 'ID3D11Device::CreateBuffer'", stderr)

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
