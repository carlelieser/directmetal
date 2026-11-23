"""Runs the API test programs on Windows and writes the reference artifact.

Each program in tests/api/programs/manifest.tsv runs once per build from its own empty directory,
so it loads the system d3d11.dll. Every build must produce the same output. The output directory
receives metadata.json and, per program, <arch>.json and sources.json.
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from compare import differences  # noqa: E402
from sources import REPO_ROOT, ReferenceToolError, read_manifest, source_digests  # noqa: E402

PROGRAM_SUBDIR = Path("tests", "api", "programs")
REQUIRED_ARCHES = ("x64", "x86")
TIMEOUT_SECONDS = 120
# System DLLs each architecture's process loads; SysWOW64 holds the x86 ones.
SYSTEM_DLL_DIRS = {"x64": "System32", "x86": "SysWOW64"}


def parse_build(text: str) -> tuple[str, Path]:
    arch, separator, directory = text.partition("=")
    if not separator or arch not in REQUIRED_ARCHES:
        raise argparse.ArgumentTypeError(f"expected ARCH=DIR with ARCH in {REQUIRED_ARCHES}")
    return arch, Path(directory)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", type=parse_build, action="append", required=True)
    parser.add_argument("--driver", default="warp", choices=["warp", "hardware"])
    parser.add_argument("--toolchain", required=True, help="compiler release, for metadata")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    options = parser.parse_args(argv)
    options.build = dict(options.build)
    if sorted(options.build) != sorted(REQUIRED_ARCHES):
        parser.error(f"--build must name each of {REQUIRED_ARCHES} once")
    return options


def run_program(executable: Path, driver: str) -> dict:
    if not executable.is_file():
        raise ReferenceToolError(f"missing program {executable}")
    with tempfile.TemporaryDirectory(prefix="reference-") as directory:
        staged = Path(directory, executable.name)
        shutil.copy2(executable, staged)
        result = subprocess.run(
            [str(staged), "--driver", driver],
            cwd=directory,
            capture_output=True,
            timeout=TIMEOUT_SECONDS,
        )
    sys.stderr.write(result.stderr.decode("utf-8", "replace"))
    if result.returncode != 0:
        raise ReferenceToolError(
            f"{executable} --driver {driver} exited with {result.returncode:#x}: "
            f"{result.stdout.decode('utf-8', 'replace')}"
        )
    try:
        return json.loads(result.stdout)
    except ValueError as error:
        raise ReferenceToolError(f"{executable} printed invalid JSON: {error}") from error


def write_json(path: Path, document: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def record_program(options: argparse.Namespace, program: str) -> list[str]:
    outputs = {}
    for arch, build_dir in sorted(options.build.items()):
        executable = build_dir / PROGRAM_SUBDIR / f"{program}.exe"
        outputs[arch] = run_program(executable, options.driver)
        write_json(options.output / program / f"{arch}.json", outputs[arch])
    write_json(options.output / program / "sources.json", source_digests(options.root, program))
    return [
        f"{program}: x86 differs from x64: {problem}"
        for problem in differences(outputs["x64"], outputs["x86"], None)
    ]


def powershell(command: str) -> str:
    result = subprocess.run(
        ["powershell", "-NoProfile", "-NonInteractive", "-Command", command],
        capture_output=True,
        text=True,
        timeout=TIMEOUT_SECONDS,
    )
    if result.returncode != 0:
        raise ReferenceToolError(f"powershell {command!r} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def file_version(path: Path) -> str:
    return powershell(
        f"$info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo('{path}'); "
        "'{0}.{1}.{2}.{3}' -f $info.FileMajorPart, $info.FileMinorPart, "
        "$info.FileBuildPart, $info.FilePrivatePart"
    )


def windows_build() -> dict[str, str]:
    import winreg

    key_path = r"SOFTWARE\Microsoft\Windows NT\CurrentVersion"
    names = ["ProductName", "DisplayVersion", "CurrentBuild", "UBR"]
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path) as key:
        values = {name: str(winreg.QueryValueEx(key, name)[0]) for name in names}
    values["version"] = platform.version()
    return values


def metadata(options: argparse.Namespace, programs: list[str]) -> dict:
    system_root = Path(os.environ.get("SystemRoot", r"C:\Windows"))
    environment = os.environ.get
    return {
        "run_id": environment("GITHUB_RUN_ID"),
        "run_attempt": environment("GITHUB_RUN_ATTEMPT"),
        "repository": environment("GITHUB_REPOSITORY"),
        "ref": environment("GITHUB_REF"),
        "commit": environment("GITHUB_SHA"),
        "runner_image": {"os": environment("ImageOS"), "version": environment("ImageVersion")},
        "windows_build": windows_build(),
        "d3d11_version": {
            arch: file_version(system_root / directory / "d3d11.dll")
            for arch, directory in SYSTEM_DLL_DIRS.items()
        },
        "driver": options.driver,
        "toolchain": options.toolchain,
        "programs": programs,
    }


def main(argv: list[str]) -> int:
    options = parse_arguments(argv)
    try:
        programs = read_manifest(options.root)
        problems = [problem for program in programs for problem in record_program(options, program)]
        write_json(options.output / "metadata.json", metadata(options, programs))
    except (ReferenceToolError, subprocess.TimeoutExpired, OSError) as error:
        print(f"run_reference: {error}", file=sys.stderr)
        return 1
    for problem in problems:
        print(f"run_reference: {problem}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
