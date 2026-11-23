# formats_dep's header is free of Windows headers. Only the table sources include DirectX-Headers'
# dxgiformat.h (never together with the toolchain's), through formats_dxgi_headers, to check their
# values in native builds.
formats_dxgi_headers = []
if is_darwin
    directx_headers = dependency(
        'DirectX-Headers',
        default_options: ['werror=false', 'warning_level=0', 'build-test=false'],
    )
    formats_dxgi_headers += directx_headers.partial_dependency(includes: true, compile_args: true)
endif

formats_dep = declare_dependency(
    include_directories: include_directories('include'),
    dependencies: backend_types_dep,
)
