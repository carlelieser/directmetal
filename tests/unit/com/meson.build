# Unit tests for src/frontends/com (suite 'unit': no GPU required).
com_tests = executable(
    'com_tests',
    'allocation_failure_test.cpp',
    'boundary_test.cpp',
    'concurrency_test.cpp',
    'debug_name_test.cpp',
    'iid_test.cpp',
    'lifetime_test.cpp',
    'private_data_test.cpp',
    'query_interface_test.cpp',
    'ref_counts_test.cpp',
    dependencies: [com_dep, gtest_dep],
)

test('com', com_tests, protocol: 'gtest', suite: 'unit', timeout: 60)

# The shim's BOOL and interface macros cannot coexist with Objective-C headers (ADR 0009), so
# including it from Objective-C++ must fail.
if meson.get_compiler('objcpp').compiles(
    '#include <windows.h>',
    dependencies: win32_headers_dep,
    name: 'windows.h as Objective-C++',
)
    error('src/win32/include/windows.h compiled as Objective-C++')
endif
