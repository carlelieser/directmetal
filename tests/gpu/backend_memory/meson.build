# GPU tests for backend_memory (suite 'gpu').
backend_memory_tests = executable(
    'backend_memory_tests',
    'adapter_test.mm',
    include_directories: backend_private_include,
    dependencies: [backend_dep, gtest_dep],
    objcpp_args: '-fobjc-arc',
)

test(
    'backend_memory',
    backend_memory_tests,
    protocol: 'gtest',
    suite: 'gpu',
    env: gpu_test_env,
    timeout: 60,
)
