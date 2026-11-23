# Build file added to the unmodified mingw-w64 tree fetched by subprojects/mingw-w64-headers.wrap at
# the revision in tools/api/sources.tsv. Only the Direct3D and DXGI headers are used, at compile
# time, after the system headers; nothing is built, installed or shipped (ADR 0010).
project('mingw-w64-headers', license: 'ZPL-2.1 AND LGPL-2.1-or-later', meson_version: '>=1.3.0')

mingw_w64_headers_dep = declare_dependency(
    compile_args: ['-idirafter', meson.current_source_dir() / 'mingw-w64-headers' / 'include'],
)
