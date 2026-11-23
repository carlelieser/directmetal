# formats_dep's header is free of Windows headers. Only the table sources include DirectX-Headers'
# dxgiformat.h (never together with the toolchain's), through formats_dxgi_headers, to check their
# values in native builds.
formats_sources = files(
    'd3d9_formats.cpp',
    'dxgi_formats.cpp',
    'metal_capabilities.cpp',
    'view_compatibility.cpp',
)
formats_dxgi_headers = []
if is_darwin
    directx_headers = dependency(
        'DirectX-Headers',
        default_options: ['werror=false', 'warning_level=0', 'build-test=false'],
    )
    formats_dxgi_headers += directx_headers.partial_dependency(includes: true, compile_args: true)
    formats_sources += files('dxgi_header_check.cpp')
endif

formats_lib = static_library(
    'formats',
    formats_sources,
    dependencies: [backend_types_dep, formats_dxgi_headers],
    include_directories: include_directories('include'),
)

formats_dep = declare_dependency(
    link_with: formats_lib,
    include_directories: include_directories('include'),
    dependencies: backend_types_dep,
)
