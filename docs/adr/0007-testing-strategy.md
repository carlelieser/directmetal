# 0007 — Testing strategy

Status: accepted

## Context

DirectMetal must reproduce Direct3D behavior that is defined partly by specification and partly by what Windows drivers do. Correctness needs an oracle; GPU tests need real Apple silicon hardware; most front-end logic does not need Wine to be exercised.

## Decision

### Oracle

Windows is the reference. A Windows CI job runs the same test programs on Microsoft's software rasterizer (WARP / Microsoft Basic Render Driver) and records their outputs: readback values, rendered images and API return codes. It also compiles the shader corpus with `fxc` and `dxc`, the compilers shipping games use. These recorded outputs are committed and are what macOS results are compared against.

### Test tiers

| Tier | Scope | Runs on | Gate |
| --- | --- | --- | --- |
| Unit | Format tables, state hashing, allocators, COM refcounting, IR passes | Hosted macOS runners | Every pull request |
| Shader | Bytecode front ends, IR passes, MSL emission; every emitted MSL source must compile with `xcrun metal` | Hosted macOS runners | Every pull request |
| Shader execution | Corpus shaders executed through the Metal backend, outputs compared with the Windows reference | Self-hosted Apple silicon | Every pull request |
| API | DirectMetal's own D3D9–12 and DXGI test programs, run natively through the in-process bridge and as PE binaries under Wine | Self-hosted Apple silicon | Every pull request |
| Conformance | Wine's `d3d9`, `d3d10core`, `d3d10`, `d3d11`, `dxgi` tests and vkd3d-proton's D3D12 tests, built from upstream and run unmodified | Self-hosted Apple silicon | Nightly; pass rate tracked per phase |
| Trace replay | API captures replayed under Wine: apitrace for D3D9–11, GFXReconstruct for D3D12; selected frames compared with the Windows replay | Self-hosted Apple silicon | Nightly |
| Performance | Frame-time benchmarks over the trace corpus | Dedicated self-hosted Apple silicon | Nightly; a regression above 5% in median or 99th-percentile frame time fails |
| Fuzzing | libFuzzer targets for every bytecode reader and the bridge's argument decoding | Self-hosted, continuous | Crashes filed as issues |

### Rules

- Tests exercise public interfaces: D3D/DXGI COM methods, the backend's C++ interface, the shader compiler's input and output. Internals are not tested directly.
- Image comparisons use a per-test tolerance (maximum per-channel difference and mismatching-pixel ratio) stored with the reference. Tolerances are tightened, never loosened without review.
- All GPU tests run with Metal API validation and shader validation enabled (`MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`). A validation error fails the test.
- Native unit and API tests also run under AddressSanitizer and UndefinedBehaviorSanitizer.
- Every bug fix includes a regression test at the lowest tier that reproduces it.
- The API inventory tooling reports, per interface method, whether it is implemented and whether any test calls it. Phase exits cite this report.

### Infrastructure

- C++ tests use GoogleTest through a Meson wrap.
- Shader tests use a DirectMetal test file format: HLSL source, input resources, dispatch or draw parameters and expected output. Bytecode and expected output come from the Windows job.
- A native in-process bridge, built from Phase 1, lets the front ends run in a macOS test process without Wine. It becomes the native SDK bridge in Phase 9.
- Hosted macOS runners do not provide a usable GPU, so all GPU tiers run on self-hosted Apple silicon Macs: at least one per supported macOS major version (14 and 26), plus one dedicated performance machine.
- Traces of commercial games are stored privately and never published. Public traces come from open-licensed samples such as [DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples) (MIT).

## Consequences

- Behavior is checked against Windows, not against another translation layer.
- The project needs Windows CI and self-hosted Mac hardware from Phase 1 onward.
- Wine and vkd3d-proton test sources are not vendored, consistent with [ADR 0001](0001-license.md).
