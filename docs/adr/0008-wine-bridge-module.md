# 0008 — Wine bridge module

Status: accepted

## Context

Wine's unixlib interface lets a PE module call into a unix-side library. Wine loads a unix library only for a module marked as builtin, and finds it by the module's name. DXGI and the Direct3D front ends must share one Metal device, one submission thread and one set of swap chains, so the Metal backend must exist once per process.

## Decision

- One PE bridge module, `directmetal.dll` (x86_64 and i686), is the only module that crosses into the unix side. It is marked builtin by a post-link step, since LLVM-MinGW does not produce Wine's builtin signature.
- One unix library, `directmetal.so` (x86_64), holds the Metal backend and exports the unixlib function tables for 64-bit callers and for 32-bit callers under WoW64.
- `d3d9.dll`, `d3d10core.dll`, `d3d10.dll`, `d3d11.dll`, `d3d12.dll` and `dxgi.dll` import `directmetal.dll` and never call the unix side directly.
- The PE↔unix calling convention (function codes, argument layouts, command-stream format) is declared in one versioned header shared by both sides. 32-bit callers go through explicit WoW64 thunks in the unix library.
- i686 exports are undecorated (`--kill-at`) to match the names applications import.

## Consequences

- The backend is instantiated once per process, whichever DLLs an application loads.
- Installing into a Wine prefix means placing `directmetal.dll` and `directmetal.so` where Wine looks for builtin modules and their unix libraries, alongside the six API DLLs.
- The native SDK bridge (ADR 0002) replaces only `directmetal.dll`'s transport; the API DLLs are unchanged.
