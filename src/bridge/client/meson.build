# Typed client library over the control calls and the command stream, used by the front ends.
# It calls the client.hpp exports, so whatever links it also links bridge_transport_dep.
bridge_client_include = include_directories('include')

bridge_client_lib = static_library(
    'bridge_client',
    'stream_writer.cpp',
    include_directories: bridge_client_include,
    dependencies: [bridge_protocol_dep, bridge_codec_dep, memory_dep],
)

bridge_client_dep = declare_dependency(
    link_with: bridge_client_lib,
    include_directories: bridge_client_include,
    dependencies: [bridge_protocol_dep, bridge_codec_dep, memory_dep],
)
