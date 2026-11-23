# Unit tests for src/runtime (suite 'unit': no GPU required).
runtime_tests = executable(
    'runtime_tests',
    'log_test.cpp',
    dependencies: [runtime_dep, gtest_dep],
)

test('runtime', runtime_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
