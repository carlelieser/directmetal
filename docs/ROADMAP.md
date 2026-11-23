# DirectMetal roadmap

DirectMetal implements Direct3D 9, 10, 11 and 12 (plus DXGI) directly on Metal, without an intermediate Vulkan layer. This document defines the architecture, the order of work, and the exit criteria for each phase.

## API surface

Counts come from the generated [API inventories](api/SUMMARY.md). Methods are those an interface declares itself.

| Group | Interfaces | Methods | Enums | Structs | Entry points |
| --- | ---: | ---: | ---: | ---: | ---: |
| Direct3D 9 | 20 | 223 | 52 | 44 | 9 |
| Direct3D 10 / 10.1 | 55 | 341 | 37 | 69 | 27 |
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
 │ bridge: directmetal.dll       │                   │ directmetal.so                  │
 └──────────────┬───────────────┘                   └────────────────┬────────────────┘
                │ shader bytecode                                     │
        ┌───────▼────────────────────────────────────────┐            │
        │ Shader compiler: D3D9 SM1–3 / DXBC SM4–5.1 /    │  MSL / AIR │
        │ DXIL SM6.x  →  DirectMetal IR  →  MSL or AIR     ├───────────►│
        └─────────────────────────────────────────────────┘
```

Layers, in dependency order (each depends only on layers above it):

1. **Platform bridge** — `directmetal.dll`, the only PE module that crosses into the unix side ([ADR 0008](adr/0008-wine-bridge-module.md)): PE↔unix call marshalling through Wine's unixlib interface, including WoW64 thunks for 32-bit applications, and access to the window's `NSView` via `winemac.drv`.
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
| Testing | Windows (WARP) output is the reference; tiered suites from unit to trace replay and performance. [ADR 0007](adr/0007-testing-strategy.md) |
| Bridge | One builtin `directmetal.dll` and one `directmetal.so` are the only PE↔unix boundary. [ADR 0008](adr/0008-wine-bridge-module.md) |
| Front ends | PE side; control calls plus a batched command stream; native builds through an in-process bridge. [ADR 0009](adr/0009-front-end-placement.md) |
| Native headers | Pinned, unmodified mingw-w64 headers at build time; SDK headers from win32metadata. [ADR 0010](adr/0010-native-windows-headers.md) |

## Testing

Defined in [ADR 0007](adr/0007-testing-strategy.md). Windows (WARP) is the oracle: the same test programs run there and their recorded outputs are the expected results on macOS.

| Tier | Gate |
| --- | --- |
| Unit, shader compile (MSL must compile with `xcrun metal`) | Every pull request, hosted runners |
| Shader execution, API tests (native bridge and under Wine) | Every pull request, self-hosted Apple silicon |
| Wine and vkd3d-proton conformance suites, trace replay | Nightly |
| Performance (fails on >5% frame-time regression) | Nightly, dedicated machine |
| Fuzzing of bytecode readers and bridge decoding | Continuous |

All GPU tests run with Metal API and shader validation enabled; any validation error is a failure.

## Phases

Each phase ends in a working, tested state. Phases 4–7 depend on 1–3; 5 depends on 4.

### Phase 0 — Foundations

- Meson build producing empty PE DLLs and a unix library loadable by Wine.
- CI: hosted macOS runners (build, lint, unit tests), a Windows job, and one self-hosted Apple silicon runner with Wine installed.
- GoogleTest wired into Meson; ASan/UBSan build configuration.
- Coding standards, formatter config, contribution guide, issue templates.
- API inventory tooling (`tools/api/`) reporting, per method, implementation status and test coverage (done; see [API inventories](api/README.md#implementation-status)).

**Exit:** `d3d11.dll` stub loads in Wine on macOS, logs a call that crosses into the unix side through `directmetal.dll`, and returns `E_NOTIMPL`; this is asserted by an API test running in CI, and the status report gate `python3 -I tools/api/report_status.py --require d3d11:stub>=1 --require d3d11:tested>=1` passes on that test's call logs.

### Phase 1 — Core runtime and Metal backend

Plan: [docs/plans/phase-1.md](plans/phase-1.md).

- COM base: `IUnknown`, internal/external refcounts, private data (`SetPrivateData`/`GetPrivateData`), `QueryInterface` tables, debug object names.
- Logging, per-application configuration, environment overrides, HUD hook points.
- Bridge: argument marshalling, WoW64 pointer thunks, command-stream batching to minimise PE↔unix transitions.
- Metal backend: `MTLDevice` selection and capability database, buffer/texture allocation over `MTLHeap`, upload/readback rings, CS-thread submission, `MTLSharedEvent` timelines.
- Native in-process bridge so front ends run in macOS test processes without Wine.
- Windows reference job: runs API test programs on WARP and records readbacks, images and return codes.
- Format tables for `DXGI_FORMAT` (123 values) and `D3DFORMAT` with support flags, substitutions (e.g. `D24_UNORM_S8_UINT` → `Depth32Float_Stencil8`, 3-component formats via buffers) and view-format compatibility.

**Exit:** unit tests cover every `DXGI_FORMAT` mapping and allocator behavior; an API test clears and reads back a texture and matches the Windows reference, with Metal validation enabled. Gated by `d3d11_clear_readback` (native, and under Wine for win64 and win32) and `python3 -I tools/api/report_status.py --require d3d11:tested>=8 --require common:implemented>=3` on its call logs.

### Phase 2 — DXGI and presentation

- `IDXGIFactory`–`IDXGIFactory7`, adapters from `MTLDevice`, outputs from `NSScreen`, mode lists.
- Swap chains (`IDXGISwapChain`–`IDXGISwapChain4`) on `CAMetalLayer`: flip/blit models, sync intervals, resize, frame latency waitable objects, fullscreen transitions.
- HDR10/scRGB output through EDR and colorspace metadata (later in phase).

**Exit:** API tests present at the display refresh rate and pass resize and fullscreen transitions; Wine's `dlls/dxgi/tests` pass at 90% or better, with every failure tracked as an issue.

### Phase 3 — Shader compiler infrastructure

- DirectMetal IR: typed SSA with structured control flow, resource bindings and I/O signatures.
- DXBC reader (SM4.0–5.0), signature and reflection parsing (`ID3D11ShaderReflection`).
- MSL emitter, binding layout generator, specialization for state-dependent variants (e.g. alpha test, sample count).
- On-disk shader and pipeline cache keyed by bytecode hash and state.
- Shader test format and corpus: HLSL compiled with `fxc`/`dxc` on the Windows job, executed on WARP for expected output, then executed through DirectMetal and compared.
- libFuzzer target for the DXBC reader.

**Exit:** every SM4/SM5 corpus shader (vertex, pixel, compute) emits MSL that compiles and produces output matching the Windows reference; the DXBC fuzzer runs for 24 hours without a crash.

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
- D3D9 shader bytecode (vs/ps 1.1–3.0) front end into DirectMetal IR, with its own fuzzer and corpus.
- D3D9 conventions: half-pixel offset, clip planes, point sprites, `D3DFMT` FOURCC extensions used by games (`INTZ`, `DF24`, `ATI1/ATI2`, `NULL`).
- 32-bit application support through WoW64.

**Exit:** Wine's `dlls/d3d9/tests` pass at 90% or better, with every failure tracked as an issue; three representative D3D9 titles render correctly.

### Phase 7 — Direct3D 12

- Device, command queues (direct/compute/copy), allocators and lists on `MTL4CommandQueue`/`MTL4CommandAllocator`/`MTL4CommandBuffer`; Metal 4 only (macOS 26).
- Descriptor heaps and root signatures on argument buffers and `MTL4ArgumentTable`; root constants and root descriptors.
- Heaps, placed and reserved resources on `MTLHeap` and sparse textures; residency via `MTLResidencySet`.
- Resource barriers and enhanced barriers mapped to Metal 4 barriers; fences on `MTLSharedEvent`.
- Pipeline state objects and pipeline state streams; `ExecuteIndirect` via indirect command buffers or compute-generated arguments; bundles.
- DXBC SM5.1 and DXIL SM6.0–6.6 front ends into DirectMetal IR (wave ops, 16-bit types, dynamic resources), with fuzzers and corpus coverage.
- Mesh shaders on Metal mesh pipelines; DXR 1.0/1.1 on Metal ray tracing. Variable-rate shading, sampler feedback and work graphs are reported unsupported through `CheckFeatureSupport`.

**Exit:** vkd3d-proton's D3D12 test suite passes at 90% or better, with every failure tracked as an issue; three representative D3D12 titles render correctly at feature level 12_0.

### Phase 8 — Performance and compatibility

- Asynchronous pipeline compilation, cache prewarming, state deduplication, encoder merging.
- Frame-time HUD, Metal GPU capture integration, per-title configuration database.
- Trace corpus: apitrace (D3D9–11) and GFXReconstruct (D3D12) captures replayed nightly under Wine, frames compared with the Windows replay.
- Performance baselines per trace on the dedicated machine.
- Public compatibility list generated from trace and conformance results.

**Exit:** nightly trace replay covers at least 20 titles across D3D9–12 with all compared frames within tolerance; performance gate active.

### Phase 9 — Distribution

- Release packaging consumable by Wine-based runtimes; versioning and changelog policy.
- Native macOS SDK: arm64 library with DirectX-compatible headers, sharing the backend and front ends.
- User and contributor documentation.
- The full test suite runs against the native SDK bridge as well as Wine.

**Exit:** a tagged release passes every gate in [Testing](#testing) on macOS 14 and 26.

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
| Undocumented driver behavior games rely on | Windows reference outputs, Wine and vkd3d-proton conformance suites, trace replay; per-title workarounds behind configuration. |
| GPU test infrastructure cost (self-hosted Macs, Windows CI) | Provisioned in Phase 0; hosted runners cover every tier that needs no GPU. |

## Prior art

License determines reuse under [ADR 0001](adr/0001-license.md):

- [DXVK](https://github.com/doitsujin/dxvk) (zlib) — D3D8–11 on Vulkan.
- [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton) (LGPL-2.1) — D3D12 on Vulkan; test suite used in Phase 7.
- [DXMT](https://github.com/3Shain/dxmt) (LGPL-2.1) — D3D10/11 on Metal via Wine.
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) (Apache-2.0) — Vulkan on Metal.
- [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) (NCSA) — DXIL format and bitcode reader.
- Wine, `wined3d` and `vkd3d` (LGPL-2.1) — reference behavior; test suites used in Phases 2 and 4–6.
- [apitrace](https://github.com/apitrace/apitrace) (MIT) and [GFXReconstruct](https://github.com/LunarG/gfxreconstruct) (MIT) — trace capture and replay.
- Apple Game Porting Toolkit (D3DMetal) and Metal Shader Converter — closed source; behavioral reference only.
