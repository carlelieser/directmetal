# Allocators shared by the backend heaps (RangeAllocator) and the bridge staging rings
# (RingAllocator). Built for PE and macOS.
memory_include = include_directories('include')

memory_lib = static_library(
    'memory',
    'src/range_allocator.cpp',
    'src/ring_allocator.cpp',
    include_directories: memory_include,
)

memory_dep = declare_dependency(link_with: memory_lib, include_directories: memory_include)
