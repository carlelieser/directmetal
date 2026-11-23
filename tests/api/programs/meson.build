# API test programs shared by the Windows reference job and the macOS runs. manifest.tsv lists the
# programs the reference job runs; tools/reference hashes their sources and the harness.
api_harness_lib = static_library(
    'api_harness',
    '../harness/output.cpp',
    '../harness/options.cpp',
    is_windows ? '../harness/process_windows.cpp' : '../harness/process_native.cpp',
    include_directories: include_directories('../harness/include'),
)

api_harness_dep = declare_dependency(
    link_with: api_harness_lib,
    include_directories: include_directories('../harness/include'),
)

d3d11_import_dep = meson.get_compiler('cpp').find_library('d3d11')

d3d11_clear_readback = executable(
    'd3d11_clear_readback',
    'd3d11_clear_readback.cpp',
    dependencies: [api_harness_dep, d3d11_import_dep],
    link_args: '-static',
)

# Drives the bridge client directly under Wine; not in manifest.tsv, since it needs the unix side.
bridge_ring_roundtrip = executable(
    'bridge_ring_roundtrip',
    'bridge_ring_roundtrip.cpp',
    dependencies: [bridge_client_dep, bridge_transport_dep],
    link_args: '-static',
)
