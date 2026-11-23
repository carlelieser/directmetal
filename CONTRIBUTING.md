# Contributing to DirectMetal

Read the [roadmap](docs/ROADMAP.md), the [architecture decision records](docs/adr/README.md) and the [coding standards](docs/CODING_STANDARDS.md) before starting non-trivial work. For anything beyond a small fix, open or comment on an issue first so the approach can be agreed.

## License

Contributions are accepted under the [MIT license](LICENSE). Pull requests containing code copied or translated from LGPL/GPL code, including Wine, vkd3d, vkd3d-proton and DXMT, are not accepted ([ADR 0001](docs/adr/0001-license.md)).

## Building and testing

Requirements, targets and commands are in the [README](README.md#building). Before opening a pull request, run at least:

```sh
tools/build.sh --test unix
tools/build.sh --test sanitize
```

Changes to PE-side code must also pass `tools/build.sh win64 win32`.

ASan is broken on macOS 27, so local runs there may use `DIRECTMETAL_SANITIZERS=undefined tools/build.sh --test sanitize`. Run the full ASan and UBSan build on a machine with macOS 26 or earlier.

Format C++ and Objective-C++ with `clang-format -i`, and Python with `ruff format` and `ruff check`.

GPU tests run with Metal validation enabled ([ADR 0007](docs/adr/0007-testing-strategy.md)):

```sh
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 tools/build.sh --test unix
```

## Branches and commits

- Branch from `main` as `<type>/<short-name>`, e.g. `feat/dxgi-factory`, `fix/bc7-upload`.
- One logical change per commit. Refactors, formatting and behavior changes are separate commits, and every commit builds and passes tests.
- Subjects are imperative, terse and prefixed by type: `feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `chore:`. The body explains why, not what.

```
fix: clamp mip count for BC formats below block size
```

## Pull requests

- Keep each pull request to one topic. Describe the change and how it was tested.
- Tests follow [ADR 0007](docs/adr/0007-testing-strategy.md); a bug fix's regression test fails without the fix.
- Results that differ from the [Windows reference](docs/adr/0007-testing-strategy.md#oracle) are fixed, or tracked as an issue and linked from the pull request.
- `tools/ci/run_local.sh` must pass; it runs the same checks as `.github/workflows/ci.yml`, which is currently disabled. Rebase on `main` rather than merging it into your branch.

## Reporting issues

Use the [issue forms](https://github.com/carlelieser/directmetal/issues/new/choose): a bug report for an application or API misbehaving, a conformance failure for a failing Wine or vkd3d-proton test.
