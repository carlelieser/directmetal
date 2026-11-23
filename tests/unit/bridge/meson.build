# Unit tests for src/bridge (suite 'unit': no GPU required).
bridge_native_tests = executable(
    'bridge_native_tests',
    'native_transport_test.cpp',
    dependencies: [bridge_transport_dep, gtest_dep],
)

test('bridge_native', bridge_native_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
