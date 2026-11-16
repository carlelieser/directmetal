# 0008 — Wine bridge module

Status: accepted. Refines [ADR 0002](0002-delivery-model.md).

## Context

Wine's unixlib interface lets a PE module call into a unix-side library. Wine attaches a unix library only to a module it loaded as builtin, and locates that library by the module's file name (`foo.dll` → `foo.so`). A native load-order override for the module disables it.

DXGI and the Direct3D front ends must share one Metal device, one submission thread and one set of swap chains, so the Metal backend must exist once per process.

Alternative considered: each of the six API DLLs marked builtin with its own `.so`, all loading a shared unix core. This puts seven images on the unix side, ties the backend's lifetime to whichever API DLL loads first, and makes every API DLL a PE↔unix boundary that needs WoW64 thunks.

## Decision

- One PE module, `directmetal.dll` (x86_64 and i686), is the only PE↔unix boundary. It is loaded as builtin and paired with one unix library, `directmetal.so` (x86_64), which holds the Metal backend.
- `d3d9.dll`, `d3d10core.dll`, `d3d10.dll`, `d3d11.dll`, `d3d12.dll` and `dxgi.dll` depend on `directmetal.dll` and never call the unix side directly.
- Builtin marking is a post-link step implemented in DirectMetal's own build tooling, written from Wine's documented module format. Wine's `winebuild` is not a build dependency.

## Consequences

- The backend is instantiated once per process, whichever API DLLs an application loads.
- WoW64 handling is confined to one module pair.
- The Phase 9 native SDK has no PE modules: the front ends and backend are unchanged and only the bridge is replaced.
