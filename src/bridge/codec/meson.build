# Command stream size rules shared by the writer, and the decoder the host runs per stream.
bridge_codec_include = include_directories('include')

bridge_codec_lib = static_library(
    'bridge_codec',
    'stream_decoder.cpp',
    include_directories: bridge_codec_include,
    dependencies: bridge_protocol_dep,
)

bridge_codec_dep = declare_dependency(
    link_with: bridge_codec_lib,
    include_directories: bridge_codec_include,
    dependencies: bridge_protocol_dep,
)
