# Unit tests for src/frontends/com (suite 'unit': no GPU required).
com_tests = executable(
    'com_tests',
    'concurrency_test.cpp',
    'iid_test.cpp',
    'lifetime_test.cpp',
    'query_interface_test.cpp',
    dependencies: [com_dep, gtest_dep],
)

test('com', com_tests, protocol: 'gtest', suite: 'unit', timeout: 60)
