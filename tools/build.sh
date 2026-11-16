#!/usr/bin/env bash
# Configures, builds and optionally tests DirectMetal targets, one Meson build directory each.
# Usage: tools/build.sh [--test] [target...]
# Targets: win64 win32 unix unix-arm64 sanitize wine-api (default: win64 win32 unix).
# wine-api builds win64, win32 and unix; with --test it runs the api suite under Wine.
# DIRECTMETAL_LLVM_MINGW names an LLVM-MinGW install whose bin/ is prepended to PATH.
# DIRECTMETAL_WINE names a Wine install root whose bin/wine runs API tests (default: wine on PATH).
# DIRECTMETAL_SANITIZERS overrides the sanitize target's sanitizer list (default: address,undefined).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
default_targets=(win64 win32 unix)
is_testing=0
setup_options=()

fail() {
  echo "build.sh: $*" >&2
  exit 1
}

# Sanitizers build arm64. ASan is broken on macOS 27 for both architectures, so CI runs this
# target on hosted runners with earlier macOS releases.
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
    *) fail "unknown target: $1" ;;
  esac
}

require_compiler() {
  command -v "$1" > /dev/null \
    || fail "$1 not found on PATH; set DIRECTMETAL_LLVM_MINGW to an LLVM-MinGW install"
}

require_wine() {
  if [[ -n "${DIRECTMETAL_WINE:-}" ]]; then
    [[ -x "$DIRECTMETAL_WINE/bin/wine" ]] \
      || fail "DIRECTMETAL_WINE=$DIRECTMETAL_WINE has no executable bin/wine"
  else
    command -v wine > /dev/null \
      || fail "wine not found on PATH; set DIRECTMETAL_WINE to a Wine install root"
  fi
}

check_target() {
  case "$1" in
    wine-api)
      check_target win64
      check_target win32
      [[ "$is_testing" == 0 ]] || require_wine
      return
      ;;
  esac
  set_setup_options "$1"
  case "$1" in
    win64) require_compiler x86_64-w64-mingw32-clang++ ;;
    win32) require_compiler i686-w64-mingw32-clang++ ;;
  esac
}

compile_target() {
  local target="$1" build_dir="build/$1"
  set_setup_options "$target"
  if [[ -f "$build_dir/build.ninja" ]]; then
    meson setup --reconfigure "$build_dir" "${setup_options[@]}" || fail "setup failed for $target"
  else
    meson setup "$build_dir" "${setup_options[@]}" || fail "setup failed for $target"
  fi
  meson compile -C "$build_dir" || fail "compile failed for $target"
}

# API tests load the unix library from build/unix, so it is built before the PE targets.
build_wine_api() {
  local target
  for target in unix win64 win32; do
    compile_target "$target"
  done
  [[ "$is_testing" == 1 ]] || return 0
  for target in win64 win32; do
    meson test -C "build/$target" --suite api --print-errorlogs \
      || fail "api tests failed for $target"
  done
}

build_target() {
  case "$1" in
    wine-api) build_wine_api ;;
    win*) compile_target "$1" ;;
    *)
      compile_target "$1"
      if [[ "$is_testing" == 1 ]]; then
        meson test -C "build/$1" --print-errorlogs || fail "test failed for $1"
      fi
      ;;
  esac
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
    check_target "$target"
  done
  for target in "${targets[@]}"; do
    build_target "$target"
  done
}

main "$@"
