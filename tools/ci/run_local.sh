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
export PATH="$DIRECTMETAL_LLVM_MINGW/bin:$DIRECTMETAL_WINE/bin:$PATH"

checks=(lint unix unix-arm64 tools-tests sanitize pe wine-api api-status)

run_check() {
    case "$1" in
        lint)
            git ls-files -z '*.c' '*.cc' '*.cpp' '*.h' '*.hpp' '*.m' '*.mm' \
                | xargs -0 clang-format --dry-run --Werror &&
                ruff format --check && ruff check
            ;;
        unix | unix-arm64 | sanitize | wine-api) tools/build.sh --test "$1" ;;
        tools-tests) python3 -I -m unittest discover -s tools/api/tests ;;
        pe) tools/build.sh win64 win32 ;;
        api-status) python3 -I tools/api/report_status.py ;;
        *)
            echo "run_local.sh: unknown check: $1" >&2
            return 2
            ;;
    esac
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
