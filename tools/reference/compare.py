"""Compares an API test program's output with its Windows reference.

HRESULTs and every other recorded value must match exactly. Readbacks are decoded per format and
each channel may differ by at most the format's tolerance from tolerance.json beside the reference.
"""

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from pixel_formats import bit_patterns, channel_difference, decode_pixels  # noqa: E402
from sources import ReferenceToolError  # noqa: E402

EXIT_MISMATCH = 1
EXIT_INVALID_INPUT = 2
MAX_PIXEL_REPORTS = 4
TOLERANCE_KEY = "max_channel_difference"


@dataclass(frozen=True)
class Location:
    path: str
    # Format of the enclosing case, which decides how its readbacks decode.
    format_name: str | None = None
    # Width of the enclosing mip, which places readback pixels.
    width: int | None = None

    def child(self, name: str) -> "Location":
        is_nested_key = bool(self.path) and not name.startswith("[")
        path = f"{self.path}.{name}" if is_nested_key else self.path + name
        return Location(path, self.format_name, self.width)

    def pixel(self, index: int) -> str:
        if self.width is None:
            return f"pixel {index}"
        return f"pixel ({index % self.width}, {index // self.width})"


class Comparator:
    def __init__(self, tolerances: dict[str, float] | None) -> None:
        """With tolerances None, every channel must match exactly."""
        for format_name, tolerance in (tolerances or {}).items():
            check_tolerance(format_name, tolerance)
        self.tolerances = tolerances

    def tolerance(self, format_name: str) -> float:
        if self.tolerances is None:
            return 0
        if format_name not in self.tolerances:
            raise ReferenceToolError(f"tolerance.json has no tolerance for format {format_name}")
        return self.tolerances[format_name]

    def compare(self, location: Location, expected: object, actual: object) -> list[str]:
        if isinstance(expected, dict) and isinstance(actual, dict):
            return self.compare_objects(location, expected, actual)
        if isinstance(expected, list) and isinstance(actual, list):
            return self.compare_lists(location, expected, actual)
        is_readback = location.path.endswith(".readback") and location.format_name is not None
        if is_readback and isinstance(expected, str) and isinstance(actual, str):
            return self.compare_readbacks(location, expected, actual)
        # JSON's false, 0 and 0.0 are distinct values although Python compares them equal.
        if type(expected) is type(actual) and expected == actual:
            return []
        return [f"{location.path}: expected {json.dumps(expected)}, got {json.dumps(actual)}"]

    def compare_objects(self, location: Location, expected: dict, actual: dict) -> list[str]:
        problems = []
        format_name = expected.get("format", location.format_name)
        scoped = Location(location.path, format_name, expected.get("width", location.width))
        for key in sorted(expected.keys() | actual.keys()):
            if key not in actual:
                problems.append(f"{scoped.child(key).path}: missing")
            elif key not in expected:
                problems.append(f"{scoped.child(key).path}: not in the reference")
            else:
                problems += self.compare(scoped.child(key), expected[key], actual[key])
        return problems

    def compare_lists(self, location: Location, expected: list, actual: list) -> list[str]:
        if len(expected) != len(actual):
            return [f"{location.path}: expected {len(expected)} entries, got {len(actual)}"]
        problems = []
        for index, (expected_item, actual_item) in enumerate(zip(expected, actual, strict=True)):
            name = expected_item.get("name", index) if isinstance(expected_item, dict) else index
            problems += self.compare(location.child(f"[{name}]"), expected_item, actual_item)
        return problems

    def compare_readbacks(self, location: Location, expected: str, actual: str) -> list[str]:
        format_name = location.format_name
        expected_pixels = decode_pixels(format_name, expected)
        actual_pixels = decode_pixels(format_name, actual)
        if len(expected_pixels) != len(actual_pixels):
            return [
                f"{location.path}: expected {len(expected_pixels)} pixels, got {len(actual_pixels)}"
            ]
        is_mismatch = self.pixel_mismatch(format_name, expected, actual)
        mismatches = [
            f"{location.path}: {location.pixel(index)} expected {list(expected_pixel)}, "
            f"got {list(actual_pixel)}"
            for index, (expected_pixel, actual_pixel) in enumerate(
                zip(expected_pixels, actual_pixels, strict=True)
            )
            if is_mismatch[index]
        ]
        if len(mismatches) > MAX_PIXEL_REPORTS:
            remaining = len(mismatches) - MAX_PIXEL_REPORTS
            mismatches = [*mismatches[:MAX_PIXEL_REPORTS], f"{location.path}: {remaining} more"]
        return mismatches

    def pixel_mismatch(self, format_name: str, expected: str, actual: str) -> list[bool]:
        tolerance = self.tolerance(format_name)
        if tolerance == 0:
            pairs = zip(
                bit_patterns(format_name, expected), bit_patterns(format_name, actual), strict=True
            )
            return [expected_bits != actual_bits for expected_bits, actual_bits in pairs]
        pairs = zip(
            decode_pixels(format_name, expected), decode_pixels(format_name, actual), strict=True
        )
        return [pixel_difference(*pixels) > tolerance for pixels in pairs]


def check_tolerance(format_name: str, tolerance: object) -> None:
    is_number = isinstance(tolerance, int | float) and not isinstance(tolerance, bool)
    if not is_number or not tolerance >= 0:
        raise ReferenceToolError(
            f"tolerance for format {format_name} is {json.dumps(tolerance)}, "
            "not a non-negative number"
        )


def pixel_difference(expected: tuple, actual: tuple) -> float:
    return max(channel_difference(*channels) for channels in zip(expected, actual, strict=True))


def differences(expected: dict, actual: dict, tolerances: dict[str, float] | None) -> list[str]:
    """Lists every difference between two program outputs, one line each."""
    return Comparator(tolerances).compare(Location(""), expected, actual)


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ReferenceToolError(f"reading JSON {path} failed: {error}") from error


def read_tolerances(path: Path) -> dict[str, float]:
    tolerances = read_json(path).get(TOLERANCE_KEY)
    if not isinstance(tolerances, dict):
        raise ReferenceToolError(f"{path} has no {TOLERANCE_KEY} object")
    return tolerances


def reference_result(path: Path) -> dict:
    result = read_json(path).get("result")
    if not isinstance(result, dict):
        raise ReferenceToolError(f"reference {path} has no result object")
    return result


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--reference", type=Path, required=True, help="reference.json")
    parser.add_argument("--actual", type=Path, required=True, help="program output")
    parser.add_argument("--tolerance", type=Path, help="default: tolerance.json beside reference")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    options = parse_arguments(argv)
    tolerance = options.tolerance or options.reference.with_name("tolerance.json")
    try:
        expected = reference_result(options.reference)
        problems = differences(expected, read_json(options.actual), read_tolerances(tolerance))
    except ReferenceToolError as error:
        print(f"compare: {error}", file=sys.stderr)
        return EXIT_INVALID_INPUT
    for problem in problems:
        print(f"compare: {problem}", file=sys.stderr)
    return EXIT_MISMATCH if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
