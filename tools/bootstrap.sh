#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
baseline="$(sed -n 's/.*"builtin-baseline": "\([a-f0-9]*\)".*/\1/p' "$project_root/vcpkg.json")"
[[ "$baseline" =~ ^[a-f0-9]{40}$ ]] || { echo 'Invalid vcpkg baseline' >&2; exit 1; }
vcpkg_directory="$project_root/.tools/vcpkg"
mkdir -p "$project_root/.tools/binary-cache" "$project_root/.tools/downloads"
if [[ ! -d "$vcpkg_directory/.git" ]]; then
  git clone --no-checkout --depth 1 https://github.com/microsoft/vcpkg.git "$vcpkg_directory"
fi
if [[ "$(git -C "$vcpkg_directory" rev-parse HEAD)" != "$baseline" ]]; then
  [[ -z "$(git -C "$vcpkg_directory" status --porcelain)" ]] || { echo 'Modified vcpkg checkout' >&2; exit 1; }
  git -C "$vcpkg_directory" fetch --depth 1 origin "$baseline"
fi
git -C "$vcpkg_directory" checkout --detach "$baseline"
"$vcpkg_directory/bootstrap-vcpkg.sh" -disableMetrics
