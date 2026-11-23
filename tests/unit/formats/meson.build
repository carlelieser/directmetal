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

format_names = executable('format_names', 'format_names.cpp', dependencies: formats_dep)

test(
    'format_coverage',
    python,
    args: [
        '-I', files('check_format_coverage.py'),
        '--names-program', format_names,
        '--dxgi-json', files('../../../docs/api/dxgi.json'),
        '--d3d9-json', files('../../../docs/api/d3d9.json'),
    ],
    suite: 'unit',
    timeout: 30,
)
