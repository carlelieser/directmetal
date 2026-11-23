# Phase 2 plan — DXGI and presentation

Scope and exit criteria are in the [roadmap](../ROADMAP.md#phase-2--dxgi-and-presentation). This plan fixes the interfaces, splits the work into packages that can proceed in parallel, and defines the exit test.

## Architecture

`dxgi.dll` is a second PE front end beside `d3d11.dll`. Both reach the unix side only through `directmetal.dll` ([ADR 0008](../adr/0008-wine-bridge-module.md), [ADR 0009](../adr/0009-front-end-placement.md)). A swap chain is a unix-side object owned by the device whose stream presents it. Presenting is a stream command, so it is ordered after the rendering it shows. The unix side copies the back buffer into a `CAMetalDrawable` on the stream thread and commits that copy as a submit.

```
 app ─ IDXGISwapChain::Present ─► dxgi.dll ── IDirectMetalPresentDevice (COM) ──► d3d11.dll / d3d10_1.dll
                                     │  control calls: displays, swap chains, waits      │ stream: …, present
                                     ▼                                                   ▼
                          directmetal.dll ════════ unixlib ════════► directmetal.so: host ─► backend presenter
                                                                       │ surface resolver      │ CAMetalLayer / offscreen
                                                                       ▼                       ▼
                                                   Wine's AppKit window for the HWND   drawable ← blit or present pass
```

Getting a layer for an HWND ([ADR 0011](../adr/0011-window-surfaces.md), new; amends [ADR 0003](../adr/0003-language.md)):

- Wine 11.21 exposes no API for this. `winemac.drv` has no PE exports, and `winemac.so` exports only its unix call tables. Its Metal surface machinery is internal. Vulkan-driver entry points such as `__wine_get_vulkan_driver` are version-checked internal ABIs described only by LGPL headers, so they are not used.
- On the PE side the front end sends the HWND, its `GA_ROOT` ancestor and `GetDesktopWindow()`. It also sends the target's client rect and the root's window rect, both in Win32 virtual-screen pixels. Under a Wine virtual desktop, the desktop window is the only one with an AppKit window.
- On the unix side, `src/unix/surface/` (Objective-C++) finds the `NSWindow` whose `hwnd` property equals the root, or failing that the desktop window. It uses only the Objective-C runtime and documented AppKit calls. Wine's AppKit window class exposes this property; the property name is observed behavior, and no Wine source is copied.
- The unix side converts the client rect to that window's content-view coordinates. Wine places the Win32 origin at the top-left of the primary display. The pixel-to-point scale is the root window's pixel size over its `NSWindow` frame in points, so `RetinaMode` is handled.
- The unix side adds a DirectMetal-owned layer-hosting `NSView` with a `CAMetalLayer`. The view returns `nil` from `hitTest:`, so input still reaches Wine's views.
- AppKit work runs on the main thread through `dispatch_async` to the main queue, which Wine's Cocoa event loop services. The caller waits at most 2 s. On a timeout or a missing window it presents offscreen and logs one `warn` line per swap chain. The next Present retries when the window appears.
- In native builds the HWND parameter is an `NSView*`. The layer fills the view and follows its bounds. Test harnesses create the `NSWindow`.

Pacing and latency:

- `CAMetalLayer.displaySyncEnabled` is set from the sync interval: 1 for intervals 1–4, 0 for interval 0. Intervals 2–4 use `presentDrawable:afterMinimumDuration:` set to the interval times the display's refresh period.
- `nextDrawable` runs on the stream thread (`allowsNextDrawableTimeout`, 1 s). When the window is occluded or the surface is lost, the frame is retired without acquiring a drawable.
- Each swap chain has a present timeline. Frame *n* signals *n* when it retires: on the drawable's presented handler (a `presentedTime` of 0 means dropped), on command-buffer completion when offscreen, or immediately when skipped.
- On the PE side, Present waits until frame *n − max latency* has retired, using the timeline mirror and `wait_timeline`. A waitable swap chain does not block in Present. Its Win32 semaphore is released by one PE waiter thread per swap chain, which blocks in `wait_timeline` for each retired frame. Unix threads never call into Wine.
- Vblank timing (`WaitForVBlank`, frame statistics) comes from a `CADisplayLink` from `-[NSScreen displayLinkWithTarget:selector:]`, scheduled on a backend-owned run-loop thread. Offscreen surfaces and sessions without a display use a synthetic clock, 60 Hz by default. `CVDisplayLink` is deprecated and not used.

The copy from back buffer to drawable:

- A blit copy is used when the drawable format matches the back buffer bit for bit: `BGRA8Unorm` (including `_SRGB`, copied as raw bits), `RGB10A2Unorm` or `RGBA16Float`, at the same size.
- Otherwise a hand-written MSL pass runs: a full-screen triangle that reads texels without filtering. This covers `R8G8B8A8_*` swizzle and the gamma LUT, and later HDR encoding. Multisampled blit-model buffers resolve through the render pass's store action. The MSL is a raw string literal in `src/backend/present/`, compiled once per device with `newLibraryWithSource:`. It is fixed-function code, does not depend on the Phase 3 compiler and owns no compiler paths.
- DXGI scaling maps to `contentsGravity` (`STRETCH` → resize, `NONE` → top-left, `ASPECT_RATIO_STRETCH` → resize-aspect), so the compositor scales. `drawableSize` is the buffer size.
- SDR layers leave `colorspace` nil, which means no color matching, as on Windows. HDR10 uses `kCGColorSpaceITUR_2100_PQ` with `RGB10A2Unorm` and `CAEDRMetadata`. scRGB uses `kCGColorSpaceExtendedLinearSRGB` with `RGBA16Float` and `wantsExtendedDynamicRangeContent`.

Displays and outputs:

- The unix side enumerates displays with CoreGraphics: `CGGetActiveDisplayList`, `CGDisplayBounds`, display modes and refresh rates, and the Metal device per display. CoreGraphics is thread-safe. The main thread is used only for `NSScreen` data: name, `maximumFramesPerSecond` and EDR headroom.
- Under Wine, Wine's user32 is the authority on display state, so the PE side takes `HMONITOR`, `DeviceName`, mode lists and `DesktopCoordinates` from GDI (`EnumDisplayMonitors`, `EnumDisplaySettingsExW`). It matches each monitor to a unix display by bounds.
- Native builds use the unix data and synthesize `HMONITOR` values.
- Every display belongs to adapter 0: Apple silicon has one GPU.

Fullscreen is emulated. `SetFullscreenState(TRUE)` makes the window a borderless window covering its output. The display mode never changes and display gamma is never written. `SetGammaControl` feeds the present pass's LUT, and `DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH` is accepted and ignored.

D3D11 interaction:

- The D3D11 device implements `IDXGIDevice`–`IDXGIDevice4`, and textures implement `IDXGISurface1`–`2` and `IDXGIResource1`. These come from mixins in `src/frontends/dxgi/{device,surface}/`, which d3d11 links statically.
- With a null adapter, `D3D11CreateDevice` calls `CreateDXGIFactory1` and uses adapter 0. `d3d11.dll` imports `dxgi.dll`, as on Windows.
- An explicit adapter must answer `IDirectMetalAdapter` and come with `D3D_DRIVER_TYPE_UNKNOWN`.
- `D3D11CreateDeviceAndSwapChain` is device creation followed by the adapter's factory `CreateSwapChain`.
- Swap chains reach the device only through the private COM interface `IDirectMetalPresentDevice`. It creates back buffers as `ID3D11Texture2D` and records `present` into the device's stream.

Runtime state (resolves the Phase 1 deferred decision): `directmetal.dll` owns the process's runtime state, as does the native bridge in macOS builds. That state is log levels, the log sink and file, HUD counters and `end_frame`, and the resolved configuration. Every PE front end loads `directmetal.dll`, and only it exists once per process. Front ends keep using `log.hpp`, `config.hpp` and `hud.hpp`, now implemented by `runtime_client`, which forwards to C exports. `runtime_core` holds the implementation. Call logs stay per module; `report_status.py` already merges them.

## Interfaces

Landed before any package starts, in the joint interfaces PR with Phase 3 ([schedule](phases-2-3.md)), together with ADR 0011 (written from the spike findings), the empty per-package subdirectories and their `meson.build` hooks. Changing one needs integrator review.

| Interface | Headers | Consumers |
| --- | --- | --- |
| I8 Bridge protocol v4 | `protocol.hpp` (`protocol_version = 4`, codes 16–25 appended; Phase 3 takes 26–28), new `presentation.hpp` (display and swap-chain parameter structs, `SurfaceTarget`, `SwapChainConfig`, `SwapChainControl`), `commands.hpp` (`Opcode::present` appended, `stream_version = 2`; layout asserts move to `command_layouts.hpp` to stay under 300 lines) in `src/bridge/include/directmetal/bridge/` | Bridge, host, DXGI, D3D11, D3D10 |
| I9 Backend presentation | `src/backend/include/directmetal/backend/present.hpp` (`SurfaceDesc`, `PresenterConfig`, `Presenter`, `PresentRequest`, `DisplayInfo`, `DisplayMode`, `wait_vblank`); `Device::create_presenter` in `device.hpp`; `Queue::present` in `commands.hpp`; private seam `src/backend/present/encode.hpp` (encodes into the queue's current `MTLCommandBuffer`; `.mm` only) | Backend lanes B and F, host |
| I10 DXGI private interfaces | `src/frontends/dxgi/include/directmetal/dxgi/{present_device,adapter_identity,device_mixin,surface_mixin}.hpp` | DXGI, D3D11, D3D10; D3D12 queues in Phase 7 |
| I11 Runtime shared state | `src/runtime/include/directmetal/runtime/shared_state.hpp` (C ABI: `directmetal_runtime_state`, `_log_write`, `_config_value`, `_hud_end_frame`) | Every PE module, native bridge |
| I12 Surface resolver | `src/unix/include/directmetal/host/surfaces.hpp` (`SurfaceResolver`, injected into `host::Host` like `DeviceProvider`) | Host, `directmetal.so`, native bridge |
| I13 Harness windows | `tests/api/harness/include/directmetal/api_harness/window.hpp` (`run_with_window`, `pump_messages`, `wait_handle`) | API programs |

Protocol v4 additions. Every struct follows the Phase 1 wire rules: fixed width, `u64` addresses, zeroed reserved fields, size and every offset asserted in win32, win64 and macOS builds.

| Code | Call | Parameters (out marked) |
| --- | --- | --- |
| 16 | `enumerate_displays` | index → `DisplayDesc` out: display id, adapter registry id, flags (primary, built-in, EDR), bounds (points, top-left global), pixel size, refresh and max refresh (millihertz), rotation, EDR headroom (×1000), UTF-8 name[64]. `status_no_more_entries` past the end |
| 17 | `enumerate_display_modes` | display id, index → width, height (pixels), refresh (millihertz), flags (current, native, scaled) |
| 18 | `query_adapter_memory` | registry id → current allocated bytes, budget |
| 19 | `wait_vblank` | display id, timeout_ns → vblank count, host time ns |
| 20 | `create_swap_chain` | device, `SurfaceTarget`, `SwapChainConfig`, `control_address` (64-byte-aligned PE `SwapChainControl`) → swap chain, present timeline |
| 21 | `resize_swap_chain` | swap chain, `SwapChainConfig`. The client flushes the device's stream first; the host decodes every queued chunk and waits for the swap chain's presents to complete before applying |
| 22 | `set_swap_chain_target` | swap chain, `SurfaceTarget`. Moves the layer; does not drain the stream |
| 23 | `set_swap_chain_gamma` | swap chain, address and size of a PE-side `DXGI_GAMMA_CONTROL`-shaped table, copied during the call; 0 clears |
| 24 | `set_swap_chain_hdr_metadata` | swap chain, HDR10 primaries, white point, luminances, MaxCLL/MaxFALL; type 0 clears |
| 25 | `destroy_swap_chain` | swap chain. The client flushes first; returns once every present has retired and the layer is detached; the `SwapChainControl` may then be freed |

- `SurfaceTarget` holds kind (`offscreen`, `win32_window`, `ns_view`), window, root window, desktop window, client rect and root window rect.
- `SwapChainConfig` holds `PixelFormat`, width, height, sample count, drawable count (2 or 3), scaling, alpha mode, `is_tearing_allowed` and color space (the `DXGI_COLOR_SPACE_TYPE` value).
- `PresentCommand` holds swap chain, source texture, source slice, sync interval (0–4), flags (allow tearing) and frame (strictly +1 per swap chain).
- `SwapChainControl` is 64 bytes, written only by the unix side, and read through `std::atomic_ref`. It holds `retired_frame` (the present timeline's mirror, stored last with release semantics), `presented_frame`, `presented_time_ns`, `presented_vblank`, `dropped_frames`, flags (`is_occluded`, `is_offscreen`, `needs_target`, `is_surface_lost`), the view size in pixels, a sticky status and a reserved field.

Rules that span interfaces (Phase 1 rules continue to apply):

- `present` commits like `submit`: it encodes the copy into the current command buffer, presents the drawable and signals the stream's progress timeline with the next value. Clients count it as a submit. Deferred frees therefore cover the back buffer read.
- The host validates `present` and makes violations sticky stream errors:
  - the swap chain and source texture belong to the stream's device
  - the source has `render_target` and `shader_read` usage
  - the source's format and size match the swap chain's current config
  - the frame number is the previous one plus 1
- Present timelines are never named in `destroy_timeline` or `destroy_object`; `destroy_swap_chain` and `destroy_device` retire them. Swap chains are destroyed only with `destroy_swap_chain` or with their device.
- A sticky stream error also fails waits on the device's present timelines with `status_device_removed`.
- Objects never cross a PE module boundary as C++ types; each DLL links its own static C++ runtime. Front ends exchange COM interfaces (public, or DirectMetal-private with fixed-width arguments) and wire structs from I8. Private IIDs change whenever their methods change.
- AppKit calls run on the main thread, with bounded waits. Stream threads and Metal completion threads never wait on the main thread.
- The unix side reads only environment variables. `DIRECTMETAL_PRESENT_OFFSCREEN=1` forces offscreen surfaces.
- Display gamma, display modes and display capture are never changed by DirectMetal.

## Packages

Each package owns the listed paths; no two packages in a wave touch the same file.

Test suites:

- `unit` needs no GPU.
- `gpu` and `api` run headless on Apple silicon with Metal validation, on the CI runner as well.
- `present` needs a logged-in GUI session and runs from `tools/ci/run_local.sh present`.
- `conformance` is described under Exit test.

Every GPU or Wine run goes through `tools/with_gpu_lock.sh` with `is_parallel: false`, bounded frame counts and timeouts. No test provokes a GPU fault, an infinite wait or a hang. `tools/ci/run_local.sh` must pass before merge, now including `present`.

### Wave 0 — spikes (throwaway; findings go into ADR 0011)

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| X1. Wine window spike | scratch branch only | In Wine 11.21 in a GUI session:<br>• the `hwnd` lookup, layer attach and one present for `WS_OVERLAPPEDWINDOW`, `WS_POPUP`, a child window, a virtual desktop and `RetinaMode` on/off<br>• main-thread dispatch latency<br>• `Graphics=null` driver behavior<br>• whether a virtual desktop emulates mode changes, decided safely: first check that `EnumDisplaySettingsExW` there returns the virtual desktop's synthetic mode list, not the real display's, and change a mode only if it does | S |
| X2. Pacing spike | scratch branch only | x86_64 (Rosetta) and arm64:<br>• `displaySyncEnabled`, `afterMinimumDuration:` and presented-handler timing on the developer's displays, including ProMotion<br>• `CGDisplayModeGetRefreshRate` versus `NSScreen.maximumFramesPerSecond`<br>• `CADisplayLink` on a non-main run loop<br>• `nextDrawable` when occluded<br>• whether `CAMetalLayer` works in the CI runner's session-less context | S |

### Wave 1

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| A. Shared runtime state (implements I11) | `src/runtime/**`, `src/bridge/pe/**`, `src/bridge/native/runtime_exports.cpp`, `tests/unit/runtime/**`, `tests/api/runtime/**` | `runtime_client` against a fake export table. A Wine program loading two test DLLs that share one level table, one log file and one HUD counter set through `directmetal.dll`. `DIRECTMETAL_LOG` applied once. Configuration warnings logged once | M |
| B. Backend presenter (implements I9 except displays) | `src/backend/present/**`, `src/backend/commands/present.mm`, `tests/gpu/present/**` | Offscreen presenter in suite `gpu`:<br>• blit path for `BGRA8`, `RGB10A2` and `RGBA16F`; MSL path for `RGBA8` swizzle and sRGB bits; MSAA resolve<br>• gamma LUT<br>• retire order, sync intervals 0–4 on the synthetic clock, drawable backpressure<br>• reconfiguring between frames; destruction with frames in flight<br>Layer presenter on a test `NSWindow` in suite `present`. No validation errors | L |
| C. Host presentation | `src/unix/host/**` except `src/unix/host/shaders/**`, `src/bridge/codec/**`, `tests/unit/host/**` except `tests/unit/host/shaders/**`, `tests/unit/bridge/**` | Every new call against a fake presenter and resolver:<br>• `present` validation failures; `present` counted in progress numbering<br>• `resize_swap_chain` draining; teardown order across `destroy_swap_chain` and `destroy_device`<br>• `SwapChainControl` writes; waits failing after a sticky error<br>• codec round trip of `present`; `stream_version` 1 chunks rejected; corpus additions | L |
| D. DXGI factory, adapters, outputs | `src/frontends/dxgi/{factory,adapter,output,entry}/**`, `src/frontends/dxgi/meson.build`, `src/frontends/meson.build`, `src/pe/**`, `tests/unit/dxgi/{factory,adapter,output}/**` | With a fake bridge and fake GDI source:<br>• `IDXGIFactory`–`7` QueryInterface chains<br>• `EnumAdapters`, `EnumAdapters1`, `EnumAdapterByLuid`, `EnumAdapterByGpuPreference`, `EnumWarpAdapter`<br>• adapter descs (vendor `0x106B`; dedicated memory = recommended working set; LUID from `D3DKMTOpenAdapterFromGdiDisplayName` under Wine, from the registry id natively)<br>• `CheckInterfaceSupport`, `QueryVideoMemoryInfo`<br>• output descs; mode lists per display format with scaling and interlaced flags<br>• `FindClosestMatchingMode` cases written from the DXGI documentation<br>• `GetParent` chains, private data<br>`dxgi.def` exports `CreateDXGIFactory`, `CreateDXGIFactory1`, `CreateDXGIFactory2` and `DXGIGetDebugInterface1` | L |

### Wave 2

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| E. Window surfaces (implements I12) | `src/unix/surface/**`, `src/unix/unixlib.cpp`, `src/unix/meson.build`, `src/bridge/native/**` except `runtime_exports.cpp`, `tests/api/surface/**` | A bridge-level Wine program in suite `present`. For each window kind from X1 it creates a swap chain, clears and presents, then checks `SwapChainControl`: not offscreen, view size equals the client size, `presented_frame` advances. It also covers window destruction while presenting (`is_surface_lost`, frames still retire) and `needs_target` after Wine recreates the window. Natively, the `ns_view` pass-through resolver is covered | M |
| F. Backend displays and vblank | `src/backend/displays/**`, `tests/gpu/displays/**` | Display count equals the active display list; primary first; the mode list contains the current mode; nonzero refresh; EDR headroom ≥ 1. `wait_vblank` intervals within 10 % of the refresh period (suite `present`). The synthetic clock headless (suite `gpu`) | M |
| G. D3D11 ↔ DXGI | `src/frontends/d3d11/**`, `src/frontends/dxgi/{device,surface}/**`, `tests/unit/d3d11/**` | • `IDXGIDevice`–`4` from the device<br>• `GetAdapter` matches `EnumAdapters(0)` by desc and LUID<br>• explicit-adapter rules; foreign adapters rejected<br>• texture `IDXGISurface1` and `IDXGIResource1`<br>• `SetMaximumFrameLatency` range<br>• `D3D11CreateDeviceAndSwapChain` argument rules (fake factory)<br>• `IDirectMetalPresentDevice` back buffers; present recorded and counted as a submit<br>• the Phase 1 `E_INVALIDARG` and `E_NOINTERFACE` rules removed | L |
| H. Harness windows and runners | `tests/api/harness/**`, `tests/api/run_api_test.py`, `tests/api/run_under_wine.py`, `tests/api/tests/**`, `tools/ci/run_local.sh` | Runner unit tests:<br>• GUI-session detection (`launchctl managername` reports `Aqua`)<br>• `--graphics-driver null` and `--virtual-desktop WxH` (registry written in the fresh prefix)<br>• `--forbid-stderr` (the offscreen warning in suite `present`)<br>• timing sections checked against bounds, not the reference<br>• overrides `dxgi,d3d11,d3d10_1,d3d10core=n;d3d12,d3d12core,d3d10=;directmetal=b`<br>Harness windows on Windows and Wine (`CreateWindowExW` and a message pump) and natively (`window_native.mm`: `NSApplication` on the main thread, test body on a worker thread). `run_local.sh` gains `present`, which fails rather than skips outside a GUI session unless `DIRECTMETAL_SKIP_PRESENT=1` prints a warning, and an opt-in `conformance-dxgi` check | M |

### Wave 3

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| I1. Swap chains | `src/frontends/dxgi/swap_chain/**`, `tests/unit/dxgi/swap_chain/**` | With a fake present device and bridge:<br>• `CreateSwapChain`, `CreateSwapChainForHwnd` and `CreateSwapChainForComposition` validation (swap effect × buffer count × format × flags × sample count); `ForCoreWindow` unsupported<br>• `GetBuffer` rules per model; for D3D10 and D3D11 flip models, buffer 0 is a stable texture<br>• `Present` and `Present1` flags: `TEST`, `DO_NOT_WAIT` → `DXGI_ERROR_WAS_STILL_DRAWING`, tearing rules, sync interval > 4 → `DXGI_ERROR_INVALID_CALL`<br>• occlusion status<br>• `ResizeBuffers` and `ResizeBuffers1` with outstanding references → `DXGI_ERROR_INVALID_CALL`; zero size → client size<br>• `GetDesc`, `GetDesc1`, `GetHwnd`, background color, rotation<br>• `hud::end_frame` at every Present | L |
| I2. Latency and statistics | `src/frontends/dxgi/{latency,platform}/**`, `tests/unit/dxgi/latency/**` | Blocking latency at device and swap-chain level. Waitable object: initial count, one release per retired frame, `SetMaximumFrameLatency`; Windows semaphore plus waiter thread, joined at swap-chain release, never in `DllMain`; native waitable plus `directmetal_dxgi_native_wait`. `GetFrameStatistics` and `GetLastPresentCount` from `SwapChainControl`; host ns → QPC offset | M |
| M. D3D10.1 bring-up slice | `src/frontends/d3d10/**`, `tests/unit/d3d10/**` | `D3D10CreateDevice1` and `D3D10CreateDeviceAndSwapChain1` in a new `d3d10_1.dll`, plus `D3D10CoreCreateDevice` in `d3d10core.dll`. Built on the D3D11 front end: device, `CreateTexture2D`, `CreateRenderTargetView`, `ClearRenderTargetView`, `OMSetRenderTargets` (state only), `CopyResource`, texture `Map`, `Flush`, `GetDeviceRemovedReason`, `IDXGIDevice`, `IDirectMetalPresentDevice`; everything else `DM_STUB`. Phase 5 inherits and completes these paths | L |
| K. Headless DXGI API programs | `tests/api/dxgi/**`, `tests/api/programs/manifest.tsv`, `tests/reference/dxgi_*/**` | `dxgi_enumerate` (shape of adapters, outputs and modes, not values), `dxgi_swap_chain_offscreen` (composition swap chain: 60 presents, frame latency, `ResizeBuffers`, back-buffer readback before Present) and `dxgi_d3d11_interop`. All run natively, under Wine with `--graphics-driver null` and on WARP for references; skipped until wired, as in Phase 1 | M |

### Wave 4

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| J. Fullscreen and window association | `src/frontends/dxgi/fullscreen/**`, `tests/unit/dxgi/fullscreen/**` | Emulated `SetFullscreenState`, `GetFullscreenState`, `ResizeTarget`, `GetContainingOutput`, the default target. Window style and rect saved and restored. `MakeWindowAssociation` and Alt+Enter via a per-thread `WH_GETMESSAGE` hook. `IDXGIOutput::TakeOwnership` and `ReleaseOwnership` (no effect on the display). Output `SetGammaControl` and `GetGammaControlCapabilities` through `set_swap_chain_gamma`. A fake window system natively | M |
| N. Presentation suite | `tests/api/present/**`, `tests/reference/dxgi_present_*/**` | The exit programs below, natively (x86_64, arm64, UBSan) and under Wine (win64, win32), in suite `present` | M |
| L. Conformance tooling | `tools/conformance/**`, `tests/conformance/**`, `.github/workflows/nightly.yml` | Fetch-and-build unit tests; output parser against recorded winetest logs; gate logic (pass rate, unexpected failures, stale expectations, crashes, skip baseline). One headless build and run (below) | M |

### Wave 5

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| O. Conformance triage | `tests/conformance/dxgi/expectations.tsv` | Runs both conformance configurations and files one issue per failing check. Fixes land as separate reviewed changes, not in this package | M |
| P. HDR10 and scRGB | `src/frontends/dxgi/color/**`, `src/backend/present/hdr*`, `tests/gpu/present/hdr/**`, `tests/api/present/hdr/**` | `CheckColorSpaceSupport`, `SetColorSpace1`, `SetHDRMetaData`, `IDXGIOutput6::GetDesc1` (color space, luminance from EDR headroom). Layer color space and EDR flags per mode. The PQ and scRGB paths produce expected drawable values offscreen. EDR on screen in suite `present` | M |

### Wave 6

The integrator wires K and N into the exit test, sets the status gate from their call logs, adds the D3D12-only and media interfaces (`IDXGIDecodeSwapChain`, `IDXGIFactoryMedia`, `IDXGISwapChainMedia`, `IDXGIOutputDuplication`) to `tools/api/out_of_scope.tsv` with reasons, and cites the gates in the roadmap.

Critical path: X1 and X2 → interfaces → B and C → E and G → I1 → J and N → O → wave 6. At most four packages run at once across Phases 2 and 3 together ([schedule](phases-2-3.md)). Waves 1, 3 and 4 have one GPU-heavy package each (B, K, N); wave 5 has two (O, P); E and F are light.

Phase 3 runs in parallel and owns the shader compiler paths, none of which this plan touches. The two phases can collide only on `src/backend/include/directmetal/backend/{device,commands}.hpp` and `src/backend/meson.build`; the integrator lands both phases' interface changes there in sequence.

## Exit test

Programs in `tests/api/present/` (suite `present`, developer GUI session):

- **`dxgi_present_pacing`**
  - Setup: windowed 640×360 `FLIP_DISCARD`, two `B8G8R8A8_UNORM` buffers; each frame clears to a frame-indexed color.
  - Phases: 30 warm-up frames, then 240 at sync interval 1, 120 at 2 and 120 at 0. The refresh period comes from the containing output's current mode (`FindClosestMatchingMode` on the desktop size).
  - Pass at interval 1: median Present-to-Present interval within ±5 % of one period, p95 ≤ 1.5 periods, ≤ 2 % of frames over 1.5 periods.
  - Pass at interval 2: median within ±5 % of two periods.
  - Pass at interval 0: median ≤ 0.9 periods.
  - `PresentCount` rises by 1 per Present.
  - A waitable variant (latency 1) waits on the handle with a 1 s timeout that never expires.
  - An occluded window is reported as a setup error asking for a rerun, never as a pass.
- **`dxgi_resize`**: `ResizeBuffers` through 640×360 → 1280×720 → 0×0 after `ResizeTarget` → `R10G10B10A2_UNORM` → a different buffer count. An outstanding back-buffer reference gives `DXGI_ERROR_INVALID_CALL`. After each step: descs, client rect, back-buffer readback before Present, successful presents.
- **`dxgi_fullscreen`**
  - Runs only with `DIRECTMETAL_TEST_FULLSCREEN=1`, which `run_local.sh present` sets. Under Wine it runs inside a 1280×800 virtual desktop, so it covers only that window; natively it covers the screen with the test window.
  - Steps: enter fullscreen; check state, containing output, and window rect equal to the output's desktop coordinates; 60 presents; `ResizeTarget`; `ResizeBuffers` after `WM_SIZE`; leave fullscreen, and style and rect are restored. The output's current mode is identical before and after.
  - Under 5 s in total. A scope guard leaves fullscreen on any failure, and the runner kills the process at its timeout.

Every `HRESULT` and desc must match the Windows reference exactly. It is recorded on WARP by `reference.yml`: pacing records only `HRESULT`s, and timing is checked against bounds. All runs use `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1` and `MTL_DEBUG_LAYER_ERROR_MODE=assert`, and treat any abnormal exit as a failure. In suite `present` the offscreen warning must not appear on stderr.

Headless coverage on every pull request comes from K's programs in suite `api`: natively, and under Wine with the null graphics driver.

Conformance, Wine `dlls/dxgi/tests`:

- **Build.** `tools/conformance/fetch_wine.py` downloads the release tarball for tag `wine-11.21` (sha256 pinned in `tools/conformance/sources.tsv`) into its own cache directory. `build_wine_tests.sh` configures it out of tree with LLVM-MinGW for x86_64 and i386, and builds only `dlls/dxgi/tests/{x86_64,i386}-windows/dxgi_test.exe` and their prerequisites. Sources are unmodified and never vendored ([ADR 0001](../adr/0001-license.md)). Output is cached by tag and toolchain.
- **Run.** `run_wine_tests.py --test dxgi --arch win64|win32 --display virtual-desktop|null` runs `dxgi_test.exe dxgi` with our `dxgi`, `d3d11`, `d3d10_1` and `d3d10core` app-local and `directmetal` builtin. `d3d12`, `d3d12core` and `d3d10` are disabled, so the D3D12 section skips until Phase 7.
  - Test environment: `WINETEST_PLATFORM=windows` (Windows expectations; `todo_wine` is inert), `WINETEST_NO_MT_D3D=1` (one GPU context at a time), Metal validation, through the GPU lock, with a 15-minute timeout.
  - The real display is never used. `virtual-desktop` runs via `explorer /desktop=directmetal,1280x800` only if X1 proved that mode changes stay inside the virtual desktop. Otherwise the tool refuses that configuration and only `null` runs.
- **Count.** Pass rate per architecture = (executed − failures) / executed, from winetest's summary line.
- **Gate.** For each architecture, `virtual-desktop`:
  - pass rate ≥ 90 %
  - every failure listed in `expectations.tsv` (key: source line plus enclosing `test_*` function from the pinned source; architecture; issue number)
  - no listed entry that now passes
  - no crash or timeout
  - skips ≤ the reviewed baseline, which excludes only the D3D12 section
- **Where it runs.** `nightly.yml` runs the `null` configuration on the self-hosted runner and reports its numbers. The gate configuration runs from `run_local.sh conformance-dxgi` in the GUI session.

Status gate: `dxgi_exit_status_gate` requires `dxgi:tested>=45`, `d3d11:tested>=12` and `common:tested>=3` on the exit programs' call logs; the integrator confirms the counts in wave 6.

## Decisions

| Decision | Choice |
| --- | --- |
| HWND → layer under Wine | Unix-side lookup of Wine's `NSWindow` by `hwnd`, then our own layer-hosting view, attached on the main thread with bounded waits ([ADR 0011](../adr/0011-window-surfaces.md), amending ADR 0003 to allow Objective-C++ in `src/unix/surface/`) |
| Native windows | HWND is an `NSView*`; the test harness creates the `NSWindow` and runs AppKit on the main thread |
| Protocol | v4 and stream version 2, append only; `present` commits like `submit` |
| Back buffer → drawable | Blit when bit-compatible, otherwise a hand-written MSL pass compiled at runtime; DXGI scaling via `contentsGravity`; no rendering into drawables in Phase 2 |
| Sync intervals | `displaySyncEnabled` for 0 and 1; `presentDrawable:afterMinimumDuration:` for 2–4 |
| Frame latency | PE waits on the present timeline; the waitable object is a Win32 semaphore released by a per-swap-chain waiter thread |
| Vblank | `CADisplayLink` from `NSScreen` on a backend run-loop thread; synthetic clock offscreen; no `CVDisplayLink` |
| Outputs | Unix display data everywhere; under Wine, `HMONITOR`, names, modes and coordinates from GDI |
| Fullscreen | Emulated borderless fullscreen; no mode changes; gamma applied in the present pass, never to the display |
| Runtime state | Owned by `directmetal.dll` and the native bridge through a C ABI; front ends link `runtime_client` |
| Cross-module objects | COM interfaces only (`IDirectMetalPresentDevice`, `IDirectMetalAdapter`) |
| D3D10.1 | Minimal `d3d10_1.dll` slice in Phase 2, required by Wine's dxgi tests; completed in Phase 5 |
| Test gating | `api` headless (CI); `present` in the developer's GUI session via `run_local.sh`; conformance only on a virtual desktop or the null driver |

## Risks

| Risk | Mitigation |
| --- | --- |
| Wine's AppKit window structure (`hwnd` property, content-view geometry) is internal and may change | X1 records the observations; Wine version logged; offscreen fallback with a warning; suite `present` re-proves it on every Wine upgrade |
| Main-thread dispatch deadlocks against winemac's main thread | Asynchronous dispatch with a 2 s bound; failure means offscreen and a retry at the next Present; stream threads never wait on the main thread |
| `nextDrawable` stalls the stream thread when occluded | Occlusion is tracked from `NSWindow.occlusionState`; occluded frames retire without a drawable; Present returns `DXGI_STATUS_OCCLUDED` |
| Presented handler or pacing behaves differently under Rosetta, or ProMotion varies the refresh rate | X2 measures both; pacing bounds come from the reported refresh; per-display tolerance only ever tightened |
| No GUI session on the CI runner | Offscreen surfaces and the null Wine driver give headless coverage; `present` gated in `run_local.sh` and fails when no GUI session is present |
| Wine's dxgi tests change display modes, clip the cursor, set gamma | Virtual desktop or null driver only, proven by X1 before first use; DirectMetal never changes modes or gamma; bounded runs |
| 90 % depends on a D3D10.1 slice and on emulated fullscreen, where Wine's tests expect exclusive fullscreen and mode changes | Slice sized from the test's calls; mismatches tracked as issues; triage wave budgeted |
| Single backing texture for D3D10/11 flip models changes `FLIP_SEQUENTIAL` contents after Present and `GetCurrentBackBufferIndex` | Documented deviation, tracked from conformance results; buffer rotation in Phase 4 if titles depend on it |
| Hosted Windows runner cannot record fullscreen or windowed references | Fallback to assertion-only checks for fullscreen; recorded in the reference metadata |
| Frame-statistics QPC differs from the host clock under Wine | Offset sampled at swap-chain creation; tests check only that values are monotonic and consistent |
| `commands.hpp` and the executor near the 300-line limit | Layout asserts and the present handler moved into their own files with the interfaces |
| Phase 3 changes the same backend headers and build files | Interface changes to those files land in sequence through the integrator |

## Deferred decisions

- Zero-copy presentation (rendering into drawables or IOSurface back buffers): Phase 8.
- Real exclusive fullscreen and display mode changes behind configuration: not planned; revisit only for a title that needs it.
- A GitHub runner in the developer's GUI session (a LaunchAgent under that account) instead of `run_local.sh`: needs a security review first.
- Native SDK model for HWND (`NSView*`) and waitable `HANDLE`s: Phase 9.
- D3D12 swap chains through `IDirectMetalPresentDevice` on the command queue, and the D3D12 section of Wine's dxgi tests: Phase 7.
- Output duplication, `GetDisplaySurfaceData`, stereo, child HWNDs drawn over D3D content.
- Call logs remain per module.
