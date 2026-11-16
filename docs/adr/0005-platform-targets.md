# 0005 — Platform targets

Status: accepted

## Context

Apple silicon GPUs (Apple GPU family 7 and later) support BC compression, mesh shaders, ray tracing and argument buffers tier 2. Metal 4 (macOS 26) introduces command allocators, argument tables and explicit barriers that map directly to Direct3D 12. Wine on macOS runs as an x86_64 process under Rosetta 2.

## Decision

- Supported hardware: Macs with Apple silicon (GPU family Apple7 or later). Intel Macs are not supported.
- Direct3D 9, 10 and 11 require macOS 14 or later.
- Direct3D 12 requires macOS 26 or later and is implemented only on Metal 4. There is no Metal 3 path for Direct3D 12.
- The unix-side library for Wine builds is x86_64 (running under Rosetta 2); the native SDK in Phase 9 builds for arm64.

## Consequences

- The capability database assumes one GPU architecture family, which removes most per-vendor paths.
- Direct3D 12 users must run macOS 26.
