#!/usr/bin/env bash
# Download a pinned sccache release binary into DEST_DIR (default: .ci-cache/sccache-bin).
#
# Usage: ci/linux/scripts/install-sccache.sh
#
# Env:
#   SCCACHE_VERSION=v0.19.0
#   DEST_DIR=.ci-cache/sccache-bin

set -euo pipefail

SCCACHE_VERSION="${SCCACHE_VERSION:-v0.19.0}"
DEST_DIR="${DEST_DIR:-.ci-cache/sccache-bin}"
mkdir -p "${DEST_DIR}"

arch="$(uname -m)"
case "${arch}" in
  x86_64)  target=x86_64-unknown-linux-musl ;;
  aarch64) target=aarch64-unknown-linux-musl ;;
  *)
    echo "ERROR: unsupported Linux arch ${arch} for sccache" >&2
    exit 1
    ;;
esac

asset="sccache-${SCCACHE_VERSION}-${target}.tar.gz"
url="https://github.com/mozilla/sccache/releases/download/${SCCACHE_VERSION}/${asset}"
tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

echo "==> Downloading ${url}"
curl -fsSL "${url}" -o "${tmp}/${asset}"
tar -xzf "${tmp}/${asset}" -C "${tmp}"
# Archive contains a single top-level dir with the binary.
find "${tmp}" -type f -name 'sccache' -exec cp -f {} "${DEST_DIR}/sccache" \;
chmod +x "${DEST_DIR}/sccache"
"${DEST_DIR}/sccache" --version
