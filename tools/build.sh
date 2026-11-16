#!/usr/bin/env bash
# Configures, builds and optionally tests DirectMetal targets, one Meson build directory each.
# Usage: tools/build.sh [--test] [target...]
# Targets: win64 win32 unix unix-arm64 sanitize (default: win64 win32 unix).
# DIRECTMETAL_LLVM_MINGW names an LLVM-MinGW install whose bin/ is prepended to PATH.
# DIRECTMETAL_SANITIZERS overrides the sanitize target's sanitizer list (default: address,undefined).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
default_targets=(win64 win32 unix)
is_testing=0
setup_options=()

# Sanitizers build arm64: ASan aborts during process start-up under Rosetta 2.
set_setup_options() {
  case "$1" in
    win64) setup_options=(--cross-file=toolchains/win64.ini) ;;
    win32) setup_options=(--cross-file=toolchains/win32.ini) ;;
    unix) setup_options=(--cross-file=toolchains/macos-x86_64.ini) ;;
    unix-arm64) setup_options=(--native-file=toolchains/macos-arm64.ini) ;;
    sanitize)
      setup_options=(--native-file=toolchains/macos-arm64.ini
        "-Db_sanitize=${DIRECTMETAL_SANITIZERS:-address,undefined}" -Db_lundef=false)
      ;;
    *) echo "unknown target: $1" >&2; return 1 ;;
  esac
}

build_target() {
  local target="$1" build_dir="build/$1"
  set_setup_options "$target"
  if [[ ! -f "$build_dir/build.ninja" ]]; then
    meson setup "$build_dir" "${setup_options[@]}"
  fi
  meson compile -C "$build_dir"
  if [[ "$is_testing" == 1 && "$target" != win* ]]; then
    meson test -C "$build_dir" --print-errorlogs
  fi
}

main() {
  local targets=() target
  for arg in "$@"; do
    case "$arg" in
      --test) is_testing=1 ;;
      *) targets+=("$arg") ;;
    esac
  done
  [[ ${#targets[@]} -gt 0 ]] || targets=("${default_targets[@]}")
  if [[ -n "${DIRECTMETAL_LLVM_MINGW:-}" ]]; then
    PATH="$DIRECTMETAL_LLVM_MINGW/bin:$PATH"
  fi
  cd "$repo_root"
  for target in "${targets[@]}"; do
    build_target "$target"
  done
}

main "$@"
