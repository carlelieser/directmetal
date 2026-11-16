"""Runs a PE test program under Wine with DirectMetal staged as out-of-tree builtins.

Each run gets a fresh directory under --work-dir holding the program with its app-local DLLs, a
new WINEPREFIX, and a WINEDLLPATH in Wine's <dir>/<arch>-windows and <dir>/x86_64-unix layout, so
Wine pairs directmetal.dll with directmetal.so. Fails unless the program exits with 0 and every
expected line appears on stderr. The run directory is deleted on success and kept on failure.

With --call-log-dir, that directory is emptied and the program writes its call logs there through
DIRECTMETAL_CALL_LOG_DIR, set to the directory's Windows path from winepath -w.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

PE_DIRS = {"win64": "x86_64-windows", "win32": "i386-windows"}
UNIX_DIR = "x86_64-unix"
DLL_OVERRIDES = "d3d11=n;directmetal=b;mscoree,mshtml="
TIMEOUT_SECONDS = 150


class SetupError(Exception):
    pass


@dataclass(frozen=True)
class Staging:
    root: Path
    app_dir: Path
    dll_path: Path
    prefix: Path
    call_log_dir: Path | None


def find_wine() -> Path:
    root = os.environ.get("DIRECTMETAL_WINE")
    if root:
        wine = Path(root, "bin", "wine")
        if not os.access(wine, os.X_OK):
            raise SetupError(f"DIRECTMETAL_WINE={root} has no executable bin/wine")
        return wine
    found = shutil.which("wine")
    if found is None:
        raise SetupError("wine not found; put it on PATH or set DIRECTMETAL_WINE to its root")
    return Path(found)


def copy_into(directory: Path, files: list[Path]) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for file in files:
        if not file.is_file():
            raise SetupError(f"missing build output {file}")
        shutil.copy2(file, directory / file.name)


@dataclass(frozen=True)
class TestFiles:
    program: Path
    dlls: list[Path]
    builtins: list[Path]
    unix_library: Path | None
    call_log_dir: Path | None


def reset_directory(directory: Path) -> Path:
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    return directory.resolve()


def stage(files: TestFiles, arch: str, work_dir: Path) -> Staging:
    if files.unix_library is not None and not files.unix_library.is_file():
        raise SetupError(f"missing {files.unix_library}; build the unix target first")
    work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="run-", dir=work_dir.resolve()))
    call_log_dir = files.call_log_dir and files.call_log_dir.resolve()
    staging = Staging(root, root / "app", root / "dlls", root / "prefix", call_log_dir)
    copy_into(staging.app_dir, [files.program, *files.dlls])
    copy_into(staging.dll_path / PE_DIRS[arch], files.builtins)
    if files.unix_library is not None:
        copy_into(staging.dll_path / UNIX_DIR, [files.unix_library])
    return staging


def wine_environment(staging: Staging) -> dict[str, str]:
    environment = dict(os.environ)
    environment.update(
        WINEPREFIX=str(staging.prefix),
        WINEDLLPATH=str(staging.dll_path),
        WINEDLLOVERRIDES=DLL_OVERRIDES,
        MVK_CONFIG_LOG_LEVEL="1",
    )
    environment.setdefault("WINEDEBUG", "-all")
    return environment


def windows_path(wine: Path, environment: dict[str, str], path: Path) -> str:
    result = subprocess.run(
        [str(wine.with_name("winepath")), "-w", str(path)],
        env=environment,
        capture_output=True,
        text=True,
        timeout=TIMEOUT_SECONDS,
    )
    converted = result.stdout.strip()
    if result.returncode != 0 or not converted:
        raise SetupError(f"winepath -w {path} failed: {result.stderr.strip()}")
    return converted


def run_program(wine: Path, staging: Staging, command: list[str]) -> subprocess.CompletedProcess:
    environment = wine_environment(staging)
    try:
        if staging.call_log_dir is not None:
            log_dir = windows_path(wine, environment, staging.call_log_dir)
            environment["DIRECTMETAL_CALL_LOG_DIR"] = log_dir
        return subprocess.run(
            [str(wine), str(staging.app_dir / command[0]), *command[1:]],
            env=environment,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=TIMEOUT_SECONDS,
        )
    finally:
        stop_wineserver(wine, environment)


def stop_wineserver(wine: Path, environment: dict[str, str]) -> None:
    wineserver = str(wine.with_name("wineserver"))
    subprocess.run([wineserver, "-k"], env=environment, check=False)
    subprocess.run([wineserver, "-w"], env=environment, check=False, timeout=TIMEOUT_SECONDS)


def failures(result: subprocess.CompletedProcess, expected_lines: list[str]) -> list[str]:
    found = set(result.stderr.splitlines())
    problems = [f"missing stderr line: {line}" for line in expected_lines if line not in found]
    if result.returncode != 0:
        problems.insert(0, f"exit status {result.returncode}, expected 0")
    return problems


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--arch", choices=sorted(PE_DIRS), required=True)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--dll", type=Path, action="append", default=[], help="app-local DLL")
    parser.add_argument("--builtin", type=Path, action="append", default=[], help="builtin DLL")
    parser.add_argument("--unix-library", type=Path, help="omit to test without the unix side")
    parser.add_argument("--program-arg", action="append", default=[], help="program argument")
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--call-log-dir", type=Path, help="emptied, then receives call logs")
    parser.add_argument("--expect", action="append", default=[], help="required stderr line")
    return parser.parse_args(arguments)


def main(arguments: list[str]) -> int:
    options = parse_arguments(arguments)
    # Emptied before anything can fail, so a failed setup never leaves earlier logs for the gate.
    if options.call_log_dir is not None:
        reset_directory(options.call_log_dir)
    try:
        wine = find_wine()
        files = TestFiles(
            options.program,
            options.dll,
            options.builtin,
            options.unix_library,
            options.call_log_dir,
        )
        staging = stage(files, options.arch, options.work_dir)
        result = run_program(wine, staging, [options.program.name, *options.program_arg])
    except SetupError as error:
        print(f"run_under_wine: {error}", file=sys.stderr)
        return 1
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    problems = failures(result, options.expect)
    for problem in problems:
        print(f"run_under_wine: {problem}", file=sys.stderr)
    if problems:
        print(f"run_under_wine: kept {staging.root}", file=sys.stderr)
        return 1
    shutil.rmtree(staging.root)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
