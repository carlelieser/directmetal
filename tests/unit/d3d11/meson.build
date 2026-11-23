# Unit tests for src/frontends/d3d11 (suite 'unit': no GPU required). The front end is linked
# against fake_bridge.cpp, which stands in for the bridge transport and the unix side.
d3d11_tests = executable(
    'd3d11_tests',
    'context_test.cpp',
    'create_device_test.cpp',
    'fake_bridge.cpp',
    'render_target_view_test.cpp',
    'staging_layout_test.cpp',
    'texture_test.cpp',
    link_whole: d3d11_frontend_lib,
    dependencies: [d3d11_frontend_deps, gtest_dep],
)

test('d3d11', d3d11_tests, protocol: 'gtest', suite: 'unit', timeout: 60)
