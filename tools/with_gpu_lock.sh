#!/usr/bin/env bash
# Runs a command while holding this machine's GPU test lock, at reduced CPU priority.
# Usage: tools/with_gpu_lock.sh command [arg...]
# Concurrent GPU test runs with Metal validation have stalled WindowServer until macOS logged the
# user out, so every test run that may touch the GPU goes through this lock, one at a time.
# DIRECTMETAL_GPU_LOCK overrides the lock file (default: ~/.cache/directmetal/gpu.lock).
set -euo pipefail

[[ $# -gt 0 ]] || { echo "with_gpu_lock.sh: no command given" >&2; exit 2; }
lock_file="${DIRECTMETAL_GPU_LOCK:-$HOME/.cache/directmetal/gpu.lock}"
mkdir -p "$(dirname "$lock_file")"
echo "with_gpu_lock.sh: waiting for $lock_file" >&2
exec lockf -k "$lock_file" nice -n 10 "$@"
