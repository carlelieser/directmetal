# 0001 — License

Status: accepted

## Context

DirectMetal should be easy to ship inside Wine-based runtimes, commercial ports and other open-source projects. Related projects use a mix of permissive (DXVK: zlib; dxil-spirv: MIT; SPIRV-Cross, MoltenVK, metal-cpp: Apache-2.0; DirectXShaderCompiler: NCSA) and copyleft licenses (Wine, vkd3d, vkd3d-proton, DXMT: LGPL-2.1-or-later).

## Decision

DirectMetal is licensed under MIT.

- Code under MIT, zlib, BSD, NCSA or Apache-2.0 may be incorporated or linked, keeping its notices in `third_party/`.
- Code under LGPL or GPL, including Wine, vkd3d, vkd3d-proton and DXMT, may be studied for behavior but not copied or translated into DirectMetal sources.
- Running Wine's and vkd3d-proton's test suites as separate, unmodified programs is permitted; their sources are not vendored.

## Consequences

- Downstream projects can redistribute DirectMetal with minimal obligations.
- Behavior that only copyleft projects implement must be reimplemented from documentation, tests and observation.
