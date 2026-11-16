"""Runs report_status.py against temporary inventories, sources and call logs."""

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
