# Unit tests for src/formats (suite 'unit': no GPU required).
formats_tests = executable(
    'formats_tests',
    'd3d9_formats_test.cpp',
    'dxgi_formats_test.cpp',
    'dxgi_support_test.cpp',
    'view_compatibility_test.cpp',
    dependencies: [formats_dep, gtest_dep],
)

test('formats', formats_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
