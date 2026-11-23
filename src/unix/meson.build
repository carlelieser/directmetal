directmetal_unix_include = include_directories('include')

# The unix side of the bridge (host::Host, host::dispatch), shared by directmetal.so and the
# native bridge.
host_core_lib = static_library(
    'host_core',
    'host/command_rules.cpp',
    'host/dispatch.cpp',
    'host/pixel_layout.cpp',
    'host/texture_rules.cpp',
    include_directories: directmetal_unix_include,
    dependencies: [bridge_protocol_dep, backend_dep],
    gnu_symbol_visibility: 'hidden',
)

host_core_dep = declare_dependency(
    link_with: host_core_lib,
    include_directories: directmetal_unix_include,
    dependencies: [bridge_protocol_dep, backend_dep],
)

# Wine pairs directmetal.dll with <dll_path>/x86_64-unix/directmetal.so, so the name is fixed.
directmetal_unix_lib = shared_library(
    'directmetal',
    'unixlib.cpp',
    'version.cpp',
    name_prefix: '',
    name_suffix: 'so',
    include_directories: directmetal_unix_include,
    dependencies: host_core_dep,
    cpp_args: '-DDIRECTMETAL_VERSION="@0@"'.format(meson.project_version()),
    gnu_symbol_visibility: 'hidden',
)

directmetal_unix_dep = declare_dependency(
    link_with: directmetal_unix_lib,
    include_directories: directmetal_unix_include,
    dependencies: bridge_protocol_dep,
)
