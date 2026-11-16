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

`tools/api/report_status.py` reports, for every DirectX method and exported function in these inventories, whether the front ends implement it and whether any test calls it. Status is derived from markers in the front-end sources and from call logs written by test runs; it is never edited by hand. Metal is excluded: it is the target, not an API DirectMetal implements.

### API keys

Every entry is identified by a key:

- Interface methods: `Interface::Method`, using the interface that *declares* the method in the inventory. `ID3D11Device1::CreateBuffer` is rejected with a hint to use `ID3D11Device::CreateBuffer`.
- `IUnknown` methods: `IUnknown::QueryInterface`, `IUnknown::AddRef`, `IUnknown::Release` (reported under `common`).
- Exported functions: the bare name, e.g. `D3D11CreateDevice`.

A key that names no inventory entry is an error, reported with its file and line.

### Source markers

Every front-end function that implements an API entry starts with exactly one marker:

| Marker | Use | Effect |
| --- | --- | --- |
| `DM_IMPL("key")` | Implemented entry | Records the call (see [call logs](#call-logs)), then continues. |
| `DM_STUB("key")` | Unimplemented entry returning `HRESULT` | Records the call, logs a warning naming `key` once per process, and executes `return E_NOTIMPL;`. |
| `DM_STUB_RETURN("key", value)` | Unimplemented entry with another return type | As `DM_STUB`, but executes `return value;`. |
| `DM_STUB_VOID("key")` | Unimplemented entry returning `void` | As `DM_STUB`, but executes `return;`. |

Rules for the macros, which the COM base provides from Phase 1:

- The first argument is a single string literal, written in the call itself: no concatenation, no macro, no variable. The scanner matches `DM_IMPL(`, `DM_STUB(`, `DM_STUB_RETURN(` or `DM_STUB_VOID(` followed by `"key"`; anything else is invisible to it.
- Each macro expands to one statement (`do { ... } while (0)` for `DM_IMPL`) and is the first statement of the function body.
- Keys name the declaring interface, so a method implemented once in a shared base class or template needs one marker, not one per derived interface.
- Markers inside `//` and `/* */` comments are ignored, so commenting a marker out removes it from the report.

Scanned files (the directory must exist): `src/frontends/**` with suffix `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hpp`, `.inl` or `.mm`.

| Status | Rule |
| --- | --- |
| `implemented` | At least one `DM_IMPL` marker and no stub marker for the key. |
| `stub` | Any `DM_STUB*` marker for the key, even if another definition carries `DM_IMPL`: some caller still gets the stub. |
| `missing` | No marker for the key. |

`tested` is independent of the status: a key is tested when any call log contains it, so a stub that a test calls is both `stub` and tested.

### Call logs

When the environment variable `DIRECTMETAL_CALL_LOG_DIR` is set, every marker records its key to a call log in that directory:

- One file per process, named `<executable>-<process id>.log`. The directory must already exist; the test harness creates it.
- UTF-8 text, one key per line, terminated by `\n`. Each key is written at most once per process, and the line is flushed when written so a crashing test still counts.
- Under Wine the front ends read the variable on the PE side; an absolute unix path is reachable through drive `Z:`.

The report reads every `*.log` file under each call-log directory, recursively. Leading and trailing whitespace and blank lines are ignored; duplicate keys across files are expected.

### Running

```bash
python3 -I tools/api/report_status.py --call-log-dir out/call-logs
```

| Option | Default |
| --- | --- |
| `--inventory DIR` | `docs/api` |
| `--sources DIR` | `src/frontends` |
| `--call-log-dir DIR` (repeatable) | `$DIRECTMETAL_CALL_LOG_DIR` if set, else none |
| `--output DIR` | `out/api-status` |
| `--require GROUP:METRIC>=N` (repeatable) | none |

Output, not committed because it depends on test runs: `<group>.md` with counts and per-interface tables, `SUMMARY.md` with counts per group, and `status.json` with `groups.<group>.counts` and `groups.<group>.methods.<key>` (`state`, `is_tested`, `location`).

`--require` takes `GROUP:METRIC` followed by `>=` or `<=` and a count, or a percentage of the group's entries with a trailing `%`. Metrics are `implemented`, `stub`, `missing` and `tested`. Phase exits cite these gates, for example:

```bash
python3 -I tools/api/report_status.py --require d3d11:stub>=1 --require d3d11:tested>=1
python3 -I tools/api/report_status.py --require d3d11:missing<=0 --require d3d11:tested>=90%
```

Exit status: `0` when every requirement holds, `1` when any is unmet (each is printed with its actual value), `2` when an input is missing or invalid.

Tests: `python3 -m unittest discover -s tools/api/tests`.

## Sources and licensing

| API | Source | License |
| --- | --- | --- |
| Direct3D 12, DXGI formats, `d3dcommon.h` | [microsoft/DirectX-Headers](https://github.com/microsoft/DirectX-Headers) | MIT |
| Direct3D 9, 10, 11, DXGI | [mingw-w64](https://github.com/mingw-w64/mingw-w64) `mingw-w64-headers/include` | Per-file; many are derived from Wine (LGPL-2.1+) |
| Metal and related frameworks | macOS SDK shipped with Xcode | Xcode and Apple SDKs Agreement |

Third-party headers are not committed. The inventories record only API names and counts.
