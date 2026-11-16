#!/usr/bin/env bash
# Downloads pinned DirectX headers and copies Metal headers from the local macOS SDK.
# Headers are not committed; see docs/api/README.md for licensing notes.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
dest="${1:-$repo_root/third_party/api-headers}"
metal_frameworks=(Metal MetalFX MetalKit QuartzCore IOSurface)

fetch_directx() {
  local group repo revision path target
  while IFS=$'\t' read -r group repo revision path; do
    [[ -z "$group" || "$group" == \#* ]] && continue
    target="$dest/$group/$(basename "$path")"
    mkdir -p "$(dirname "$target")"
    curl -fsSL "https://raw.githubusercontent.com/$repo/$revision/$path" -o "$target" \
      || { echo "fetch failed: $repo@$revision:$path" >&2; exit 1; }
  done < "$script_dir/sources.tsv"
}

copy_metal() {
  local sdk framework headers
  sdk="$(xcrun --sdk macosx --show-sdk-path)"
  for framework in "${metal_frameworks[@]}"; do
    headers="$sdk/System/Library/Frameworks/$framework.framework/Headers"
    [[ -d "$headers" ]] || { echo "missing SDK framework: $headers" >&2; exit 1; }
    mkdir -p "$dest/metal/$framework"
    cp "$headers"/*.h "$dest/metal/$framework/"
  done
  xcrun --sdk macosx --show-sdk-version > "$dest/metal/SDK_VERSION"
}

fetch_directx
copy_metal
echo "headers written to $dest"
