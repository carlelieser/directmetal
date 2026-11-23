runtime_lib = static_library(
    'runtime',
    'log.cpp',
    include_directories: include_directories('include'),
)

runtime_dep = declare_dependency(
    link_with: runtime_lib,
    include_directories: include_directories('include'),
)
