# Queues, command recording, submission and timelines (factory.hpp). backend_commands_lib is linked
# into the backend.
backend_commands_lib = static_library(
    'backend_commands',
    'clear.mm',
    'factory.mm',
    'queue.mm',
    'recorder.mm',
    'timeline.mm',
    include_directories: [backend_private_include, backend_include],
    dependencies: [memory_dep, metal_dep],
    objcpp_args: '-fobjc-arc',
)
