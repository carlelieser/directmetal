# API inventories

Machine-generated lists of the public API surface DirectMetal implements (Direct3D 9–12, DXGI) and the Metal API it targets. They are the coverage checklist for the [roadmap](../ROADMAP.md): every DirectX interface method listed here must eventually be implemented, stubbed with a documented reason, or declared out of scope.

| File | Contents |
| --- | --- |
| [SUMMARY.md](SUMMARY.md) | Counts per API group |
| `<group>.md` | Exported functions, interfaces with their own methods, enums, structs |
| `<group>.json` | Same data, including enum members and Metal availability, for tooling |

Per-method implementation status and test coverage are generated separately; see [Implementation status](#implementation-status).

Groups: `d3d9`, `d3d10`, `d3d11`, `d3d12`, `dxgi`, `common` (shared `D3D_*` types), `metal` (Metal, MetalFX, MetalKit, QuartzCore, IOSurface).

## Conventions

- DirectX interface member lists contain only methods the interface declares itself; inherited methods, including `IUnknown`, appear on the base interface.
- Metal members are selectors (`newBufferWithLength:options:`) and property names. `macOS x.y` is the member's own `API_AVAILABLE` annotation; members without one inherit their container's availability.
- Parsing is lexical, not a compiler front end. Counts are accurate enough for planning; verify individual entries against the header before relying on them.

## Regenerating

```bash
tools/api/fetch_headers.sh
python3 -I tools/api/generate_inventory.py
```

`fetch_headers.sh` downloads the DirectX headers at the revisions pinned in [`tools/api/sources.tsv`](../../tools/api/sources.tsv) and copies Metal headers from the active Xcode SDK (`xcrun --show-sdk-path`). Headers land in `third_party/api-headers/`, which is git-ignored.

## Implementation status

`tools/api/report_status.py` reports, for every DirectX method and exported function in these inventories, whether the front ends implement it and whether any test calls it. Status is derived from markers in the front-end sources, from call logs written by test runs, and from a reviewed list of out-of-scope entries; it is never edited by hand. Metal is excluded: it is the target, not an API DirectMetal implements.

### API keys

Every entry is identified by a key:

- Interface methods: `Interface::Method`, using the interface that *declares* the method in the inventory. `ID3D11Device1::CreateBuffer` is rejected with a hint to use `ID3D11Device::CreateBuffer`.
- `IUnknown` methods: `IUnknown::QueryInterface`, `IUnknown::AddRef`, `IUnknown::Release`. These keys are project-wide: the COM base implements them once for every object in every API group, and they are reported under `common`.
- Exported functions: the bare name, e.g. `D3D11CreateDevice`.

A key that names no inventory entry is an error, reported with its file and line.

D3D10 is implemented on the D3D11 front end. Each D3D10 method carries its own marker with its D3D10 key; when it forwards to a D3D11 method, that method's marker also records its D3D11 key. A D3D10 test therefore also marks the D3D11 keys it reaches as tested.

### Source markers

Every front-end function that implements an API entry starts with exactly one marker:

| Marker | Use | Effect |
| --- | --- | --- |
| `DM_IMPL("key")` | Implemented entry | Records the call (see [recording](#recording)), then continues. |
| `DM_STUB("key")` | Unimplemented entry returning `HRESULT` | Records the call, logs a warning naming `key`, and executes `return E_NOTIMPL;`. |
| `DM_STUB_RETURN("key", value)` | Unimplemented entry with another return type | As `DM_STUB`, but executes `return value;`. |
| `DM_STUB_VOID("key")` | Unimplemented entry returning `void` | As `DM_STUB`, but executes `return;`. |

The scanner enforces these rules; a violation is an error naming the file and line:

- The marker is the first statement of the function body: only whitespace may separate it from the body's opening `{`. Markers in helpers, after other statements, at file scope or inside raw strings are rejected.
- The first argument is a single string literal written in the call itself: no concatenation, no macro, no variable. Anything else is invisible to the scanner.
- Markers inside `//` and `/* */` comments are ignored, so commenting a marker out removes it from the report. Comment openers inside string and character literals are not comments.

Keys name the declaring interface, so a method implemented once in a shared base class or template needs one marker, not one per derived interface.

Scanned files: `src/frontends/**` with suffix `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, `.inl`, `.ipp`, `.m` or `.mm`. Other files, such as `meson.build` and `.def` files, are skipped. The directory must exist.

### Recording

The COM base provides the macros from Phase 1. Each expansion behaves as follows:

- Each call site owns a `static std::atomic<std::uint8_t>`, initially zero. Every call performs one `memory_order_relaxed` load of it; when it is nonzero, the marker does nothing further (`DM_IMPL` continues, stubs return).
- When it is zero, the calling thread attempts `compare_exchange_strong` from 0 to 1. Only the thread that wins records the call and, for stubs, logs the warning. Recording therefore happens once per call site per process, from exactly one thread, with no lock on the call path.
- `DIRECTMETAL_CALL_LOG_DIR` is read once per process, on the first recording, and cached. When it is unset, recording writes nothing, so after its first call a marker costs one relaxed atomic load.

### Call logs

When `DIRECTMETAL_CALL_LOG_DIR` is set, recording appends the key to the process's call log:

- One file per process, named `<executable>-<process id>-<random hex>.log`, where the suffix is 16 lowercase hex digits from the operating system's random source. Process IDs alone collide: Wine's are small and reused. The directory must already exist; the test harness creates it.
- The file is opened in append mode on the first recording and kept open. Each line is written with a single write call, unbuffered, so lines completed before a crash survive.
- UTF-8 text, one key per line, each terminated by `\n`.
- The variable holds a directory path as seen by the process that writes the log; the front ends do no conversion. Under Wine the test harness passes PE test processes a Windows path, e.g. `Z:\tmp\call-logs` from `winepath -w /tmp/call-logs`, and passes the report the unix path.

The report reads every `*.log` file under each call-log directory, recursively. Leading and trailing whitespace and blank lines are ignored, and duplicate keys within and across files are expected. A final line without a terminating `\n` was cut off mid-write and is ignored.

### Out-of-scope entries

[`tools/api/out_of_scope.tsv`](../../tools/api/out_of_scope.tsv) lists entries DirectMetal will not implement, reviewed like code. Each row is a key or an interface name, a tab, and the reason; an interface name excludes every method it declares. Lines starting with `#` and blank lines are ignored. A row naming an unknown key or interface, or without a reason, is an error.

Excluded entries are reported as `excluded` with their reason. They may carry stub markers, since callers still need `E_NOTIMPL`; an excluded entry with `DM_IMPL` is an error, because the exclusion is stale.

| Status | Rule |
| --- | --- |
| `excluded` | Listed in the out-of-scope file. |
| `implemented` | At least one `DM_IMPL` marker and no stub marker for the key. |
| `stub` | Any `DM_STUB*` marker for the key, even if another definition carries `DM_IMPL`: some caller still gets the stub. |
| `missing` | No marker for the key. |

`tested` is independent of the status: an in-scope key is tested when any call log contains it, so a stub that a test calls is both `stub` and tested. Counts and shares of `implemented`, `stub`, `missing` and `tested` cover in-scope entries only.

### Running

```bash
python3 -I tools/api/report_status.py --call-log-dir out/call-logs
```

| Option | Default |
| --- | --- |
| `--inventory DIR` | `docs/api` |
| `--sources DIR` | `src/frontends` |
| `--call-log-dir DIR` (repeatable) | `$DIRECTMETAL_CALL_LOG_DIR` if set, else none |
| `--out-of-scope FILE` | `tools/api/out_of_scope.tsv` |
| `--output DIR` | `out/api-status` |
| `--require GROUP:METRIC>=N` (repeatable) | none |

Output, not committed because it depends on test runs: `<group>.md` with counts and per-interface tables, `SUMMARY.md` with counts per group, and `status.json` with `groups.<group>.counts` (each status, `tested`, `in_scope`, `total`) and `groups.<group>.methods.<key>` (`state`, `is_tested`, `location`, `exclusion_reason`).

`--require` takes `GROUP:METRIC`, then `>=` or `<=`, then a whole-number count or a percentage of the group's in-scope entries with a trailing `%`, at most `100%`. Metrics are `implemented`, `stub`, `missing` and `tested`. A percentage on a group with no in-scope entries is an error. Phase exits cite these gates, for example:

```bash
python3 -I tools/api/report_status.py --require d3d11:stub>=1 --require d3d11:tested>=1
python3 -I tools/api/report_status.py --require d3d11:missing<=0 --require d3d11:tested>=90%
```

Exit status: `0` when every requirement holds, `1` when any is unmet (each is printed with its actual value), `2` when an input is missing or invalid: inventory, sources, call logs, exclusions, requirements or the output directory.

Tests: `python3 -I -m unittest discover -s tools/api/tests`.

## Sources and licensing

| API | Source | License |
| --- | --- | --- |
| Direct3D 12, DXGI formats, `d3dcommon.h` | [microsoft/DirectX-Headers](https://github.com/microsoft/DirectX-Headers) | MIT |
| Direct3D 9, 10, 11, DXGI | [mingw-w64](https://github.com/mingw-w64/mingw-w64) `mingw-w64-headers/include` | Per-file; many are derived from Wine (LGPL-2.1+) |
| Metal and related frameworks | macOS SDK shipped with Xcode | Xcode and Apple SDKs Agreement |

Third-party headers are not committed. The inventories record only API names and counts.
