#!/usr/bin/env bash
# Runs the CI checks from .github/workflows/ci.yml on this machine and reports each one.
# Usage: tools/ci/run_local.sh [check...]   (default: every check, in order)
# DIRECTMETAL_SANITIZERS defaults to undefined because ASan is broken on macOS 27.
# The workflow's Windows DLL load job needs Windows and is not run here.
set -uo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export DIRECTMETAL_LLVM_MINGW="${DIRECTMETAL_LLVM_MINGW:-$HOME/.local/opt/llvm-mingw}"
export DIRECTMETAL_WINE="${DIRECTMETAL_WINE:-$HOME/.local/opt/wine-devel.app/Contents/Resources/wine}"
export DIRECTMETAL_SANITIZERS="${DIRECTMETAL_SANITIZERS:-undefined}"
# Homebrew first: macOS ships Python 3.9, and the tools need 3.11 or later.
export PATH="$DIRECTMETAL_LLVM_MINGW/bin:$DIRECTMETAL_WINE/bin:${DIRECTMETAL_TOOLS_BIN:-/opt/homebrew/bin}:$PATH"

checks=(lint unix unix-arm64 tools-tests reference sanitize api-native pe wine-api api-status)
native_targets=(unix unix-arm64 sanitize)

run_check() {
    case "$1" in
        lint)
            git ls-files -z '*.c' '*.cc' '*.cpp' '*.h' '*.hpp' '*.m' '*.mm' \
                | xargs -0 clang-format --dry-run --Werror &&
                ruff format --check && ruff check
            ;;
        unix | unix-arm64 | sanitize | wine-api) tools/build.sh --test "$1" ;;
        tools-tests)
            python3 -I -m unittest discover -s tools/api/tests &&
                python3 -I -m unittest discover -s tests/api/tests
            ;;
        reference)
            python3 -I -m unittest discover -s tools/reference/tests &&
                python3 -I tools/reference/check_fresh.py
            ;;
        api-native) run_api_native ;;
        pe) tools/build.sh win64 win32 ;;
        api-status) python3 -I tools/api/report_status.py ;;
        *)
            echo "run_local.sh: unknown check: $1" >&2
            return 2
            ;;
    esac
}

has_api_tests() {
    meson test -C "build/$1" --list --no-rebuild --suite api 2> /dev/null | grep '^api - ' > /dev/null
}

# The api suite of every native build, each run holding the GPU lock. Reports a skip while no
# native build registers api tests.
run_api_native() {
    local target is_found=0
    tools/build.sh "${native_targets[@]}" || return 1
    for target in "${native_targets[@]}"; do
        has_api_tests "$target" || continue
        is_found=1
        tools/with_gpu_lock.sh meson test -C "build/$target" --suite api \
            --num-processes "${DIRECTMETAL_TEST_JOBS:-2}" --print-errorlogs || return 1
    done
    [[ "$is_found" == 1 ]] || echo "run_local.sh: no native api tests registered; api-native skipped"
}

main() {
    local selected=("$@") failed=() check
    [[ ${#selected[@]} -gt 0 ]] || selected=("${checks[@]}")
    cd "$repo_root" || exit 1
    for check in "${selected[@]}"; do
        echo "==> $check"
        if run_check "$check"; then
            echo "==> $check: pass"
        else
            echo "==> $check: FAIL"
            failed+=("$check")
        fi
    done
    if [[ ${#failed[@]} -gt 0 ]]; then
        echo "run_local.sh: failed: ${failed[*]}" >&2
        exit 1
    fi
    echo "run_local.sh: all checks passed (${selected[*]})"
}

main "$@"
