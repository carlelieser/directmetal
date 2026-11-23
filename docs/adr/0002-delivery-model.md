# 0002 — Delivery model

Status: accepted. The WoW64 pointer-thunk consequence is superseded by [ADR 0009](0009-front-end-placement.md).

## Context

Unmodified Windows games run on macOS only inside Wine-based runtimes (Wine, CrossOver and their front ends). Developers porting from source would instead link a native library.

## Decision

1. Phases 0–8 deliver Wine-integrated builds: PE DLLs (`d3d9.dll`, `d3d10core.dll`, `d3d10.dll`, `d3d11.dll`, `d3d12.dll`, `dxgi.dll`) that call a native unix-side library through Wine's unixlib interface, by way of one bridge module ([ADR 0008](0008-wine-bridge-module.md)).
2. Phase 9 adds a native macOS library exposing the same COM interfaces with DirectX-compatible headers, built from the same Metal backend and front ends.
3. The API front ends and Metal backend have no dependency on Wine. Only the platform bridge layer does.

## Consequences

- Window access goes through `winemac.drv`; the bridge tracks upstream Wine changes.
- 32-bit applications require WoW64 pointer thunks in the bridge.
- The native SDK requires no redesign, only a second bridge implementation.
