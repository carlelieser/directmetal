directmetal_unix_include = include_directories('include')

directmetal_unix_lib = shared_library(
    'directmetal_unix',
    'version.cpp',
    include_directories: directmetal_unix_include,
    cpp_args: '-DDIRECTMETAL_VERSION="@0@"'.format(meson.project_version()),
    gnu_symbol_visibility: 'hidden',
)

directmetal_unix_dep = declare_dependency(
    link_with: directmetal_unix_lib,
    include_directories: directmetal_unix_include,
)
