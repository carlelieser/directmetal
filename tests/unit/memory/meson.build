# Unit tests for src/memory (suite 'unit': no GPU required).
memory_tests = executable(
    'memory_tests',
    'range_allocator_test.cpp',
    dependencies: [memory_dep, gtest_dep],
)

test('memory', memory_tests, protocol: 'gtest', suite: 'unit', timeout: 30)
