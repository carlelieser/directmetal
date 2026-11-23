formats_platform_deps = []
if is_darwin
    # DXGI_FORMAT on macOS comes from DirectX-Headers (MIT); PE builds use the toolchain's header.
    # Only its headers are used; its own sources are built with its own warning settings.
    directx_headers = dependency(
        'DirectX-Headers',
        default_options: ['werror=false', 'warning_level=0', 'build-test=false'],
    )
    formats_platform_deps += directx_headers.partial_dependency(
        includes: true,
        compile_args: true,
    )
endif

formats_dep = declare_dependency(
    include_directories: include_directories('include'),
    dependencies: [backend_types_dep, formats_platform_deps],
)
