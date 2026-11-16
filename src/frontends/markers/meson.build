frontend_markers_include = include_directories('include')

# PE builds report stubs through the bridge; native builds have no bridge yet and use stderr.
if host_machine.system() == 'windows'
    frontend_markers_platform_sources = ['call_log_windows.cpp', 'stub_report_bridge.cpp']
    frontend_markers_platform_deps = [
        directmetal_bridge_dep,
        meson.get_compiler('cpp').find_library('bcrypt'),
    ]
else
    frontend_markers_platform_sources = ['call_log_posix.cpp', 'stub_report_stderr.cpp']
    frontend_markers_platform_deps = []
endif

frontend_markers_lib = static_library(
    'frontend_markers',
    'call_log.cpp',
    'markers.cpp',
    frontend_markers_platform_sources,
    include_directories: frontend_markers_include,
    dependencies: frontend_markers_platform_deps,
)

frontend_markers_dep = declare_dependency(
    link_with: frontend_markers_lib,
    include_directories: frontend_markers_include,
    dependencies: frontend_markers_platform_deps,
)
