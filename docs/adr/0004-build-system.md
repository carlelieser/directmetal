# 0004 — Build system

Status: accepted

## Context

Each build produces two kinds of binaries: Windows PE DLLs and a macOS unix-side library.

## Decision

- Meson is the build system.
- PE targets build with a cross file for `x86_64-w64-mingw32` and `i686-w64-mingw32` (LLVM-MinGW toolchain).
- The unix-side library builds natively with Apple clang from Xcode.
- Formatting: `clang-format` for C++/Objective-C++, `ruff format` for Python tooling.

## Consequences

- CI installs LLVM-MinGW and Xcode on macOS runners.
- One `meson setup` per target; a top-level script drives both.
