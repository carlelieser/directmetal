# Contributing to DirectMetal

Read the [roadmap](docs/ROADMAP.md), the [architecture decision records](docs/adr/README.md) and the [coding standards](docs/CODING_STANDARDS.md) before starting non-trivial work. For anything beyond a small fix, open or comment on an issue first so the approach can be agreed.

## License

Contributions are accepted under the [MIT license](LICENSE). Never copy or translate code from Wine, vkd3d, vkd3d-proton, DXMT or any other LGPL/GPL project; they are behavioral reference only ([ADR 0001](docs/adr/0001-license.md)). Pull requests that contain such code are not accepted.

## Building and testing

Requirements, targets and commands are in the [README](README.md#building). Before opening a pull request, run at least:

```sh
tools/build.sh --test unix
tools/build.sh --test sanitize
```

ASan is broken on macOS 27, so local runs there may use `DIRECTMETAL_SANITIZERS=undefined tools/build.sh --test sanitize`. CI runs the full ASan and UBSan build on hosted runners.

Format C++ and Objective-C++ with `clang-format -i`, and Python with `ruff format` and `ruff check`.

GPU tests run with Metal validation enabled:

```sh
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 tools/build.sh --test unix
```

A validation error is a test failure.

## Branches and commits

- Branch from `main` as `<type>/<short-name>`, e.g. `feat/dxgi-factory`, `fix/bc7-upload`.
- One logical change per commit. Refactors, formatting and behavior changes are separate commits, and every commit builds and passes tests.
- Subjects are imperative, terse and prefixed by type: `feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `chore:`. The body explains why, not what.

```
fix: clamp mip count for BC formats below block size
```

## Pull requests

- Keep each pull request to one topic. Describe the change and how it was tested.
- New behavior and every bug fix come with tests at the lowest tier that exercises them, per [ADR 0007](docs/adr/0007-testing-strategy.md). A bug fix's regression test must fail without the fix.
- GPU tests pass with Metal API and shader validation enabled.
- Results that differ from the Windows reference are fixed, or tracked as an issue and linked from the pull request.
- CI must pass. Rebase on `main` rather than merging it into your branch.

## Reporting issues

Use the [issue forms](https://github.com/carlelieser/directmetal/issues/new/choose): a bug report for an application or API misbehaving, a conformance failure for a failing Wine or vkd3d-proton test.
