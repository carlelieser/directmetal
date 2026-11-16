# DirectMetal

Open-source translation layer that implements the Direct3D 9, 10, 11 and 12 APIs on top of Apple's Metal.

Status: planning. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Building

Requires Meson, Ninja, Xcode and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) on `PATH` (or set `DIRECTMETAL_LLVM_MINGW` to its install directory).

```sh
tools/build.sh                      # win64 and win32 PE DLLs, x86_64 unix library
tools/build.sh --test unix          # build and run unit tests
tools/build.sh --test sanitize      # arm64 build and tests with ASan and UBSan
tools/build.sh --test wine-api      # win64, win32 and unix builds; API tests under Wine
```

Targets are `win64`, `win32`, `unix` (x86_64, as Wine runs under Rosetta 2), `unix-arm64`, `sanitize` and `wine-api`. Each builds into `build/<target>`; `wine-api` builds `build/unix`, `build/win64` and `build/win32`. `DIRECTMETAL_SANITIZERS` overrides the sanitizer list, e.g. `DIRECTMETAL_SANITIZERS=undefined`.

API tests need Wine: `wine` on `PATH`, or `DIRECTMETAL_WINE` set to a Wine install root containing `bin/wine`. Each test creates a throwaway `WINEPREFIX` in its build directory, places `directmetal.dll` and `directmetal.so` on `WINEDLLPATH` (`x86_64-windows` or `i386-windows`, plus `x86_64-unix`) and loads `d3d11.dll` from the test program's directory with `WINEDLLOVERRIDES=d3d11=n;directmetal=b`. The win32 run exercises WoW64. Override the unix library location with `-Dunix_library=<path>`.

The Wine tests write call logs to `build/<target>/tests/api/call-logs`. The suite's last test, `d3d11_status_gate`, runs `tools/api/report_status.py` on them with the Phase 0 exit requirements and writes the report to `build/<target>/tests/api/api-status`.

The minimum supported Wine version is 11.0; CI tests Wine 11.21. The bridge depends on Wine's unixlib ABI: `directmetal.dll` loads `directmetal.so` through `NtQueryVirtualMemory` information class 1000 (`MemoryWineLoadUnixLib`, formerly `MemoryWineUnixFuncs`) and calls it through ntdll's `__wine_unix_call_dispatcher` export, and `directmetal.so` exports `__wine_unix_call_funcs` and `__wine_unix_call_wow64_funcs`. This ABI exists since Wine 8.0, but earlier releases than 11.0 are untested.

ASan is known to be broken on macOS 27: x86_64 processes abort at start-up and arm64 processes hang during shadow-memory initialization, so tests time out. CI runs the `sanitize` target on hosted runners; locally on macOS 27 use `DIRECTMETAL_SANITIZERS=undefined`.

Format C++ with `clang-format -i` and Python with `ruff format`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [coding standards](docs/CODING_STANDARDS.md).

## License

[MIT](LICENSE).
