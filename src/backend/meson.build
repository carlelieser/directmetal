# Metal backend. Objective-C++ sources stay here; public headers are plain C++. resource_access.hpp
# is private and for .mm sources only.
backend_private_include = include_directories('.')
metal_dep = dependency('appleframeworks', modules: ['Foundation', 'Metal'])

subdir('commands')

backend_lib = static_library(
    'backend',
    'adapters.mm',
    'arena.mm',
    'device.mm',
    'heap_pool.mm',
    'pixel_format_check.mm',
    'resources.mm',
    'texture_descriptor.mm',
    include_directories: [backend_private_include, backend_include],
    dependencies: [memory_dep, metal_dep],
    objcpp_args: '-fobjc-arc',
)

backend_dep = declare_dependency(
    link_with: [backend_lib, backend_commands_lib],
    include_directories: backend_include,
    dependencies: [memory_dep, metal_dep],
)
