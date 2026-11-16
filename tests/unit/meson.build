gtest_dep = dependency('gtest_main', default_options: ['default_library=static'])

unit_tests = executable(
    'unit_tests',
    'version_test.cpp',
    dependencies: [directmetal_unix_dep, gtest_dep],
    cpp_args: '-DDIRECTMETAL_EXPECTED_VERSION="@0@"'.format(meson.project_version()),
)

test('unit', unit_tests, protocol: 'gtest', timeout: 30)
