# 0009 — Front-end placement

Status: accepted. Refines [ADR 0008](0008-wine-bridge-module.md).

## Context

The API front ends (COM objects, state tracking, validation) could run on the PE side and send work to the unix side, or run on the unix side behind a thunk per COM method. Direct3D 9 titles make tens of thousands of calls per frame; Direct3D 9–12 and DXGI declare about 1,500 methods, many taking structs and pointers that differ between 32-bit and 64-bit callers. Applications pass objects the front ends must call back into, such as `ID3DInclude` and interfaces stored with `SetPrivateDataInterface`. The Phase 9 native SDK has no PE modules.

## Decision

- The front ends run on the PE side and reach the unix side only through `directmetal.dll`.
- The interface between the two sides is a small set of synchronous control calls (create and destroy objects, wait) plus a one-way command stream. The front end flushes the stream only when a chunk is full, on an explicit flush, before a CPU wait and at submit.
- Every field crossing the boundary has a fixed width and every address travels as 64 bits, so 32-bit and 64-bit callers send identical bytes. Memory both sides access is allocated on the PE side and imported by the unix side without copying.
- The same front-end sources build natively for macOS. A native bridge library provides `directmetal.dll`'s exports in-process and passes calls to the unix-side dispatcher through the same protocol.

## Consequences

- Recording a command costs no PE↔unix transition; a flush costs one.
- WoW64 support is limited to the protocol's fixed layouts and the shared-memory rule instead of per-method thunks.
- The native SDK is the front ends, the native bridge, the dispatcher and the backend in one library.
- Native test processes exercise the encoder, decoder and dispatcher on every run, without Wine.
