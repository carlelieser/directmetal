# Unit tests for src/runtime (suite 'unit': no GPU required).
runtime_tests = executable(
    'runtime_tests',
    'config_test.cpp',
    'hud_test.cpp',
    'log_test.cpp',
    'log_file_test.cpp',
    'log_spec_test.cpp',
    dependencies: [runtime_dep, gtest_dep],
)

test('runtime', runtime_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
