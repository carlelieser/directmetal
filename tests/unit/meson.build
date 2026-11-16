gtest_dep = dependency('gtest_main', default_options: ['default_library=static'])

unit_tests = executable(
    'unit_tests',
    'unixlib_test.cpp',
    'version_test.cpp',
    dependencies: [directmetal_unix_dep, gtest_dep],
    cpp_args: '-DDIRECTMETAL_EXPECTED_VERSION="@0@"'.format(meson.project_version()),
)

test('unit', unit_tests, protocol: 'gtest', timeout: 30)

marker_tests = executable(
    'marker_tests',
    'markers_test.cpp',
    dependencies: [frontend_markers_dep, gtest_dep],
)

test('markers', marker_tests, protocol: 'gtest', timeout: 30)

call_log_tests = executable(
    'call_log_tests',
    'call_log_test.cpp',
    dependencies: [frontend_markers_dep, gtest_dep],
)

test('call_log', call_log_tests, protocol: 'gtest', timeout: 30)
