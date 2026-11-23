# GPU tests for host (suite 'gpu'): the process-wide host on the Metal backend, driven through the
# native in-process bridge by the bridge client.
host_gpu_tests = executable(
    'host_gpu_tests',
    'host_fixture.cpp',
    'readback_test.cpp',
    'session_test.cpp',
    'teardown_test.cpp',
    dependencies: [bridge_client_dep, bridge_transport_dep, runtime_dep, gtest_dep],
)

test(
    'host',
    host_gpu_tests,
    protocol: 'gtest',
    suite: 'gpu',
    env: gpu_test_env,
    timeout: 120,
)
