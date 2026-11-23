# Metal backend. Objective-C++ sources stay here; public headers are plain C++. resource_access.hpp
# is private and for .mm sources only.
backend_private_include = include_directories('.')

subdir('commands')

backend_dep = declare_dependency(
    link_with: backend_commands_lib,
    include_directories: backend_include,
    dependencies: memory_dep,
)
