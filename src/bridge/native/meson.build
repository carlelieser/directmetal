# In-process bridge for macOS builds: provides the client.hpp exports and calls host::dispatch.
bridge_native_lib = static_library(
    'bridge_native',
    'transport.cpp',
    include_directories: bridge_transport_include,
    dependencies: [bridge_protocol_dep, host_core_dep],
)

bridge_transport_dep = declare_dependency(
    link_with: bridge_native_lib,
    dependencies: [bridge_protocol_dep, host_core_dep],
)
