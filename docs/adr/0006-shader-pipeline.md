# 0006 — Shader translation pipeline

Status: accepted

## Context

Applications supply D3D9 shader bytecode (SM1–3), DXBC (SM4.0–5.1) or DXIL (SM6.x). Metal accepts MSL source or AIR (Metal's LLVM IR). Routing through SPIR-V would add a second IR and an extra translation step; Apple's Metal Shader Converter is closed source.

## Decision

- All shader formats translate into one internal IR, DirectMetal IR: typed SSA with structured control flow, explicit resource bindings and I/O signatures.
- Front ends: D3D9 bytecode reader, DXBC reader, and a DXIL reader. The DXIL bitcode reader may reuse code from DirectXShaderCompiler (NCSA).
- Backends: MSL source first; AIR emission follows in Phase 8 and becomes the default once it passes the same test corpus.
- Metal Shader Converter and SPIR-V are not used.
- Translated shaders and pipelines are cached on disk, keyed by bytecode hash and relevant state.

## Consequences

- Emulation passes (geometry shaders, tessellation, stream output, fixed function) are written once against DirectMetal IR.
- The project owns the full compiler; DXIL support is the largest single item in Phase 7.
