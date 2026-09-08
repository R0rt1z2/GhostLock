#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version="${VERSION:-$(cat "$here/VERSION" 2>/dev/null || echo 0.0.0)}"
name="ghostlock-sheldon-v${version}"
stage="$here/dist/$name"

echo "[*] building exploit"
make -C "$here" clean >/dev/null
make -C "$here"

echo "[*] staging $name"
rm -rf "$stage"
mkdir -p "$stage"
cp "$here/build/ghostlock_root" "$stage/"
cp "$here/root.sh" "$stage/"
cp "$here/README.md" "$stage/"
cp "$here/VERSION" "$stage/"

echo "[*] zipping"
( cd "$here/dist" && zip -qr "$name.zip" "$name" )
sha256sum "$here/dist/$name.zip"
echo "[*] wrote dist/$name.zip"
