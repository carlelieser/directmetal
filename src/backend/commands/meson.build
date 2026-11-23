# Queues, command recording, submission and timelines (factory.hpp). backend_commands_lib is linked
# into the backend.
backend_commands_lib = static_library(
    'backend_commands',
    'clear.mm',
    'factory.mm',
    'loss_tracker.mm',
    'queue.mm',
    'recorder.mm',
    'timeline.mm',
    include_directories: [backend_private_include, backend_include],
    dependencies: [memory_dep, metal_dep],
    objcpp_args: '-fobjc-arc',
)

# Test seams (test_seams.hpp), linked only into the backend_commands GPU tests.
backend_commands_testing_lib = static_library(
    'backend_commands_testing',
    'test_seams.mm',
    include_directories: [backend_private_include, backend_include],
    dependencies: [memory_dep, metal_dep],
    objcpp_args: '-fobjc-arc',
)
backend_commands_testing_dep = declare_dependency(link_with: backend_commands_testing_lib)
