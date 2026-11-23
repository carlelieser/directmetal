# Windows headers for native macOS builds of front-end code (ADR 0010): the first-party shim in
# include/ declares the base types, and the pinned mingw-w64 tree supplies the Direct3D and DXGI
# headers, searched after the system headers so its C runtime headers never shadow the SDK's. PE
# builds use the toolchain's headers, so the dependency is empty there. has_native_win32_headers
# turns on native builds of the front ends.
if is_windows
    win32_headers_dep = declare_dependency()
else
    win32_headers_dep = declare_dependency(
        include_directories: include_directories('include'),
        dependencies: subproject('mingw-w64-headers').get_variable('mingw_w64_headers_dep'),
    )
endif
has_native_win32_headers = false
