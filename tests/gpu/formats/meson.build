# GPU tests for formats (suite 'gpu'): every format the tables claim on the running GPU is created,
# cleared and copied with its claimed usages under Metal validation.
format_gpu_tests = executable(
    'format_gpu_tests',
    'buffer_usage_test.mm',
    'claimed_texture_test.mm',
    'format_fixture.mm',
    'mip_autogen_test.mm',
    'multisample_test.mm',
    'texture_plan.cpp',
    include_directories: backend_private_include,
    dependencies: [backend_dep, formats_dep, gtest_dep],
    objcpp_args: '-fobjc-arc',
)

test(
    'formats',
    format_gpu_tests,
    protocol: 'gtest',
    suite: 'gpu',
    env: gpu_test_env,
    timeout: 120,
)
