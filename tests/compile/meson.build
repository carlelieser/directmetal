# Compiles every interface header, and with it the wire-layout static_asserts, in each build:
# win32, win64 and the macOS unix builds.
if is_windows
    static_library(
        'interfaces_pe',
        'interfaces_pe.cpp',
        dependencies: [bridge_client_dep, formats_dep, memory_dep, runtime_dep, com_dep],
    )
else
    static_library(
        'interfaces_native',
        'interfaces_native.cpp',
        dependencies: [bridge_client_dep, formats_dep, host_core_dep, runtime_dep],
    )
    static_library(
        'backend_private',
        'backend_private.mm',
        include_directories: backend_private_include,
        dependencies: backend_dep,
        objcpp_args: '-fobjc-arc',
    )
endif
