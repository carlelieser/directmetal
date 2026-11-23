# Architecture decision records

Each record states one decision, its context and its consequences. A decision changes only through a new record that supersedes the old one.

| ADR | Decision |
| --- | --- |
| [0001](0001-license.md) | MIT license; permissive-only code reuse |
| [0002](0002-delivery-model.md) | Wine-integrated DLLs first, native macOS SDK in Phase 9 |
| [0003](0003-language.md) | C++20; Objective-C++ confined to the Metal backend |
| [0004](0004-build-system.md) | Meson with a PE cross file |
| [0005](0005-platform-targets.md) | Apple silicon; macOS 14 for D3D9–11, macOS 26 for D3D12 |
| [0006](0006-shader-pipeline.md) | Native front ends into DirectMetal IR; MSL then AIR |
| [0007](0007-testing-strategy.md) | Windows output as reference; tiered test suites and CI gates |
| [0008](0008-wine-bridge-module.md) | One builtin bridge DLL and one unix library shared by all API DLLs |
| [0009](0009-front-end-placement.md) | Front ends on the PE side; control calls plus a batched command stream |
| [0010](0010-native-windows-headers.md) | Pinned mingw-w64 headers at build time for native builds; win32metadata for the SDK |
