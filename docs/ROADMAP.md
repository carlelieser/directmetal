# DirectMetal roadmap

DirectMetal implements Direct3D 9, 10, 11 and 12 (plus DXGI) directly on Metal, without an intermediate Vulkan layer. This document defines the architecture, the order of work, and the exit criteria for each phase.

## API surface

Counts come from the generated [API inventories](api/SUMMARY.md). Methods are those an interface declares itself.

| Group | Interfaces | Methods | Enums | Structs | Entry points |
| --- | ---: | ---: | ---: | ---: | ---: |
| Direct3D 9 | 20 | 223 | 52 | 44 | 9 |
| Direct3D 10 / 10.1 | 48 | 284 | 37 | 69 | 27 |
| Direct3D 11.0–11.4 | 79 | 470 | 88 | 171 | 3 |
| Direct3D 12 | 176 | 603 | 320 | 573 | 10 |
| DXGI 1.0–1.6 | 46 | 189 | 32 | 38 | 5 |
| Metal (macOS 26 SDK) | 294 | 2524 | 128 | — | 8 |

Metal 4 (`MTL4*`, 61 types in macOS 26) adds explicit command allocators, argument tables, residency sets and an explicit compiler. Direct3D 12 is implemented on it.

## Architecture

```
 Windows app (PE)                                    macOS (native, unix side)
 ┌──────────────────────────────┐                   ┌─────────────────────────────────┐
 │ d3d9.dll  d3d10core.dll       │                   │ Metal backend                   │
 │ d3d11.dll d3d12.dll dxgi.dll  │  Wine unixlib     │  device, heaps, encoders,       │
 │  API front ends: COM objects, ├──────────────────►│  pipeline + shader caches,      │
 │  state tracking, validation   │  thunks (+WoW64)  │  CAMetalLayer presentation      │
 └──────────────┬───────────────┘                   └────────────────┬────────────────┘
                │ shader bytecode                                     │
        ┌───────▼────────────────────────────────────────┐            │
        │ Shader compiler: D3D9 SM1–3 / DXBC SM4–5.1 /    │  MSL / AIR │
        │ DXIL SM6.x  →  DirectMetal IR  →  MSL or AIR     ├───────────►│
        └─────────────────────────────────────────────────┘
```

Layers, in dependency order (each depends only on layers above it):

1. **Platform bridge** — PE↔unix call marshalling through Wine's unixlib interface, including WoW64 thunks for 32-bit applications, and access to the window's `NSView` via `winemac.drv`.
2. **Metal backend** — API-neutral wrapper over Metal: device and feature detection, allocators, staging rings, command recording and submission thread, sync primitives, presentation.
3. **Shared translation services** — format tables (`DXGI_FORMAT`, `D3DFORMAT` → `MTLPixelFormat` + capabilities), state-object hashing, pipeline cache, shader compiler.
4. **API front ends** — D3D9, D3D10, D3D11, D3D12 and DXGI COM implementations. D3D10 is implemented on the D3D11 front end.

## Decisions

Recorded in [architecture decision records](adr/README.md).

| Area | Decision |
| --- | --- |
| License | MIT. Permissive code (MIT, zlib, BSD, NCSA, Apache-2.0) may be reused; LGPL/GPL code (Wine, vkd3d, vkd3d-proton, DXMT) is reference only. [ADR 0001](adr/0001-license.md) |
| Delivery | Wine-integrated PE/unix DLLs in Phases 0–8; native macOS SDK in Phase 9. [ADR 0002](adr/0002-delivery-model.md) |
| Language | C++20; Objective-C++ only inside the Metal backend. [ADR 0003](adr/0003-language.md) |
| Build | Meson; LLVM-MinGW cross file for PE, Apple clang for the unix side. [ADR 0004](adr/0004-build-system.md) |
| Platform | Apple silicon only; macOS 14 for D3D9–11, macOS 26 (Metal 4) for D3D12; x86_64 unix side under Rosetta 2. [ADR 0005](adr/0005-platform-targets.md) |
| Shaders | D3D9 bytecode, DXBC and DXIL front ends into DirectMetal IR; MSL backend, AIR from Phase 8. No SPIR-V, no Metal Shader Converter. [ADR 0006](adr/0006-shader-pipeline.md) |

## Phases

Each phase ends in a working, tested state. Phases 4–7 depend on 1–3; 5 depends on 4.

### Phase 0 — Foundations

- Meson build producing empty PE DLLs and a unix library loadable by Wine; CI on macOS runners (build, lint, unit tests).
- Coding standards, formatter config, contribution guide, issue templates.
- API inventory tooling (done: `tools/api/`) extended to emit per-interface coverage reports.

**Exit:** `d3d11.dll` stub loads in Wine on macOS, logs a call that crosses into the unix side, and returns `E_NOTIMPL`.

### Phase 1 — Core runtime and Metal backend

- COM base: `IUnknown`, internal/external refcounts, private data (`SetPrivateData`/`GetPrivateData`), `QueryInterface` tables, debug object names.
- Logging, per-application configuration, environment overrides, HUD hook points.
- Bridge: argument marshalling, WoW64 pointer thunks, command-stream batching to minimise PE↔unix transitions.
- Metal backend: `MTLDevice` selection and capability database, buffer/texture allocation over `MTLHeap`, upload/readback rings, CS-thread submission, `MTLSharedEvent` timelines.
- Format tables for `DXGI_FORMAT` (123 values) and `D3DFORMAT` with support flags, substitutions (e.g. `D24_UNORM_S8_UINT` → `Depth32Float_Stencil8`, 3-component formats via buffers) and view-format compatibility.

**Exit:** unit tests cover format mapping and allocator behavior; a native test harness clears and reads back a texture through the backend.

### Phase 2 — DXGI and presentation

- `IDXGIFactory`–`IDXGIFactory7`, adapters from `MTLDevice`, outputs from `NSScreen`, mode lists.
- Swap chains (`IDXGISwapChain`–`IDXGISwapChain4`) on `CAMetalLayer`: flip/blit models, sync intervals, resize, frame latency waitable objects, fullscreen transitions.
- HDR10/scRGB output through EDR and colorspace metadata (later in phase).

**Exit:** a D3D11 test app presents a cleared back buffer at the display refresh rate and survives window resize and fullscreen toggling.

### Phase 3 — Shader compiler infrastructure

- DirectMetal IR: typed SSA with structured control flow, resource bindings and I/O signatures.
- DXBC reader (SM4.0–5.0), signature and reflection parsing (`ID3D11ShaderReflection`).
- MSL emitter, binding layout generator, specialization for state-dependent variants (e.g. alpha test, sample count).
- On-disk shader and pipeline cache keyed by bytecode hash and state.
- Test corpus: HLSL compiled to DXBC with [vkd3d-shader](https://gitlab.winehq.org/wine/vkd3d), executed on GPU with result comparison.

**Exit:** vertex, pixel and compute SM5 corpus compiles and produces reference-matching output.

### Phase 4 — Direct3D 11

- Device and immediate context: state tracking, lazy encoder creation, render-pass merging, `Map` semantics (`DISCARD`/`NO_OVERWRITE` via buffer renaming), `UpdateSubresource`, copies, clears, mip generation.
- Resources and views; binding model (14 constant buffers, 128 SRVs, 16 samplers, 8/64 UAVs per stage) on argument buffers.
- Queries and predicates via visibility buffers and counter sample buffers.
- Deferred contexts and command lists.
- Emulation of features Metal lacks: geometry shaders and stream output (compute/mesh pipeline), hull/domain shaders (compute tessellation factors + post-tessellation vertex functions), line/point primitive quirks.
- Feature levels 10_0–11_1, then D3D11.1–11.4 interfaces (`ID3D11Device1`–`ID3D11Device5`, `ID3D11DeviceContext1`–`4`).

**Exit:** Wine's `dlls/d3d11/tests` pass at 90% or better, with every failure tracked as an issue; three representative D3D11 titles render correctly.

### Phase 5 — Direct3D 10 / 10.1

- `ID3D10Device`/`ID3D10Device1` mapped onto the D3D11 front end.
- `d3d10.dll` effects framework (`ID3D10Effect*`) and shader reflection.

**Exit:** Wine's `dlls/d3d10core/tests` and `dlls/d3d10/tests` pass at 90% or better, with every failure tracked as an issue.

### Phase 6 — Direct3D 9

- `IDirect3D9`/`IDirect3D9Ex`, devices, swap chains, resources with `D3DPOOL` semantics, lost/reset devices.
- Render, sampler and texture-stage state model; state blocks.
- Fixed-function pipeline generated as shaders (lighting, fog, texture stages, vertex blending).
- D3D9 shader bytecode (vs/ps 1.1–3.0) front end into DirectMetal IR.
- D3D9 conventions: half-pixel offset, clip planes, point sprites, `D3DFMT` FOURCC extensions used by games (`INTZ`, `DF24`, `ATI1/ATI2`, `NULL`).
- 32-bit application support through WoW64.

**Exit:** Wine's `dlls/d3d9/tests` pass at 90% or better, with every failure tracked as an issue; three representative D3D9 titles render correctly.

### Phase 7 — Direct3D 12

- Device, command queues (direct/compute/copy), allocators and lists on `MTL4CommandQueue`/`MTL4CommandAllocator`/`MTL4CommandBuffer`; Metal 4 only (macOS 26).
- Descriptor heaps and root signatures on argument buffers and `MTL4ArgumentTable`; root constants and root descriptors.
- Heaps, placed and reserved resources on `MTLHeap` and sparse textures; residency via `MTLResidencySet`.
- Resource barriers and enhanced barriers mapped to Metal 4 barriers; fences on `MTLSharedEvent`.
- Pipeline state objects and pipeline state streams; `ExecuteIndirect` via indirect command buffers or compute-generated arguments; bundles.
- DXBC SM5.1 and DXIL SM6.0–6.6 front ends into DirectMetal IR (wave ops, 16-bit types, dynamic resources).
- Mesh shaders on Metal mesh pipelines; DXR 1.0/1.1 on Metal ray tracing. Variable-rate shading, sampler feedback and work graphs are reported unsupported through `CheckFeatureSupport`.

**Exit:** vkd3d-proton's D3D12 test suite passes at 90% or better, with every failure tracked as an issue; three representative D3D12 titles render correctly at feature level 12_0.

### Phase 8 — Performance and compatibility

- Asynchronous pipeline compilation, cache prewarming, state deduplication, encoder merging.
- Frame-time HUD, Metal GPU capture integration, per-title configuration database.
- Public compatibility list and automated regression runs with captured frame comparisons.

### Phase 9 — Distribution

- Release packaging consumable by Wine-based runtimes; versioning and changelog policy.
- Native macOS SDK: arm64 library with DirectX-compatible headers, sharing the backend and front ends.
- User and contributor documentation.

## Concept mapping

| Direct3D | Metal |
| --- | --- |
| Device / immediate context | `MTLDevice`, `MTLCommandQueue`, encoders recorded on a submission thread |
| Command list / allocator (D3D12) | `MTL4CommandBuffer` / `MTL4CommandAllocator` |
| Swap chain | `CAMetalLayer` + `CAMetalDrawable` |
| Constant buffers, SRVs, UAVs, samplers | Buffer/texture/sampler bindings, argument buffers |
| Descriptor heap + root signature | Argument buffers + `MTL4ArgumentTable` |
| Pipeline state object | `MTLRenderPipelineState` / `MTLComputePipelineState` + depth-stencil state |
| Rasterizer / depth / blend state | Encoder state, `MTLDepthStencilState`, pipeline blend descriptors |
| Fence | `MTLSharedEvent` |
| Heap, placed resource | `MTLHeap`, placement heap allocation |
| Tiled / reserved resource | Sparse textures and sparse heaps |
| Queries | Visibility result buffers, `MTLCounterSampleBuffer` |
| ExecuteIndirect | `MTLIndirectCommandBuffer` or compute-generated indirect arguments |
| Geometry / hull / domain shaders | No direct equivalent; emulated with compute, mesh and post-tessellation vertex functions |
| DXR | `MTLAccelerationStructure`, intersection function tables |

## Risks

| Risk | Mitigation |
| --- | --- |
| Geometry shader, tessellation and stream-output emulation cost | Isolate in dedicated pipeline paths; measure early in Phase 4 with synthetic tests. |
| Runtime MSL compilation stalls | Disk cache in Phase 3, AIR emission and async compilation in Phase 8. |
| PE↔unix transition overhead | Batched command streams instead of per-call transitions. |
| Binding-model limits (Metal 31 buffer slots per stage) | Argument buffers from the start, not as a later optimisation. |
| Wine/macOS integration changes (`winemac.drv`, WoW64) | Track upstream Wine; keep the bridge layer small and versioned. |
| Undocumented driver behavior games rely on | Conformance tests from Wine and vkd3d-proton; per-title workarounds behind configuration. |

## Prior art

License determines reuse under [ADR 0001](adr/0001-license.md):

- [DXVK](https://github.com/doitsujin/dxvk) (zlib) — D3D8–11 on Vulkan.
- [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton) (LGPL-2.1) — D3D12 on Vulkan; test suite used in Phase 7.
- [DXMT](https://github.com/3Shain/dxmt) (LGPL-2.1) — D3D10/11 on Metal via Wine.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) (Apache-2.0) — Vulkan on Metal.
- [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) (NCSA) — DXIL format and bitcode reader.
- Wine, `wined3d` and `vkd3d` (LGPL-2.1) — reference behavior; test suites used in Phases 4–6.
- Apple Game Porting Toolkit (D3DMetal) and Metal Shader Converter — closed source; behavioral reference only.
