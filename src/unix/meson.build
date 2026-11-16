directmetal_unix_include = include_directories('include')

# Wine pairs directmetal.dll with <dll_path>/x86_64-unix/directmetal.so, so the name is fixed.
directmetal_unix_lib = shared_library(
    'directmetal',
    'unixlib.cpp',
    'version.cpp',
    name_prefix: '',
    name_suffix: 'so',
    include_directories: [directmetal_unix_include, bridge_include],
    cpp_args: '-DDIRECTMETAL_VERSION="@0@"'.format(meson.project_version()),
    gnu_symbol_visibility: 'hidden',
)

directmetal_unix_dep = declare_dependency(
    link_with: directmetal_unix_lib,
    include_directories: [directmetal_unix_include, bridge_include],
)
