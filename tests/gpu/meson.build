# GPU tests (suite 'gpu'): run on Apple silicon with Metal API and shader validation enabled; any
# validation error fails the test (ADR 0007).
gpu_test_env = ['MTL_DEBUG_LAYER=1', 'MTL_SHADER_VALIDATION=1', 'MTL_DEBUG_LAYER_ERROR_MODE=assert']

foreach component : ['backend_memory', 'backend_commands', 'host', 'formats']
    subdir(component)
endforeach

# Proves validation is really active: the canary's invalid blit encode must abort the process with
# the debug layer's message, which run_api_test.py checks (it also sets the validation variables).
# tests/api runs the unix library variant in place of directmetal.so under Wine.
validation_canary = executable(
    'validation_canary',
    'validation_canary.mm',
    dependencies: metal_dep,
    objcpp_args: '-fobjc-arc',
)

test(
    'validation_canary',
    python,
    args: [
        '-I', files('../api/run_api_test.py'),
        '--mode', 'native',
        '--program', validation_canary,
        '--expect-abort-stderr', 'failed assertion `Copy From Buffer Validation',
    ],
    suite: 'gpu',
    is_parallel: false,
    timeout: 60,
)

validation_canary_unixlib = shared_library(
    'validation_canary_unixlib',
    'validation_canary.mm',
    name_prefix: '',
    name_suffix: 'so',
    dependencies: [host_core_dep, metal_dep],
    objcpp_args: ['-fobjc-arc', '-DDIRECTMETAL_VALIDATION_CANARY_UNIXLIB'],
    gnu_symbol_visibility: 'hidden',
)
