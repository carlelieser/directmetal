# Unit tests for src/unix (host) (suite 'unit': no GPU required).
host_tests = executable(
    'host_tests',
    'dispatch_test.cpp',
    dependencies: [host_core_dep, gtest_dep],
)

test('host', host_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
