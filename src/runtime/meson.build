runtime_lib = static_library(
    'runtime',
    'log.cpp',
    'log_spec.cpp',
    'text.cpp',
    include_directories: include_directories('include'),
)

runtime_dep = declare_dependency(
    link_with: runtime_lib,
    include_directories: include_directories('include'),
)
