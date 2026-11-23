# GPU tests (suite 'gpu'): run on Apple silicon with Metal API and shader validation enabled; any
# validation error fails the test (ADR 0007).
gpu_test_env = ['MTL_DEBUG_LAYER=1', 'MTL_SHADER_VALIDATION=1', 'MTL_DEBUG_LAYER_ERROR_MODE=assert']

foreach component : ['backend_memory', 'backend_commands', 'host', 'formats']
    subdir(component)
endforeach
