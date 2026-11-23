import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from inventory.directx_parser import parse_directx_group  # noqa: E402

BASELESS_IDL = """
[ local, object, uuid(ae897105-00e6-45bf-bb8e-281dd6db8e1b) ]
interface ID3D10EffectVariable
{
    BOOL IsValid();
    HRESULT GetDesc(D3D10_EFFECT_VARIABLE_DESC *desc);
}

[ local, object, uuid(a68e2a52-9d0b-4a8f-a1cc-7d43d4ecfa6b) ]
interface ID3D10EffectScalarVariable : ID3D10EffectVariable
{
    HRESULT SetFloat(float value);
    HRESULT GetFloat(float *value);
}

interface ID3D10EffectForward;
"""


class DirectXParserTest(unittest.TestCase):
    def parse(self, name: str, text: str):
        with tempfile.TemporaryDirectory() as root:
            group = Path(root) / "d3d10"
            group.mkdir()
            (group / name).write_text(text, encoding="utf-8")
            return parse_directx_group(group)

    def test_parses_idl_interface_without_base(self) -> None:
        inventory = self.parse("effect.idl", BASELESS_IDL)
        variable = inventory.interfaces["ID3D10EffectVariable"]
        self.assertIsNone(variable.base)
        self.assertEqual([member.name for member in variable.members], ["IsValid", "GetDesc"])

    def test_derived_interface_keeps_base_and_own_methods(self) -> None:
        inventory = self.parse("effect.idl", BASELESS_IDL)
        scalar = inventory.interfaces["ID3D10EffectScalarVariable"]
        self.assertEqual(scalar.base, "ID3D10EffectVariable")
        self.assertEqual([member.name for member in scalar.members], ["SetFloat", "GetFloat"])

    def test_ignores_forward_declarations(self) -> None:
        inventory = self.parse("effect.idl", BASELESS_IDL)
        self.assertNotIn("ID3D10EffectForward", inventory.interfaces)


if __name__ == "__main__":
    unittest.main()
