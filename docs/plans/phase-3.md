# Phase 3 plan — shader compiler infrastructure

Scope and exit criteria are in the [roadmap](../ROADMAP.md#phase-3--shader-compiler-infrastructure). This plan fixes where the compiler runs, the IR, the binding model, the cache, the bridge and backend additions, and the shader test pipeline. It then splits the work into packages and defines the exit test. Phase 2 is planned in parallel; [Coordination with Phase 2](#coordination-with-phase-2) lists the files both phases change.

## Architecture

The shader compiler is a plain C++20 library, `src/shader/`, with no Apple, Wine or Windows dependencies ([ADR 0003](../adr/0003-language.md)). It builds for every target (win32, win64, macOS x86_64 and arm64), so any side can link any part of it. The two sides use it differently:

- **PE side (front ends, Phase 4 on):** only the DXBC container, signature and reflection reader. It is used to validate bytecode in `CreateXShader`, to match an input layout against the VS input signature in `CreateInputLayout`, and to supply reflection data for D3D10 effects in Phase 5. It never translates.
- **Unix side (host):** the full pipeline. DXBC is lifted to DirectMetal IR, run through passes, specialized per pipeline state, emitted as MSL, compiled to an `MTLLibrary` and pipeline states by the backend, and cached on disk.

```
 PE side                                   unix side (directmetal.so / native bridge)
 front end                                 host
  CreateXShader ── dxbc::Container ──┐      create_shader:  copy bytecode, SHA-256,
  (validate, signatures)             │        shader::prepare (lift + generic passes)
                                     │      create_*_pipeline: derive VariantKeys
  bytecode address+size ─────────────┴──►     translation cache ─miss─► shader::emit ─► MSL
                                              backend::create_library (MTLLibrary)
                                              backend::create_*_pipeline (+ binary archive)
```

Translation runs on the unix side for these reasons:

- Variants depend on pipeline state that only the host sees together (RT formats, input layout formats, the upstream stage's outputs).
- The cache key is the original bytecode, and the cache belongs to the side that does the file I/O and owns `MTLBinaryArchive`.
- MSL text never crosses the bridge.
- No serialized IR becomes a second wire format and fuzzing surface.
- The 32-bit address space of WoW64 processes holds no compiler state.

The cost is that the unix side parses untrusted bytecode. Both sides are in the same process, and the reader is fuzzed.

`src/shader_cache/` is unix-only: it hashes with CommonCrypto and handles files and archive paths. The host owns one cache per process.

## Interfaces

These land before any package starts. Packages implement or consume them; changing one needs integrator review. S5 and S6 land in one joint protocol-v4 PR with Phase 2's additions (see Coordination).

| Interface | Headers | Consumers |
| --- | --- | --- |
| S1 IR | `src/shader/ir/include/directmetal/shader/ir/{types,ops,module,builder,verify,text}.hpp`; spec in `docs/shaders/ir.md` | Every compiler package |
| S2 DXBC reader | `src/shader/dxbc/include/directmetal/shader/dxbc/{container,signature,reflection,program,lift}.hpp` | Lifter, host, PE front ends (Phase 4), fuzzers |
| S3 Passes | `src/shader/passes/include/directmetal/shader/passes/{pass,pipeline}.hpp` | Passes, layout, specialization, driver |
| S4 Compiler driver | `src/shader/include/directmetal/shader/{compiler,variant_key,binding_layout,stage_interface,diagnostic}.hpp` | Host, shader runner, cache, `dm-shaderc` |
| S5 Backend pipelines | `src/backend/include/directmetal/backend/{pipeline,encoding}.hpp`; new virtuals on `Device` and `CommandRecorder`, wired to stubs that return `Status::unsupported` | Backend lane G, shader runner, host |
| S6 Bridge protocol v4 | `protocol.hpp` (`protocol_version = 4`, appended `FunctionCode`s 26–28 after Phase 2's 16–25); new `src/bridge/include/directmetal/bridge/shaders.hpp` (params); `src/unix/host/calls.hpp` entries answering `status_not_implemented` | Host, bridge client, Phase 4 front end |
| S7 Shader cache | `src/shader_cache/include/directmetal/shader_cache/{translation_cache,pipeline_archive}.hpp` | Host |
| S8 Shader test format | `docs/shaders/test-format.md`; `tests/shaders/spec/include/directmetal/shader_test/spec.hpp` | Windows runner, macOS runner, tooling |

Rules that span interfaces:

- **Placement.** Only `src/shader/dxbc/{container,reflection}` is linked into PE modules. `tests/compile/interfaces_pe.cpp` includes its headers in win32 and win64 builds. Every other part of `src/shader/` must still build there; nothing in it includes Apple or Windows headers.
- **Bytecode crossing.** `create_shader` passes bytecode as a 64-bit address and size in PE memory (at most `max_bytecode_size` = 4 MiB). The host copies it during the call, after `is_addressable` checks (below 4 GiB under WoW64). The PE side may free it once the call returns.
- **Destruction.** Shader and pipeline handles are device objects destroyed with `destroy_object`. The host holds them until the submit that last references them, as it does for buffers.
- **Compilation is synchronous.** `create_*_pipeline` returns only after the MSL is compiled and the pipeline exists. Asynchronous compilation is Phase 8.
- **Failure.** A shader that uses a feature DirectMetal does not translate fails `create_shader` with `status_not_supported` and logs the opcode and token offset. The Phase 4 front end chooses the `HRESULT`. Invalid bytecode fails with `status_invalid_parameter`.
- **Diagnostics.** Every reader and lifter error carries a `Diagnostic{code, token_offset, message}`. Messages name the chunk, opcode or operand and the offset.
- **Dependencies.** `src/shader` depends on `formats` (component types of DXGI formats) and on `backend_types_dep` (plain types only). It never depends on the backend library. The backend never depends on `src/shader`: the host and the runner convert `shader::BindingLayout` into backend `ArgumentValue`s.
- **Determinism.** The same bytecode and `VariantKey` produce byte-identical IR text and MSL. Golden tests and the cache rely on this.
- **Math.** All MSL is compiled as MSL 3.1 (macOS 14) with fast math disabled: `fastMathEnabled = NO`, or `mathMode = MTLMathModeSafe` where available. `preserveInvariance` is set when a shader declares `precise` or invariant position.
- **Large tables.** The opcode, operand and op-metadata tables are `constexpr` arrays split by family into separate files (arithmetic, integer and bit, conversion, flow, resource, atomic, declaration, hull and geometry). Dispatch goes through tables of handler functions, as `CommandExecutor` does. No `switch` spans files, and no `utils`/`helpers`/`common` modules exist.
- **Licensing.** Wine's `d3dcompiler`, vkd3d-shader, DXMT's `airconv` and vkd3d-proton are not consulted ([ADR 0001](../adr/0001-license.md)). Sources are the D3D11.3 functional spec, `d3d12TokenizedProgramFormat.hpp` from the pinned DirectX-Headers wrap (MIT), Apple's MSL specification, and Windows outputs.

## DirectMetal IR

### Values and types

- **Interned types** (`TypeId`, a 16-bit index into the module's type table):
  - `bool`, `i16`, `i32`, `i64`, `f16`, `f32`, `f64`;
  - `vec<scalar, 2..4>`;
  - `array<T, N>`, for variables only;
  - opaque `handle<kind>`, one per resource class and shape.
- **Integers are signless.** The op carries signedness (`div.s`/`div.u`, `shr.s`/`shr.u`, `cvt.s2f`).
- **Dense SSA values.** A `ValueId` is a 32-bit index into a per-function arena. Each value is defined by exactly one instruction or block parameter.
- **Data-oriented storage.** Instructions are records (`op`, result `TypeId`, operand range in a shared pool, flags `precise`/`saturate`/`nonuniform`, immediate payload). There is no class hierarchy per op.
- **Constants** are `constant` instructions in the entry block with their bits in the operand pool. `-0.0` and NaN payloads are preserved.
- **Scalar arithmetic.** The DXBC lifter scalarizes register components: `r0.y` becomes its own value. Apple GPUs are scalar SIMT, so this costs nothing at run time and simplifies typing. Vectors remain where interfaces need them: stage I/O, sample results, typed loads. `extract` and `construct` convert between the two.
- **Explicit bitcasts.** DXBC registers are untyped 32-bit, so the lifter reads them as `i32` and inserts `bitcast` where an op needs `f32`. The `simplify` pass folds bitcast chains.
- **Memory is named variables, not pointers.**
  - Function-local registers and indexable temps (`x#`) are `local` variables.
  - Group-shared memory is a `threadgroup` variable.
  - Immediate constant buffers are `constant` variables.
  - Access is `load_var`/`store_var` with an index.
  - DXIL allocas and GEPs (Phase 7) map onto the same ops.
- **Handles.** `handle %h = resource_handle %decl, %index` yields a handle. Every resource op takes a handle operand. Static D3D11 slots, SM5.1 register ranges and SM6.6 heap indices (Phase 7) all use this one form. Handles may not be stored or passed through block parameters (the verifier enforces this). This rule is relaxed only if DXIL needs it.

### Control flow

- **Structured CFG.** Functions are a CFG of basic blocks with block parameters (no phi instructions). Each construct header names its merge block, and each loop names its continue block:
  - `branch_if %c, ^then, ^else merge ^m`
  - `switch %v, default ^d, [k: ^b...] merge ^m`
  - `loop ^body merge ^exit continue ^cont`
  - plus `jump`, `ret` and `unreachable`.
- **The verifier enforces SPIR-V-style structured rules.** Constructs nest properly and have a single entry. A break targets only the innermost loop or switch merge. A continue targets only the innermost loop's continue block. Every construct dominates its merge.
- **Why a structured CFG rather than a region tree.** DXBC's `if`/`loop`/`switch`/`breakc`/`continuec`/`retc` lift directly. DXIL's unstructured LLVM CFG (Phase 7) can be held as-is and fixed by a structurizer pass, and the IR form records whether it is structured. The MSL emitter rebuilds the construct tree from the annotations. AIR (Phase 8) needs only the CFG.
- **Discard.** `discard` is not a terminator; it has demote semantics, so derivatives keep working, matching D3D's `discard_nz`. Corpus tests pin down how it maps to `discard_fragment()`.

### Resources, bindings and signatures

- **Resource declarations.** A `ResourceDecl` has:
  - an id;
  - a class (`cbv`, `srv`, `uav`, `sampler`);
  - a shape (buffer, raw, structured with stride, typed buffer, `1d`/`2d`/`3d`/`cube`, `[array]`, `2dms`);
  - a return or element type (`f32`/`i32`/`u32`/`unorm`/`snorm` and component count);
  - a register space, lower bound and range size (1 in SM4–5.0);
  - flags (`globally_coherent`, `has_counter`, comparison or mono sampler mode).
- **Signatures.** An `IoElement` has a semantic name and index, register, component mask, system value (`SV_Position`, `SV_VertexID`, `SV_InstanceID`, `SV_Target`, `SV_Depth*`, `SV_Coverage`, `SV_IsFrontFace`, `SV_SampleIndex`, `SV_ClipDistance`, …), component type, min precision, interpolation mode and stream. Modules carry input, output and patch-constant lists.
- **Stage I/O ops.** `load_input`, `store_output` and `load_system_value` are explicit, with an optional vertex or control-point index. Phase 4's GS, HS/DS and stream-output emulation retarget them to buffer memory in an IR pass, without changing the core.
- **Stages and functions.** Modules hold several functions and one entry point. The stage enum already includes geometry, hull and domain, plus the ops `emit_vertex`, `cut_primitive` and the HS phase entry markers, so Phase 4 adds lowering only. Phase 3 decodes GS/HS/DS bytecode fully but rejects lifting it with `unsupported_stage`.
- **Numeric types.** `min16float`/`min16int` lift to 32-bit types (allowed by the min-precision rules). Doubles are decoded but lifting rejects them: Apple GPUs have no fp64, and DirectMetal will report `D3D11_FEATURE_DOUBLES` unsupported.

### Forms and passes

A module records its **form**, and the verifier checks each form's rules:

| Form | Meaning |
| --- | --- |
| `registers` | Lifted; temp registers are `local` variables |
| `ssa` | Temp registers promoted to SSA |
| `bound` | Bindings assigned |
| `emittable` | All D3D lowering done |

A pass is a `PassInfo{name, required form, produced form, run}` entry in a `constexpr` registry. A pipeline is a list of pass names. The pass manager verifies after every pass in debug builds and in all tests (`verify_each`), and records per-pass statistics.

The Phase 3 pipeline, with the package that owns each stage:

- **Prepare** (once per shader, at `create_shader`):
  - `lift_dxbc` (E)
  - `promote_registers` (F): SSA construction with block parameters, after Braun et al. 2013
  - `simplify` (F): constant folding, copy propagation, bitcast folding
  - `remove_dead_code` (F)
  - `lower_d3d_arithmetic` (F)
  - `guard_resource_access` (F)
- **Emit** (per variant):
  - `specialize_inputs`, `specialize_outputs`, `link_upstream`, `add_point_size`, `specialize_sample_rate` (I)
  - `assign_bindings` (I)
  - `guard_loops` (F, test option)
  - `emit_msl` (J)

Notes on individual passes:

- **`lower_d3d_arithmetic`** makes D3D semantics explicit where MSL leaves them undefined:
  - unsigned divide or remainder by 0 yields `0xFFFFFFFF`;
  - shift amounts are masked to 5 bits;
  - float-to-int conversions saturate, and NaN converts to 0;
  - `bfi`/`ubfe`/`ibfe` edge cases follow the spec.

  Doing this in IR means the AIR backend inherits it.
- **`guard_resource_access`** gives D3D's out-of-bounds behavior: reads return 0 and writes are dropped. It covers constant, typed, raw and structured buffers, texture `ld`, indexable temps and group-shared memory. Without it, Metal shader validation would flag the access.
- **`guard_loops`** adds an iteration counter to every loop. After 65,536 iterations the loop breaks and sets a flag in a guard buffer. The shader runner always enables it ([GPU safety](#gpu-safety)).

### Text format

Printer and parser are public (S1); golden tests use them in both directions.

```
directmetal.ir 1
module stage=pixel form=ssa
resource %t0 = srv texture2d<f32x4> space=0 register=0
resource %s0 = sampler default space=0 register=0
input %i1 = TEXCOORD0 register=1 mask=xy f32 linear
output %o0 = SV_Target0 register=0 mask=xyzw f32

func @main() {
^entry:
  %0 = constant i32 0
  %1 = load_input %i1 : vec<f32,2>
  %h = resource_handle %t0, %0 : handle<srv.texture2d>
  %s = resource_handle %s0, %0 : handle<sampler>
  %2 = sample %h, %s, %1 : vec<f32,4>
  store_output %o0, %2
  ret
}
```

Value names are printed in definition order and blocks in reverse post-order, so the output is deterministic. Unknown ops, form violations and type mismatches are parse errors with line and column.

## Binding layout

D3D11 gives each stage 14 constant buffers, 128 SRVs, 16 samplers and 8 UAVs (64 from 11.1). Metal gives each stage 31 buffer slots. Resources therefore go through **argument buffers from the start**, using Metal 3 resource IDs: `gpuAddress` and `gpuResourceID`, macOS 13 and later, Apple7 tier 2.

| Metal buffer index | Contents |
| --- | --- |
| 0–28 | Vertex buffers, vertex stage only: the input layout's used IA slots are mapped densely (a layout using more than 29 slots fails pipeline creation with `status_not_supported` and is logged; vertex pulling is the Phase 4 fallback) |
| 29 | Driver constants via `setBytes` (≤ 4 KiB): loop-guard buffer address, sample count, later D3D9 alpha reference and clip planes |
| 30 | The stage's argument buffer |

- **Compact per-shader layout.** `assign_bindings` lists only the slots the shader declares, in the order CBs, SRVs, UAVs (each followed by its counter if any), samplers. Each entry is 8 bytes: a buffer `gpuAddress` or an `MTLResourceID`.
- **Size table.** A trailing `u32` table holds the bound byte size or element count of every buffer entry, which `guard_resource_access` reads.
- **`BindingLayout`** (S4) records, per entry: D3D class, register, space, offset, Metal kind, and size-table index. The MSL emitter declares a matching plain struct with no `[[id]]`. Package G's tests prove the backend's raw writes match the compiler's layout.
- **Who writes it.** The host writes the argument buffer from bound state, through `CommandRecorder::set_arguments(stage, ArgumentBlock)` (Phase 4 wiring; the Phase 3 runner does the same). The backend copies the block into its upload ring, writes addresses and IDs, binds index 30, and calls `useResource` for each resource. `useHeap` batching is a Phase 4 optimization.
- **Unbound slots.** Buffers get address 0 and size 0, so the guard returns 0. Textures get a backend-owned zero texture per type and component type (`Device::null_texture`).
- **Metal mapping.**
  - CBs are `constant uint4*`.
  - Raw and structured buffers are `device uint*`.
  - Typed buffers are `texture_buffer<T, access>`.
  - Typed UAV textures are `texture*<T, access::read_write>`.
  - Comparison samplers use samplers created with `compareFunction` and `supportArgumentBuffers`.
- **Later policies.** The layout generator takes a `BindingModel`. Phase 3 implements `d3d11_compact`; Phase 7 adds a root-signature-driven policy over the same `BindingLayout` type.

## Specialization

A `VariantKey` (S4) is a fixed-layout struct of fixed-width fields, hashed by its bytes. It holds per-stage state that changes the emitted interface:

- **Common:** option bits (`guard_loops`).
- **VS:** component type and count of each input register's IA format (Metal `stage_in` types must match the attribute format); whether to write `[[point_size]]` (point topology); clip-distance count.
- **PS:**
  - per render target (8): component type and count, or unbound;
  - sample count and whether to run per sample;
  - dual-source blending;
  - whether a depth attachment exists;
  - the upstream interface: the VS output elements (register, mask, component type), so fragment inputs are declared exactly as the vertex function writes them;
  - a reserved D3D9 alpha-test field (Phase 6).
- **CS:** none beyond common.

Notes:

- Specialization is done by IR passes, not Metal function constants: one mechanism, and interface changes need IR changes anyway. Function constants are reconsidered in Phase 8.
- Vertex buffer strides never create variants: pipelines use `MTLBufferLayoutStrideDynamic` and `setVertexBuffer:offset:attributeStride:atIndex:` (macOS 14).
- Blend state is part of the Metal pipeline, not of the `VariantKey`. Depth-stencil and rasterizer state are encoder state.
- `shader::derive_variant_keys(const PipelineState&)` (package I) is shared by the host and the runner, so tests exercise the host's derivation.

## Cache

- **Translation cache** (`src/shader_cache/translation_cache`).
  - Key: SHA-256 over the SHA-256 of the bytecode, the `VariantKey` bytes, and the translator id.
  - Translator id: SHA-256 of every source under `src/shader/`, generated at build time by `tools/shaders/source_digest.py`. Any compiler change invalidates entries with no manual version bump.
  - Value: the `CompiledShader` (MSL, entry name, `BindingLayout`, `StageInterface`). It is serialized in a fixed-width format with a magic, format version, key copy, payload length and SHA-256 checksum.
- **Pipeline archives** (`src/shader_cache/pipeline_archive`).
  - One `MTLBinaryArchive` file per pipeline key. The key hashes the translation keys of its functions, the Metal pipeline descriptor fields, the macOS build and the GPU name.
  - Lookup creates the pipeline with `MTLPipelineOptionFailOnBinaryArchiveMiss`. On a miss or failure it compiles normally, adds the functions to a new archive and serializes it.
  - One file per pipeline means archives never need merging between processes.
- **Location.** Defaults to `~/Library/Caches/DirectMetal/v1/`, resolved from the unix environment. `DIRECTMETAL_SHADER_CACHE_DIR` overrides it, and `none` disables the cache. Every test sets it to a temporary directory.
- **One cache per user.** Entries are content-addressed and safe to share between applications and Wine prefixes.
- **Concurrency.** Files go to `<root>/<kind>/<hh>/<hash>`. Writers create a temporary file in the same directory and `rename` it into place. Readers check the header and checksum, and treat a corrupt or truncated entry as a miss and delete it.
- **Size limit.** A budget (default 1 GiB) is enforced at host start by the one process that holds `<root>/evict.lock` (non-blocking `flock`). It removes the oldest files by access time.
- **Owner.** The host's `HostContext` owns the cache. Lookups and inserts are thread-safe. A cache I/O failure is logged once and never fails a call.

## Shader tests

### Test format

`docs/shaders/test-format.md` defines the format. Each test is a directory `tests/shaders/corpus/<stage>/<name>/` holding `test.shadertest` and its HLSL files. The format is line-based, with sections:

| Section | Purpose |
| --- | --- |
| `[shader vs\|ps\|cs]` | `file`, `entry`, `profiles` (any of `4_0`, `4_1`, `5_0`), `optimization` (`O3`, `Od`; both by default) |
| `[resource <register>]` | `kind` (`buffer`, `raw`, `structured`, `typed_buffer`, `texture2d`, …, `uav_*`, `cbuffer`), format or stride, size, mips, and data: `zero`, inline `u32:`/`f32:` lists, or a generated pattern |
| `[sampler s<n>]` | Filter, address modes, comparison function, LOD bias and range |
| `[dispatch]` | `groups = x y z` |
| `[draw]` | Topology, vertex data and input layout, instance count, RT formats and size, sample count, optional depth format and state |
| `[expect]` | Resources to read back |

The C++ spec parser (S8) enforces these limits:

- at most 16 resources and 64 KiB of initial data;
- render targets at most 64×64;
- at most 4,096 vertices or instances in total;
- at most 65,536 compute threads per dispatch.

### Corpus layout

The Windows job generates bytecode and references, which are committed and reviewed as diffs:

| Path | Contents |
| --- | --- |
| `tests/shaders/corpus/**` | Execution corpus (HLSL and specs) |
| `tests/shaders/compile_corpus/**` | Larger compile-only set from MIT-licensed sources (DirectX-SDK-Samples, DirectX-Graphics-Samples SM5 shaders), notices in `third_party/` |
| `tests/shaders/bytecode/<name>/<stage>_<profile>_<opt>.dxbc` | fxc output |
| `tests/reference/shaders/<name>/<variant>.json` | WARP readbacks, hex, as in Phase 1 |
| `tests/reference/shaders/<name>/reflection/<variant>.json` | `D3DReflect` dump: shader desc, constant buffers, variables and types, bound resources, all signatures |
| `tests/reference/shaders/<name>/tolerance.json` | Per-resource tolerance |
| `tests/reference/shaders/sources.json` | Digests of every HLSL and spec file, fxc flags, `d3dcompiler_47.dll` version, Windows SDK version |

### Windows job

`.github/workflows/shader-reference.yml` is a separate workflow so it does not touch Phase 1 and 2 references. It is `workflow_dispatch` only, runs on `windows-2025` with read-only permissions, and uses the same LLVM-MinGW pin as `reference.yml`. Its steps:

1. `tools/shaders/compile_corpus.py` runs the Windows SDK `fxc.exe` (path and version pinned and recorded) for every profile and optimization variant. Phase 3 uses fxc only; dxc produces DXIL and joins the corpus in Phase 7.
2. `tests/shaders/runner_windows/` is a D3D11 program built with LLVM-MinGW for x64 and x86. It executes each variant on WARP and dumps `D3DReflect` (loaded from `d3dcompiler_47.dll` with `LoadLibrary`). The x64 and x86 outputs must agree.
3. The artifact is imported with `tools/shaders/update.py --run <id>`, which reuses `tools/reference`'s artifact and run-check functions by import, without modifying them.

`tools/shaders/check_fresh.py` fails when a digest in `sources.json` no longer matches.

### macOS runner

`tests/shaders/runner/` is plain C++ and links `src/shader` and the backend through its public interface. No D3D11 front end is needed before Phase 4. For every variant it:

1. parses the spec;
2. loads the committed DXBC;
3. derives `VariantKey`s with `derive_variant_keys`;
4. compiles through the driver with `guard_loops` on;
5. creates resources, samplers and pipelines with S5;
6. writes argument buffers from the `BindingLayout`;
7. runs each test in its own command buffer;
8. checks the guard flag;
9. reads back and prints one JSON line per variant.

`tests/shaders/run_shader_tests.py` compares the output against the references with `tools/shaders/compare.py`. In Phase 4, the Windows runner's D3D11 path also builds natively against the DirectMetal front end, and the same corpus then runs through the full stack.

VS tests draw a point list, one vertex per pixel, with `nointerpolation` outputs that a pass-through PS writes to an `R32G32B32A32` target. This covers `[[point_size]]` specialization along the way.

### Tolerances

`tolerance.json` gives each read-back resource a `kind` and limits:

- **`exact`** (default): integer, bit and conversion results, and float `add`, `mul`, `min`, `max` and comparisons.
- **`float`** with `max_ulp` or `max_abs`, taken from the D3D11.3 functional spec's precision table:
  - `mad`: 1 ULP (fused or not);
  - `div`: 2.5 ULP;
  - `rcp`, `rsq`, `sqrt`, `exp`, `log`, `sincos`: per the spec's bounds.
- **`unorm8` / `float_filtered`** with a maximum channel difference and a mismatching-pixel ratio, for filtered sampling.

Any NaN matches any NaN. Inputs avoid denormals except in tests marked `denormals = either`. Tolerances are only tightened; loosening one needs explicit review ([ADR 0007](../adr/0007-testing-strategy.md)).

### GPU safety

These rules are mandatory:

- Every test that creates an `MTLDevice` runs through `tools/with_gpu_lock.sh` (`tools/build.sh --test` already does), with `is_parallel: false`. That includes the MSL compile tier and archive tests.
- Corpus HLSL has no data-dependent unbounded loops. Every loop's trip count is a literal at most 256, or `min(cb_value, 64)`. Reviewers check this.
- The runner refuses to execute any module compiled without `guard_loops`. A guarded shader that hits the limit fails its test instead of hanging the GPU.
- Workloads stay within the spec limits above, one command buffer per test, each waited on with a 10 s timeout. After the first timeout or device-lost error the runner stops submitting and exits non-zero, so it never piles work onto a GPU that may be stuck.
- Tests run sequentially in one process with one device. Execution runs in the `unix` (x86_64, the Wine configuration) and `unix-arm64` builds. The `sanitize` build runs compile and unit tiers only.
- No more than two GPU-heavy packages work at once. G and N are GPU-heavy; J, K and O run compile or archive tests only.

## Fuzzing

- **Targets** (`tests/fuzz/dxbc/`, Homebrew LLVM 21 or later, which ships `libclang_rt.fuzzer_osx.a`; Apple clang has no libFuzzer):
  - `dxbc_reader_fuzzer`: container, signatures, reflection, token decoding;
  - `dxbc_translate_fuzzer`: reader, lift, prepare passes, a fixed set of variant keys, then MSL emission. It never calls Metal.

  The checksum check is bypassed for both.
- **Build.** `tools/fuzz/build.sh` configures `build/fuzz` with `toolchains/macos-arm64-fuzz.ini` and `-fsanitize=fuzzer,undefined -fno-sanitize-recover=undefined`. It links Homebrew's libc++ consistently, with `_LIBCPP_HARDENING_MODE_DEBUG`. ASan is unavailable on macOS 27, so hardened bounds checks in `std::span`/`std::vector` stand in for it.
- **Running.** `tools/fuzz/run.sh <target> --hours N` runs the target with:
  - `taskpolicy -b nice -n 19`, so it is background QoS on efficiency cores and never takes the GPU lock;
  - `-workers=2 -jobs=2`, `-rss_limit_mb=2048`, `-timeout=10`, `-max_len=1048576`;
  - a corpus in `build/fuzz/corpus/<target>`, seeded from all committed bytecode;
  - artifacts in `build/fuzz/artifacts/`;
  - optionally `--keep-awake` (`caffeinate -i`).

  It writes `build/fuzz/reports/<target>-<start>.json` with the commit, binary hash, start, end, exec count, corpus size and crash count.
- **Crashes.** A crash is minimized with `-minimize_crash`, filed as an issue, and its input committed to `tests/fuzz/dxbc/regressions/`.
- **Replay.** `dxbc_fuzz_replay` is built with Apple clang into the `unit` suite of every native build (including UBSan). It replays the seeds and regressions under a plain `main`, so `run_local.sh` covers it with no fuzzing toolchain. `DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib` replay is a manual mode of `run.sh`.
- **The 24-hour run** is one continuous `run.sh dxbc_reader_fuzzer --hours 24` on the M1 Max at a fixed commit. If a reboot interrupts it, segments at the same commit and binary hash may be summed, and the report lists each segment. `dxbc_translate_fuzzer` runs continuously when the machine is idle, but is not an exit gate.

## Packages

Each package owns the listed paths; no two packages in a wave touch the same file. Tiers follow [ADR 0007](../adr/0007-testing-strategy.md): suite `unit` needs no GPU; suites `shader` (MSL compile), `gpu` and `shader_exec` run on Apple silicon with Metal validation in assert mode. `tools/ci/run_local.sh` must pass before merge. Every `src/shader/*/meson.build` stub is created by the interfaces PR and then owned by the package owning its directory.

### Wave 1

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| A. IR core | `src/shader/ir/**`, `tests/unit/shader/ir/**` | Builder and verifier reject every structured-CF violation (break out of a non-loop, wrong merge, merge not dominated, crossing constructs), block-parameter arity and type mismatches, handle stores or handle block parameters, and form violations; printer/parser round trip of every op from the op table; parser errors carry line and column | L |
| B. DXBC container, signatures, reflection | `src/shader/dxbc/container/**`, `src/shader/dxbc/reflection/**`, `tests/unit/shader/dxbc_container/**`, `tests/unit/shader/dxbc_reflection/**` | Hand-built containers with truncated, overlapping, oversize and misaligned chunks; checksum checked against fxc output; ISGN/OSGN/OSG5/PCSG/ISG1/OSG1 and RDEF (4.0, 4.1, 5.0 `RD11`) parsing; from D's first run on, reflection of every corpus variant equal to its `D3DReflect` JSON; builds in win32 and win64 | M–L |
| C. DXBC token decoder | `src/shader/dxbc/decode/**`, `tests/unit/shader/dxbc_decode/**` | Every SM4.0–5.0 opcode, operand form, index representation, extended opcode and operand token, declaration, custom-data block and HS phase decoded; opcode and operand enums checked against the pinned DirectX-Headers `d3d12TokenizedProgramFormat.hpp`; malformed lengths, unknown opcodes and runaway custom data rejected with offsets | L |
| D. Test format, Windows job, seed corpus | `docs/shaders/test-format.md`, `tests/shaders/spec/**`, `tests/shaders/runner_windows/**`, `tests/shaders/corpus/**`, `tests/shaders/bytecode/**`, `tests/reference/shaders/**`, `tools/shaders/**`, `.github/workflows/shader-reference.yml` | Spec parser rejects malformed sections and every limit; comparator (ULP, NaN, tolerance kinds) and freshness unit tests; one dispatched run with about 30 seed tests (CS ALU, buffers, PS sample and RT, VS points) committed | L |

### Wave 2

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| E. DXBC lifter | `src/shader/dxbc/lift/**`, `tests/unit/shader/dxbc_lift/**`, `tests/shaders/golden/lift/**` | Golden `registers`-form IR for every seed-corpus variant; lifting of every opcode family split across files; GS/HS/DS, doubles, class linkage and `Aon9` rejected with diagnostics | L |
| F. Pass manager and prepare passes | `src/shader/passes/**`, `tests/unit/shader/passes/**`, `tests/shaders/golden/passes/**` | Golden IR in/out per pass, driven from IR text (independent of E); `verify_each`; SSA promotion over nested loops with breaks and continues, switch fallthrough and indexable temps left in memory; each `lower_d3d_arithmetic` case; guard insertion for every access kind; `guard_loops` counter and flag | L |
| G. Backend pipelines and encoding (GPU) | `src/backend/pipelines/**`, `src/backend/commands/{render,compute,arguments}*.mm`, `tests/gpu/backend_pipelines/**` | With hand-written MSL: compile errors reported with text; compute dispatch through an argument buffer writing a buffer; render to a texture with dynamic-stride `stage_in`; typed buffer views; samplers including comparison; null textures; depth write; clockwise front face; raw argument layout matching the MSL struct; binary archive serialize, reload and `FailOnBinaryArchiveMiss`; no validation errors | L |
| H. Fuzzing | `tests/fuzz/**`, `tools/fuzz/**`, `toolchains/macos-arm64-fuzz.ini`, `tests/unit/shader/fuzz_replay/**` | Both targets build with Homebrew LLVM; a one-hour reader run with no crash; replay in the unit suite; report JSON; `run.sh` refuses to run without `taskpolicy`/`nice` | M |

### Wave 3

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| I. Binding layout and specialization | `src/shader/layout/**`, `src/shader/specialize/**`, `tests/unit/shader/layout/**`, `tests/shaders/golden/specialize/**` | Layout for full D3D11 limits (14/128/16/64, counters); deterministic offsets; `derive_variant_keys` for every DXGI RT and IA format class; upstream linking with subset and superset masks; point size; sample rate | M |
| J. MSL emitter (compile-only GPU) | `src/shader/msl/**`, `tests/unit/shader/msl/**`, `tests/shaders/golden/msl/**`, `tests/shader/msl_compile/**` | Construct-tree reconstruction for every CF shape in F's goldens; block parameters as locals; every op family; argument-buffer struct equal to the `BindingLayout`; every emitted golden compiles through `backend::create_library` (suite `shader`) | L |
| K. Shader cache | `src/shader_cache/**`, `tests/unit/shader_cache/**`, `tests/gpu/pipeline_archive/**` | Hit and miss; corrupt, truncated and wrong-version entries treated as misses and deleted; two processes inserting the same key; eviction under budget with the lock held elsewhere; translator-id invalidation; archive per pipeline reloaded in a second process | M |
| L. Corpus expansion | D's paths, `tests/shaders/compile_corpus/**`, `third_party/` notices | About 130 execution tests in three classes (below); a compile corpus of at least 200 shaders; every loop bounded; refreshed references | M–L |

Package L's execution tests:

- **CS, about 70:** every ALU op family; raw, structured and typed buffers; group-shared memory with barriers; order-independent atomics; UAV counters; nested if, loop and switch with break and continue; indexable temps; dynamic CB indexing; out-of-bounds and unbound reads.
- **PS, about 40:** sample, `_l`, `_b`, `_d`, `_c`, `_c_lz`, gather; `ld` and `ld_ms`; `resinfo` and `sampleinfo`; discard followed by derivatives; `SV_Depth`; 8 RTs; integer RTs; interpolation modes; `SV_IsFrontFace`; `SV_SampleIndex` at 4×; `SV_Position`.
- **VS, about 20:** IA formats; `SV_VertexID` and `SV_InstanceID`; instancing; clip distance.

### Wave 4

| Package | Owns | Proves it with | Size |
| --- | --- | --- | --- |
| M. Compiler driver, CLI, compile tier | `src/shader/driver/**`, `tools/shaderc/**`, `tests/shaders/compile_test/**` | `prepare`/`emit` split; `dm-shaderc --dump-ir=<pass> --emit-msl --variant --reflect`; every execution-corpus and compile-corpus variant emits MSL that compiles (suite `shader`), and also with `xcrun metal` when the Metal Toolchain is installed | M |
| N. macOS shader runner (GPU) | `tests/shaders/runner/**`, `tests/shaders/run_shader_tests.py`, `tests/shaders/meson.build`, `tests/shaders/tests/**` | Runner unit tests (guard trip, timeout stop, skip accounting); the full corpus in `unix` and `unix-arm64`; a deliberately broken loop trips the guard instead of hanging | M |
| O. Host and protocol v4 | `src/unix/host/shaders/**`, `src/bridge/client/shader_client*`, `src/bridge/client/include/directmetal/bridge/shader_client.hpp`, `tests/unit/host/shaders/**`, `tests/gpu/host_shaders/**` | `create_shader` with bad addresses (and WoW64 above 4 GiB), oversize, wrong stage, invalid and unsupported bytecode; dedup by digest; `destroy_object` on shaders and pipelines held until submit; pipeline creation through the protocol on the real backend filling a temporary cache; a second host process hitting the cache; `DIRECTMETAL_SHADER_DUMP_DIR` | M |

### Wave 5

Exit convergence. The integrator does the following:

- wires the exit tests and gates;
- triages every corpus failure into an issue, assigned to the owner of the failing directory, one owner per directory at a time;
- starts the 24-hour fuzz run once E, F, I and J stop changing the reader and lifter;
- cites the gates in the roadmap.

**Critical path:** interfaces → A and C → E (with F in parallel) → I and J → M → N → wave 5.

**Concurrency:** at most four packages run at once across Phases 2 and 3 together ([schedule](phases-2-3.md)). G is the only GPU-heavy package in wave 2, and N the only one in wave 4.

## Exit test

All gates run through `tools/ci/run_local.sh` with `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1` and `MTL_DEBUG_LAYER_ERROR_MODE=assert`:

- `shader_exec_corpus_unix` and `shader_exec_corpus_arm64` (suite `shader_exec`):
  - every execution-corpus variant (VS, PS, CS × profiles × `O3`/`Od`) compiles and runs;
  - every variant matches its reference within its tolerance;
  - the run reports no skips (`--require-all`);
  - no guard trips, timeouts, abnormal exits or validation errors.
- `shader_compile_corpus` (suite `shader`): every execution- and compile-corpus variant emits MSL that `create_library` accepts.
- `dxbc_reflection_corpus` (suite `unit`): reflection of every variant equals its `D3DReflect` JSON exactly.
- `shader_golden` (suite `unit`): IR and MSL goldens match.
- `dxbc_fuzz_replay` (suite `unit`, including the UBSan build).
- `shader_reference_fresh` (`run_local.sh reference`): bytecode and references match their sources.
- Fuzzing: a `dxbc_reader_fuzzer` report covering 24 hours at the exit commit with zero crashes. Its commit, exec count and corpus size are recorded in the roadmap's Phase 3 exit note.

## Decisions

| Decision | Choice |
| --- | --- |
| Compiler placement | Plain C++ library for all targets; PE side links only container, signature and reflection; translation, specialization and caching run unix-side |
| IR control flow | Structured CFG with block parameters and merge/continue annotations, verified; not a region tree |
| IR typing | Signless integers, scalarized DXBC registers, opaque resource handles, named variables instead of pointers |
| Binding model | Per-stage compact argument buffer at index 30 with an 8-byte entry per used slot and a size table; driver constants at 29; vertex buffers densely remapped into 0–28 |
| Vertex strides | `MTLBufferLayoutStrideDynamic`; strides never create variants |
| Specialization | IR passes over a fixed-width `VariantKey`; no function constants in Phase 3 |
| Robustness | D3D out-of-bounds and divide/shift/convert semantics lowered in IR, not left to MSL |
| Math | MSL 3.1, fast math off |
| Doubles, min precision | Doubles unsupported (no fp64 on Apple GPUs); min precision lifted to 32-bit |
| Cache | Content-addressed per user under `~/Library/Caches/DirectMetal`; translator id from source digests; one binary archive per pipeline |
| Protocol | v4 adds `create_shader`, `create_compute_pipeline`, `create_render_pipeline`; no new stream opcodes; draw, dispatch and binding opcodes come in Phase 4 |
| Shader execution before Phase 4 | Runner drives the backend directly; the Windows runner uses D3D11 on WARP |
| Reference generation | Separate `shader-reference.yml`; fxc only; x64 and x86 WARP must agree |
| MSL compile gate | `MTLDevice newLibraryWithSource` under the GPU lock; `xcrun metal` added when the Metal Toolchain is installed |
| Fuzzing | Homebrew LLVM libFuzzer and UBSan with hardened libc++; background QoS, never GPU; replay in the unit suite |

## Risks

| Risk | Mitigation |
| --- | --- |
| A miscompiled loop hangs the GPU and stalls WindowServer | Mandatory `guard_loops` in every executed shader; bounded corpus loops; small workloads; one command buffer per test with a 10 s wait; stop on first timeout; GPU lock; at most 2 GPU-heavy packages |
| Executed shaders differ from production shaders by `guard_loops` | The guard is one late pass; compile-tier and golden tests cover unguarded MSL; the guard's IR is covered by F's goldens |
| Metal Toolchain not installed (`xcrun metal` fails locally) | Runtime compilation is the gate; `xcrun metal` is optional |
| Homebrew libFuzzer and libc++ mixing | Fuzz build uses Homebrew clang and libc++ throughout in its own build directory; replay builds with Apple clang |
| No ASan on macOS 27 | Hardened libc++, UBSan, `libgmalloc` replay mode, bounds-checked span reader |
| WARP and Apple GPU numeric differences (denormals, `mad` fusion, transcendentals, filtering) | Spec-derived tolerances, inputs away from denormals, tighten-only reviews |
| Unverified Metal semantics: `discard_fragment` vs demote, `[[position]].w`, texture atomics on Apple7, depth-write pipelines without a depth attachment | Each has a corpus test; the Windows reference decides; workarounds as IR passes |
| fxc or WARP drift on runner images | SDK and `d3dcompiler_47.dll` versions recorded; bytecode committed; regenerations reviewed as diffs |
| Round-trip latency of Windows-only bytecode generation slows corpus authoring | Batch corpus additions per dispatch; `dm-shaderc` works on committed bytecode offline |
| Opcode tables and lifter exceed file and function limits | Split by family from the interfaces PR; handler tables |
| Runtime compile stalls in titles | Translation cache and per-pipeline archives now; async compile and prewarming in Phase 8 |
| Binary archives under Rosetta or after OS updates | Key includes the macOS build; `FailOnBinaryArchiveMiss` falls back to compiling; K's two-process test runs in the x86_64 build |
| DXIL's unstructured CFG strains the IR | CFG IR holds it as-is; a structurizer establishes the structured form before emission |

## Deferred decisions

- GS, HS, DS and stream-output lifting and emulation passes (Phase 4). The reader decodes them now.
- Vertex pulling when more than 29 IA slots are used, or for IA formats Metal lacks (Phase 4).
- Draw, dispatch and binding stream opcodes, and host-side argument buffer writing (Phase 4).
- The `ID3D11ShaderReflection` COM object: `D3D10ReflectShader` in Phase 5. `d3dcompiler_47`'s `D3DReflect` stays Wine's.
- Class linkage (`fcall`, interfaces): unsupported until a title needs it.
- SM5.1 register spaces and ranges, and DXIL (Phase 7). The IR already models them.
- Function constants, AIR emission and asynchronous compilation (Phase 8).
- Cache configuration through the runtime config file. The unix side currently reads only its environment.

## Coordination with Phase 2

The joint order of work is in the [schedule](phases-2-3.md).

Both phases change the following. The integrator lands both phases' interface additions in one protocol-v4 PR, so `FunctionCode` order and `protocol_version` change once. Calls not yet implemented answer `status_not_implemented`.

- `src/bridge/include/directmetal/bridge/protocol.hpp`: `protocol_version = 4`, appended `FunctionCode`s. Phase 3's parameter structs go in the new `shaders.hpp`, so `control.hpp` is untouched by Phase 3.
- `src/bridge/include/directmetal/bridge/commands.hpp`: Phase 3 changes only the `destroy_object` comment (shaders and pipelines). Phase 2 likely appends a present opcode.
- `src/unix/host/{calls.hpp, host.cpp (call table), objects.hpp, command_executor.cpp (destroy_object), meson.build}`. Phase 3's code lives in `src/unix/host/shaders/`.
- `src/backend/include/directmetal/backend/{device.hpp, commands.hpp}`, `src/backend/{device.mm, resource_access.hpp, meson.build}`, `src/backend/commands/{recorder.hpp, recorder.mm, meson.build}`: the interfaces PR adds virtuals and forwarding stubs once. Phase 3's code lives in `src/backend/pipelines/` and new `commands/*.mm`.
- Host and backend fakes in `tests/unit/host/` gain stubs for the new virtuals.
- Presentation blits in Phase 2 that need shaders: until G lands, keep them in Phase 2's private backend code; switch to `create_library`/`create_render_pipeline` afterwards.
- `src/runtime/config_options.cpp` registry, if either phase adds options.
- `tools/ci/run_local.sh`: Phase 3 adds `shader_reference_fresh` to the `reference` check. Phase 3 does not change `tools/build.sh` or `reference.yml`.
- One-line `subdir`/`foreach` additions in the top-level `meson.build`, `tests/meson.build`, `tests/unit/meson.build`, `tests/gpu/meson.build`, and `tests/compile/interfaces_*.cpp`.
- `docs/ROADMAP.md` exit notes.
