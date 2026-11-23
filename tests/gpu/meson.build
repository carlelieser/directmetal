# GPU tests (suite 'gpu'): run on Apple silicon with Metal API and shader validation enabled; any
# validation error fails the test (ADR 0007).
gpu_test_env = ['MTL_DEBUG_LAYER=1', 'MTL_SHADER_VALIDATION=1', 'MTL_DEBUG_LAYER_ERROR_MODE=assert']

foreach component : ['backend_memory', 'backend_commands', 'host', 'formats']
    subdir(component)
endforeach

# Proves validation is really active: the canary's invalid blit encode must abort the process.
validation_canary = executable(
    'validation_canary',
    'validation_canary.mm',
    dependencies: metal_dep,
    objcpp_args: '-fobjc-arc',
)

test(
    'validation_canary',
    validation_canary,
    suite: 'gpu',
    env: gpu_test_env,
    should_fail: true,
    is_parallel: false,
    timeout: 60,
)
