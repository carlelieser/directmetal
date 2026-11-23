# Unit tests for src/bridge (suite 'unit': no GPU required).
bridge_native_tests = executable(
    'bridge_native_tests',
    'native_transport_test.cpp',
    dependencies: [bridge_transport_dep, gtest_dep],
)

test('bridge_native', bridge_native_tests, protocol: 'gtest', suite: 'unit', timeout: 30)

bridge_codec_tests = executable(
    'bridge_codec_tests',
    'chunk_builder.cpp',
    'decoder_corpus_test.cpp',
    'stream_decoder_test.cpp',
    dependencies: [bridge_codec_dep, gtest_dep],
)

test('bridge_codec', bridge_codec_tests, protocol: 'gtest', suite: 'unit', timeout: 120)
