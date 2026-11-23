# The front end is a static library on both platforms: linked into d3d11.dll in PE builds and into
# native test executables and the SDK in macOS builds. The library itself leaves the bridge
# transport unlinked, so unit tests can link it against a fake transport; d3d11_frontend_dep and the
# DLL add the real one.
d3d11_frontend_deps = [
    bridge_client_dep,
    com_dep,
    formats_dep,
    frontend_markers_dep,
    runtime_dep,
    win32_headers_dep,
]

d3d11_frontend_lib = static_library(
    'd3d11_frontend',
    'bridge_device.cpp',
    'clear_color.cpp',
    'context_commands.cpp',
    'context_pipeline_stubs.cpp',
    'context_query_stubs.cpp',
    'context_transfer_stubs.cpp',
    'd3d11.cpp',
    'device.cpp',
    'device_context.cpp',
    'device_stubs.cpp',
    'feature_level.cpp',
    'hresult.cpp',
    'render_target_view.cpp',
    'staging_layout.cpp',
    'texture.cpp',
    'texture_desc.cpp',
    dependencies: d3d11_frontend_deps,
)

d3d11_frontend_dep = declare_dependency(
    link_whole: d3d11_frontend_lib,
    dependencies: [d3d11_frontend_deps, bridge_transport_dep],
)

if is_windows
    d3d11_dll = shared_library(
        'd3d11',
        link_whole: [pe_dll_main, d3d11_frontend_lib],
        name_prefix: '',
        dependencies: [d3d11_frontend_deps, bridge_transport_dep],
        vs_module_defs: 'd3d11.def',
        link_args: pe_link_args,
    )
endif
