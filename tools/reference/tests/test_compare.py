import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import compare  # noqa: E402

TOLERANCES = {
    "R8G8B8A8_UNORM": 1,
    "R10G10B10A2_UNORM": 0,
    "R16G16B16A16_FLOAT": 0,
    "R32G32B32A32_FLOAT": 0,
    "R32G32B32A32_UINT": 0,
}


def packed(packing: str, pixels: list[tuple]) -> str:
    return b"".join(struct.pack(packing, *pixel) for pixel in pixels).hex()


def output(format_name: str, readback: str, map_result: str = "0x00000000") -> dict:
    return {
        "test": "program",
        "create_device": "0x00000000",
        "cases": [
            {
                "name": format_name,
                "format": format_name,
                "create_texture": "0x00000000",
                "mips": [{"mip": 0, "map": map_result, "readback": readback}],
            }
        ],
    }


def rgb10a2(red: int, green: int, blue: int, alpha: int) -> tuple:
    return (red | green << 10 | blue << 20 | alpha << 30,)


class DifferencesTest(unittest.TestCase):
    def assert_same(self, expected: dict, actual: dict) -> None:
        self.assertEqual(compare.differences(expected, actual, TOLERANCES), [])

    def assert_differs(self, expected: dict, actual: dict, fragment: str) -> None:
        problems = compare.differences(expected, actual, TOLERANCES)
        self.assertTrue(any(fragment in problem for problem in problems), problems)

    def test_accepts_unorm8_within_tolerance(self) -> None:
        reference = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)] * 4))
        actual = output("R8G8B8A8_UNORM", packed("4B", [(52, 101, 153, 204)] * 4))
        self.assert_same(reference, actual)

    def test_rejects_unorm8_beyond_tolerance(self) -> None:
        reference = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)] * 4))
        actual = output(
            "R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)] * 3 + [(53, 102, 153, 204)])
        )
        self.assert_differs(reference, actual, "pixel 3 expected [51, 102, 153, 204]")

    def test_decodes_packed_ten_bit_channels(self) -> None:
        reference = output("R10G10B10A2_UNORM", packed("<I", [rgb10a2(100, 500, 900, 2)]))
        actual = output("R10G10B10A2_UNORM", packed("<I", [rgb10a2(100, 500, 900, 3)]))
        self.assert_differs(
            reference, actual, "expected [100, 500, 900, 2], got [100, 500, 900, 3]"
        )

    def test_compares_half_floats_exactly(self) -> None:
        reference = output("R16G16B16A16_FLOAT", packed("<4e", [(0.25, -0.5, 1.5, 1.0)]))
        same = output("R16G16B16A16_FLOAT", packed("<4e", [(0.25, -0.5, 1.5, 1.0)]))
        changed = output("R16G16B16A16_FLOAT", packed("<4e", [(0.25, -0.5, 1.5, 0.99951171875)]))
        self.assert_same(reference, same)
        self.assert_differs(reference, changed, "pixel 0")

    def test_treats_matching_nans_as_equal(self) -> None:
        nan = float("nan")
        reference = output("R32G32B32A32_FLOAT", packed("<4f", [(nan, 1.0, 2.0, 3.0)]))
        self.assert_same(
            reference, output("R32G32B32A32_FLOAT", packed("<4f", [(nan, 1.0, 2.0, 3.0)]))
        )
        self.assert_differs(
            reference,
            output("R32G32B32A32_FLOAT", packed("<4f", [(0.0, 1.0, 2.0, 3.0)])),
            "pixel 0",
        )

    def test_rejects_uint_off_by_one(self) -> None:
        reference = output("R32G32B32A32_UINT", packed("<4I", [(1, 255, 65536, 123456)]))
        actual = output("R32G32B32A32_UINT", packed("<4I", [(1, 255, 65536, 123457)]))
        self.assert_differs(reference, actual, "pixel 0")

    def test_requires_hresults_to_match_exactly(self) -> None:
        readback = packed("4B", [(51, 102, 153, 204)])
        reference = output("R8G8B8A8_UNORM", readback)
        actual = output("R8G8B8A8_UNORM", readback, map_result="0x80070057")
        self.assert_differs(
            reference, actual, 'cases[R8G8B8A8_UNORM].mips[0].map: expected "0x00000000"'
        )

    def test_reports_missing_and_extra_keys(self) -> None:
        reference = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)]))
        actual = json.loads(json.dumps(reference))
        del actual["create_device"]
        actual["extra"] = 1
        self.assert_differs(reference, actual, "create_device: missing")
        self.assert_differs(reference, actual, "extra: not in the reference")

    def test_rejects_readbacks_of_different_size(self) -> None:
        reference = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)] * 2))
        actual = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)]))
        self.assert_differs(reference, actual, "expected 2 pixels, got 1")

    def test_exact_mode_ignores_tolerance(self) -> None:
        reference = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)]))
        actual = output("R8G8B8A8_UNORM", packed("4B", [(52, 102, 153, 204)]))
        self.assertNotEqual(compare.differences(reference, actual, None), [])

    def test_fails_on_format_without_tolerance(self) -> None:
        reference = output("B8G8R8A8_UNORM", packed("4B", [(51, 102, 153, 204)]))
        actual = output("B8G8R8A8_UNORM", packed("4B", [(52, 102, 153, 204)]))
        with self.assertRaisesRegex(compare.ReferenceToolError, "B8G8R8A8_UNORM"):
            compare.differences(reference, actual, TOLERANCES)


class CommandLineTest(unittest.TestCase):
    def run_compare(self, actual: dict) -> int:
        reference = output("R8G8B8A8_UNORM", packed("4B", [(51, 102, 153, 204)]))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "reference.json").write_text(json.dumps({"result": reference}))
            (root / "tolerance.json").write_text(json.dumps({"max_channel_difference": TOLERANCES}))
            (root / "actual.json").write_text(json.dumps(actual))
            arguments = [
                "--reference",
                str(root / "reference.json"),
                "--actual",
                str(root / "actual.json"),
            ]
            return compare.main(arguments)

    def test_exits_zero_on_match_and_one_on_mismatch(self) -> None:
        matching = output("R8G8B8A8_UNORM", packed("4B", [(50, 102, 153, 204)]))
        mismatching = output("R8G8B8A8_UNORM", packed("4B", [(49, 102, 153, 204)]))
        self.assertEqual(self.run_compare(matching), 0)
        self.assertEqual(self.run_compare(mismatching), compare.EXIT_MISMATCH)


if __name__ == "__main__":
    unittest.main()
