# API inventories

Machine-generated lists of the public API surface DirectMetal implements (Direct3D 9–12, DXGI) and the Metal API it targets. They are the coverage checklist for the [roadmap](../ROADMAP.md): every DirectX interface method listed here must eventually be implemented, stubbed with a documented reason, or declared out of scope.

| File | Contents |
| --- | --- |
| [SUMMARY.md](SUMMARY.md) | Counts per API group |
| `<group>.md` | Exported functions, interfaces with their own methods, enums, structs |
| `<group>.json` | Same data, including enum members and Metal availability, for tooling |

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

## Sources and licensing

| API | Source | License |
| --- | --- | --- |
| Direct3D 12, DXGI formats, `d3dcommon.h` | [microsoft/DirectX-Headers](https://github.com/microsoft/DirectX-Headers) | MIT |
| Direct3D 9, 10, 11, DXGI | [mingw-w64](https://github.com/mingw-w64/mingw-w64) `mingw-w64-headers/include` | Per-file; many are derived from Wine (LGPL-2.1+) |
| Metal and related frameworks | macOS SDK shipped with Xcode | Xcode and Apple SDKs Agreement |

Third-party headers are not committed. The inventories record only API names and counts.
