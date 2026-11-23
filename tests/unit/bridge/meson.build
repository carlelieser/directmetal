# Unit tests for src/bridge (suite 'unit': no GPU required).
bridge_native_tests = executable(
    'bridge_native_tests',
    'native_transport_test.cpp',
    dependencies: [bridge_client_dep, bridge_transport_dep, runtime_dep, gtest_dep],
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

bridge_client_tests = executable(
    'bridge_client_tests',
    'chunk_builder.cpp',
    'device_client_test.cpp',
    'fake_transport.cpp',
    'shared_block_allocator_test.cpp',
    'staging_ring_test.cpp',
    'status_mapping_test.cpp',
    'stream_writer_test.cpp',
    dependencies: [bridge_client_dep, runtime_dep, gtest_dep],
)

test('bridge_client', bridge_client_tests, protocol: 'gtest', suite: 'unit', timeout: 60)
