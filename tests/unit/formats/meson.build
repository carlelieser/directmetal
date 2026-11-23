# Unit tests for src/formats (suite 'unit': no GPU required).
formats_tests = executable(
    'formats_tests',
    'dxgi_formats_test.cpp',
    'dxgi_support_test.cpp',
    dependencies: [formats_dep, gtest_dep],
)

test('formats', formats_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
