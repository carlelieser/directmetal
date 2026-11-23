platform_source = host_machine.system() == 'windows' ? 'platform_windows.cpp' : 'platform_posix.cpp'

runtime_lib = static_library(
    'runtime',
    'log.cpp',
    'log_environment.cpp',
    'log_file.cpp',
    'log_spec.cpp',
    platform_source,
    'text.cpp',
    include_directories: include_directories('include'),
)

runtime_dep = declare_dependency(
    link_with: runtime_lib,
    include_directories: include_directories('include'),
)
