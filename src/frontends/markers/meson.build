frontend_markers_include = include_directories('include')

frontend_markers_lib = static_library(
    'frontend_markers',
    'markers.cpp',
    include_directories: frontend_markers_include,
    dependencies: directmetal_bridge_dep,
)

frontend_markers_dep = declare_dependency(
    link_with: frontend_markers_lib,
    include_directories: frontend_markers_include,
    dependencies: directmetal_bridge_dep,
)
