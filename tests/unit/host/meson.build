# Unit tests for src/unix (host) (suite 'unit': no GPU required). The host runs on a fake backend.
host_tests = executable(
    'host_tests',
    'concurrency_test.cpp',
    'control_calls_test.cpp',
    'copy_rules_test.cpp',
    'fake_backend.cpp',
    'fake_timeline.cpp',
    'handles_test.cpp',
    'host_fixture.cpp',
    'stream_error_test.cpp',
    'stream_test.cpp',
    'teardown_test.cpp',
    'texture_desc_test.cpp',
    dependencies: [host_core_dep, gtest_dep, dependency('gmock', default_options: ['default_library=static'])],
)

test('host', host_tests, protocol: 'gtest', suite: 'unit', timeout: 60)

# The Wine call tables of directmetal.so, over the process-wide host.
host_unixlib_tests = executable(
    'host_unixlib_tests',
    'unixlib_tables_test.cpp',
    dependencies: [directmetal_unix_dep, gtest_dep],
)

test('host_unixlib', host_unixlib_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
