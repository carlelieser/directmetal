# 0010 — Windows headers for native builds

Status: accepted. Refines [ADR 0001](0001-license.md).

## Context

Front-end sources include `<d3d11.h>`, `<dxgi.h>` and `<unknwn.h>`. PE builds get them from LLVM-MinGW. Native macOS builds of the same sources ([ADR 0009](0009-front-end-placement.md)) need them too. Microsoft's DirectX-Headers (MIT) cover Direct3D 12 and DXGI formats but not Direct3D 9–11. The mingw-w64 Direct3D headers are generated from Wine's IDL files, which are LGPL. Writing the headers by hand from documentation is slow and risks vtable-order errors.

## Decision

- Native builds compile against pinned, unmodified mingw-w64 headers fetched at build time by a Meson wrap, plus a first-party shim for base Windows types. The headers are never vendored, modified, installed or shipped.
- `DXGI_FORMAT` and `dxgicommon.h` come from DirectX-Headers, also fetched by a Meson wrap.
- The Phase 9 native SDK ships headers generated from Microsoft's win32metadata (MIT) instead.

## Consequences

- LGPL headers are used only as compile-time inputs, consistent with treating LGPL code as reference only.
- The pinned revision matches the one the API inventory is generated from (`tools/api/sources.tsv`).
- Phase 9 needs a header generator before the SDK can be released.
