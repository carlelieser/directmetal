# The front end is a static library on both platforms: linked into d3d11.dll in PE builds and into
# native test executables and the SDK in macOS builds.
d3d11_frontend_lib = static_library(
    'd3d11_frontend',
    'd3d11.cpp',
    dependencies: [frontend_markers_dep, win32_headers_dep],
)

d3d11_frontend_dep = declare_dependency(
    link_whole: d3d11_frontend_lib,
    dependencies: frontend_markers_dep,
)

if is_windows
    d3d11_dll = shared_library(
        'd3d11',
        link_whole: [pe_dll_main, d3d11_frontend_lib],
        name_prefix: '',
        dependencies: frontend_markers_dep,
        vs_module_defs: 'd3d11.def',
        link_args: pe_link_args,
    )
endif
