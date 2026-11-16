# DirectMetal

Open-source translation layer that implements the Direct3D 9, 10, 11 and 12 APIs on top of Apple's Metal.

Status: planning. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Building

Requires Meson, Ninja, Xcode and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) on `PATH` (or set `DIRECTMETAL_LLVM_MINGW` to its install directory).

```sh
tools/build.sh                      # win64 and win32 PE DLLs, x86_64 unix library
tools/build.sh --test unix          # build and run unit tests
tools/build.sh --test sanitize      # arm64 build and tests with ASan and UBSan
```

Targets are `win64`, `win32`, `unix` (x86_64, as Wine runs under Rosetta 2), `unix-arm64` and `sanitize`. Each builds into `build/<target>`. `DIRECTMETAL_SANITIZERS` overrides the sanitizer list, e.g. `DIRECTMETAL_SANITIZERS=undefined`.

Format C++ with `clang-format -i` and Python with `ruff format`.

## License

[MIT](LICENSE).
