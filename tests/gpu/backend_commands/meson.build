# GPU tests for backend_commands (suite 'gpu').
backend_commands_tests = executable(
    'backend_commands_tests',
    'clear_test.mm',
    'copy_test.mm',
    'deferred_free_test.mm',
    'device_lost_test.mm',
    'progress_test.mm',
    'subresource_test.mm',
    'timeline_test.mm',
    include_directories: backend_private_include,
    dependencies: [backend_dep, gtest_dep],
    objcpp_args: '-fobjc-arc',
)

test(
    'backend_commands',
    backend_commands_tests,
    protocol: 'gtest',
    suite: 'gpu',
    env: gpu_test_env,
    timeout: 120,
)
