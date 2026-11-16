import unittest

from fixture import FixtureTestCase


class SourceMarkerTest(FixtureTestCase):
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

    def test_scans_every_c_family_suffix(self) -> None:
        suffixes = [
            ".c",
            ".cc",
            ".cpp",
            ".cxx",
            ".h",
            ".hh",
            ".hpp",
            ".hxx",
            ".inl",
            ".ipp",
            ".m",
            ".mm",
        ]
        for suffix in suffixes:
            with self.subTest(suffix=suffix):
                self.fixture.write(
                    f"src/device{suffix}", 'void f() { DM_IMPL("ID3D11Device::CreateBuffer"); }'
                )
                self.run_ok()
                method = self.fixture.methods()["ID3D11Device::CreateBuffer"]
                self.assertEqual(method["location"], f"device{suffix}:1")
                (self.fixture.sources / f"device{suffix}").unlink()

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

    def test_rejects_inherited_key_in_source_naming_declaring_interface(self) -> None:
        self.fixture.write(
            "src/device.cpp", '\n\nvoid f() { DM_IMPL("ID3D11Device1::CreateBuffer"); }'
        )
        stderr = self.run_failing()
        self.assertIn("device.cpp:3", stderr)
        self.assertIn("use 'ID3D11Device::CreateBuffer'", stderr)


if __name__ == "__main__":
    unittest.main()
