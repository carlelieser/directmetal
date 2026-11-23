# Queues, command recording, submission and timelines (factory.hpp). backend_commands_lib is linked
# into the backend.
backend_commands_lib = static_library(
    'backend_commands',
    'factory.cpp',
    include_directories: [backend_private_include, backend_include],
    dependencies: memory_dep,
)
