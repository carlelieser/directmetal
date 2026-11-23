# Phase 1 plan — core runtime and Metal backend

Scope and exit criteria are in the [roadmap](../ROADMAP.md#phase-1--core-runtime-and-metal-backend). This plan fixes the interfaces, splits the work into packages that can proceed in parallel, and defines the exit test.

## Architecture

The API front ends run on the PE side and reach the unix side only through `directmetal.dll`: a few synchronous control calls plus a batched, one-way command stream ([ADR 0009](../adr/0009-front-end-placement.md)). The same front-end sources build natively for macOS, where a native in-process bridge provides the `directmetal.dll` exports and calls the unix-side dispatcher directly; native builds use pinned Windows headers ([ADR 0010](../adr/0010-native-windows-headers.md)).

## Interfaces

Landed before any package starts. Packages implement or consume them; changing one needs integrator review.

| Interface | Headers | Consumers |
| --- | --- | --- |
| I1 Backend types | `src/backend/include/directmetal/backend/{types,pixel_format}.hpp` | Backend, bridge protocol, formats |
| I2 Backend interface | `src/backend/include/directmetal/backend/{device,commands}.hpp`; allocators in `src/memory/include/directmetal/memory/` | Backend, host, format GPU test |
| I3 Bridge protocol v3 | `src/bridge/include/directmetal/bridge/{protocol,control,commands,client}.hpp`; typed client in `src/bridge/client/include/directmetal/bridge/` | Bridge, host, front ends |
| I4 COM base | `src/frontends/com/include/directmetal/com/` | Front ends |
| I5 Format queries | `src/formats/include/directmetal/formats/formats.hpp` | Front ends, backend, format GPU test |
| I6 Runtime | `src/runtime/include/directmetal/runtime/{log,config,hud}.hpp` | Every component |
| I7 Host dispatch | `src/unix/include/directmetal/host/dispatch.hpp` | Host, native bridge |

Rules that span interfaces:

- Wire structs use fixed-width fields, carry addresses as `u64`, zero reserved fields, and assert size and offsets in every build (win32, win64, macOS).
- Memory the PE side reads or writes is allocated on the PE side (`directmetal_bridge_allocate_shared`, below 4 GiB under WoW64) and imported by the unix side without copying. Imports require address and size to be multiples of 16 KiB (`shared_memory_alignment`); the PE side may free imported memory only after `release_memory` returns, which waits for all GPU work using it.
- The unix side creates the Metal device lazily on the first device call, never while `directmetal.so` is being loaded: the first `MTLCreateSystemDefaultDevice` inside Wine takes 50–60 ms.
- Front ends flush the stream only when a chunk is full, on an explicit flush, before any CPU wait, and at submit.
- One stream-consumer thread per stream (device context or queue). Stream threads never call into Wine.
- Handles are assigned by the unix side, synchronously, and are opaque to the PE side.
- Front-end code never uses `wchar_t` or `long` for wire or ABI data.

## Packages

Each package owns the listed paths; no two packages in a wave touch the same file. Tests follow the [ADR 0007](../adr/0007-testing-strategy.md) tiers: suite `unit` needs no GPU; suites `gpu` and `api` run on Apple silicon with Metal validation enabled. `tools/ci/run_local.sh` must pass before merge.

### Wave 1

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| A. Format tables | `src/formats/**`, `tests/unit/formats/**` | Every `DXGI_FORMAT` and `D3DFORMAT` entry; a coverage check that diffs table names against `docs/api/dxgi.json` and `docs/api/d3d9.json`; view-compatibility matrix; substitutions (D24S8, B8G8R8X8, R32G32B32) | L |
| B. Allocators, backend device and resources | `src/memory/**`, `src/backend/**` except `src/backend/commands/`, `tests/unit/memory/**`, `tests/gpu/backend_memory/**` | Allocator alignment, overlap, coalescing, exhaustion, ring wrap and retire order; placement heaps, heap growth, dedicated allocations, no range reuse before completion, zero-copy import | L |
| C. Bridge codec and transports | `src/bridge/**`, `tests/unit/bridge/**` | Round trip of every opcode; rejection of truncated, misaligned, oversize and unknown commands with a sticky error; corpus replay; native transport reaching `host::dispatch` | M–L |
| D. Windows reference job and exit program | `.github/workflows/reference.yml`, `toolchains/windows-llvm-mingw.ini`, `tools/reference/**`, `tests/api/harness/**`, `tests/api/programs/**`, `tests/reference/**` | Comparator and freshness-check unit tests; one dispatched run whose output is committed | M |

### Wave 2

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| E. Native Windows headers and COM base | `src/win32/**`, `subprojects/mingw-w64-headers.wrap`, `src/frontends/com/**`, `tests/unit/com/**` | QueryInterface tables, public and private refcount lifetimes, concurrent AddRef/Release, every documented private-data return code, debug-name callback; `<d3d11.h>` compiling on macOS x86_64 and arm64, win32 and win64 | M–L |
| F. Backend commands, timelines, submission | `src/backend/commands/**`, `tests/gpu/backend_commands/**` | Clear-then-copy readback per format kind; buffer↔texture copies with pitches; timeline signal, wait and timeout; device-lost path | L |
| G. Host | `src/unix/**`, `tests/unit/host/**`, `tests/gpu/host/**` | Every control call through `host::Host` with a fake device provider; bad and stale handles; retired sequencing; decode errors; clear and readback through the protocol on the real backend | L |
| H. Runtime | `src/runtime/**`, `tests/unit/runtime/**` | Configuration precedence, application sections, malformed lines, unknown keys, `DIRECTMETAL_LOG` grammar, sinks, counter atomicity | M |

### Wave 3

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| I. D3D11 minimal slice | `src/frontends/d3d11/**` | `D3D11CreateDevice` with a null adapter; `CreateTexture2D`, `CreateRenderTargetView`, `ClearRenderTargetView`, `CopyResource`, `Map`/`Unmap`, `GetImmediateContext`, device private data and `IUnknown` implemented, everything else `DM_STUB`; builds as a PE DLL and a native static library | L |
| J. Staging rings and WoW64 proof | `src/bridge/client/staging_ring*`, `tests/api/programs/bridge_ring_roundtrip.cpp` | Upload, copy and readback through the ring, natively and under Wine for win64 and win32 | M |
| K. API runners and gates | `tests/api/run_api_test.py`, `tests/api/meson.build`, `tests/gpu/validation_canary.mm`, `tools/ci/run_local.sh`, `.github/workflows/ci.yml` | Runner unit tests; a validation canary that must abort | M |
| L. Format GPU check | `tests/gpu/formats/**` | Every format claiming support on the running GPU family is created with its claimed usages, cleared and copied where render-target support is claimed, with no validation errors | M |

### Wave 4

The integrator wires package I into package K's runners for the exit test, updates the status gate and cites the gates in the roadmap.

Critical path: interfaces → B and C → F and G → I → wave 4.

## Exit test

`tests/api/programs/d3d11_clear_readback.cpp` runs headless D3D11: create a device, a render-target texture and a staging texture; clear through a render-target view; copy; map and read back. No DXGI, swap chain or shaders are involved.

- Formats: `R8G8B8A8_UNORM`, `R8G8B8A8_UNORM_SRGB`, `B8G8R8A8_UNORM`, `R10G10B10A2_UNORM`, `R16G16B16A16_FLOAT`, `R32G32B32A32_FLOAT`, `R32G32B32A32_UINT`, 8×8, plus a mip-1 clear through `MipSlice = 1`. Colors avoid rounding boundaries.
- Recorded per step: the `HRESULT`s, the final `Release` count, two expected failures (`Map` on a DEFAULT texture, `CreateTexture2D` with zero width) and the readback as tightly packed hex. JSON goes to stdout, logs to stderr.
- On Windows (`reference.yml`, `workflow_dispatch` only, `windows-2025`, read-only permissions): the program runs on WARP for x64 and x86, which must agree. The artifact records the run, commit, image, Windows build, `d3d11.dll` version and a hash of the program and harness sources.
- References are regenerated with `gh workflow run reference.yml --ref <branch>` and `tools/reference/update.py --run <id>`, which checks the source hash and writes `tests/reference/<test>/reference.json`. The reference is committed with the test change and reviewed as a diff. `tolerance.json` sits beside it; loosening it needs explicit review.
- On macOS (suite `api`): natively for x86_64, arm64 and the UBSan build, and under Wine for win64 and win32, all with `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1` and `MTL_DEBUG_LAYER_ERROR_MODE=assert`. `HRESULT`s must match exactly; readbacks match within 0 for integer and float formats and 1 for 8-bit UNORM and sRGB.
- A Metal validation assertion aborts the whole Wine process (the PE side sees `0xc0000005`), so the runners treat any abnormal exit as a failure, not only a mismatched result.
- Status gate: `--require d3d11:tested>=8 --require common:implemented>=3`.

## Decisions

| Decision | Choice |
| --- | --- |
| Front-end placement | PE side, batched stream plus control calls ([ADR 0009](../adr/0009-front-end-placement.md)) |
| Native bridge | Goes through the full protocol, so the decoder is exercised by every native test |
| Windows headers for native builds | Pinned, unmodified mingw-w64 headers fetched at build time ([ADR 0010](../adr/0010-native-windows-headers.md)) |
| Backend interface | Virtual, so the host can run against a fake device without a GPU |
| Handles | Assigned by the unix side, synchronously |
| Stream threads | One per stream |
| References | JSON with hex readbacks, regenerated from a workflow artifact; the workflow has no write permission |
| DXGI | Absent in Phase 1: a non-null adapter returns `E_INVALIDARG`, `QueryInterface` for `IDXGIDevice` returns `E_NOINTERFACE` |

## Risks

| Risk | Mitigation |
| --- | --- |
| Zero-copy import of Wine-allocated memory under Rosetta 2 | Retired: `VirtualAlloc` memory wrapped with `newBufferWithBytesNoCopy` works as blit source and destination for win64 and win32 under Wine 11.21 with the debug layer in assert mode, and the PE side sees GPU writes after completion. Wrapping costs about 0.05 ms against 35–76 ms for a copying buffer. Pages are 4 KiB on the PE side (64 KiB allocation granularity) and 16 KiB for the kernel, so imports rely only on 16 KiB alignment |
| Metal validation behaves differently in an x86_64 Wine process | A validation canary runs under Wine as well as natively |
| AddressSanitizer is unavailable on macOS 27 | UBSan, `libgmalloc` for the codec and allocator tests, deterministic corpus replay |
| WARP and Apple GPUs round clears differently | Colors away from rounding boundaries; per-format tolerances that are only tightened |
| Runner image changes WARP output | Pinned image; versions recorded with each reference; diffs reviewed |
| 64-bit atomics in shared blocks from 32-bit processes | `std::atomic_ref` on 8-byte-aligned fields, asserted lock-free |
| Stream threads at DLL detach | Never joined in `DllMain`; drained on `destroy_device` and `close_stream` |
| Large format tables against the 300-line file limit | Tables split by format family |
