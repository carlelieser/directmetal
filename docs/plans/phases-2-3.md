# Phases 2 and 3 — joint schedule

Phases 2 ([plan](phase-2.md)) and 3 ([plan](phase-3.md)) run in parallel. This page fixes what they share and the order in which their packages start.

## Shared interfaces PR

One PR lands both phases' interfaces before any package starts, after Phase 2's spikes X1 and X2:

- `protocol_version = 4`. `FunctionCode` 16–25 are Phase 2's display and swap-chain calls, 26–28 Phase 3's `create_shader`, `create_compute_pipeline` and `create_render_pipeline`. Unimplemented calls answer `status_not_implemented`.
- `stream_version = 2` with `Opcode::present` appended; Phase 3 adds no stream opcodes.
- New virtuals on `backend::Device`, `CommandRecorder` and `Queue` for presenters (Phase 2 I9) and pipelines (Phase 3 S5), each forwarding to a stub that returns `Status::unsupported`; host and backend fakes gain the same stubs.
- Empty directories and `meson.build` hooks for every package of both phases, and ADR 0011 (window surfaces).

After it lands, `protocol.hpp`, `commands.hpp`, `backend/{device,commands}.hpp`, `src/backend/meson.build` and the top-level `meson.build` files change only through the integrator.

## Path boundaries

| Shared area | Phase 2 | Phase 3 |
| --- | --- | --- |
| Host | `src/unix/host/**` except `shaders/` (package C) | `src/unix/host/shaders/**` (package O) |
| Backend | `src/backend/present/**`, `src/backend/displays/**`, `commands/present.mm` | `src/backend/pipelines/**`, `commands/{render,compute,arguments}*.mm` |
| Bridge headers | `presentation.hpp`, `command_layouts.hpp` | `shaders.hpp` |
| `tools/ci/run_local.sh` | Package H adds `present` and `conformance-dxgi` | The integrator adds `shader_reference_fresh` to `reference` |

Phase 2's present pass uses its own fixed MSL until Phase 3's package G lands, then moves onto `create_library` and `create_render_pipeline` as a separate refactor.

## Order of work

At most four packages run at once across both phases, and at most two of them use the GPU (marked †). Fuzzing runs at background priority and never takes the GPU lock.

| Step | Phase 2 | Phase 3 |
| --- | --- | --- |
| 1 | X1, X2 spikes | Interfaces drafting |
| 2 | Shared interfaces PR (integrator) | |
| 3 | A, C | A, C |
| 4 | B † , D | B, D |
| 5 | E, G | E, F, G † |
| 6 | F, H | H, I |
| 7 | I1, I2 | J, K |
| 8 | M, K † | L |
| 9 | J, N † | M |
| 10 | L | N † , O |
| 11 | O † , P | Exit convergence, 24-hour fuzz run |
| 12 | Exit wiring | |

Each step starts as packages from the one before merge, so a step may hold more than four packages only in sequence, never at once. A step slips rather than exceed either cap.
