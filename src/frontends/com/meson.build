# COM base shared by every front end.
com_include = include_directories('include')

com_lib = static_library(
    'com',
    'object.cpp',
    include_directories: com_include,
    dependencies: [frontend_markers_dep, runtime_dep, win32_headers_dep],
)

com_dep = declare_dependency(
    link_with: com_lib,
    include_directories: com_include,
    dependencies: [frontend_markers_dep, runtime_dep, win32_headers_dep],
)
