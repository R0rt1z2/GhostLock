#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version="${VERSION:-$(cat "$here/VERSION" 2>/dev/null || echo 0.0.0)}"
name="ghostlock-kara-v${version}"
stage="$here/dist/$name"
cache="${CACHE:-$HOME/android-ndk-cache}"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fetch() {
  url=$1
  out=$2
  [ -f "$out" ] && return 0
  echo "[*] fetching $(basename "$out")"
  mkdir -p "$(dirname "$out")"
  curl -sSL --retry 3 -o "$out.part" "$url" && mv "$out.part" "$out"
}

bundle() {
  os=$1
  dest=$2
  zipf="$cache/platform-tools-$os.zip"
  [ -f "$zipf" ] || return 1
  rm -rf "$tmp/$os"
  unzip -q -o "$zipf" -d "$tmp/$os"
  mkdir -p "$stage/bin/$dest"
  if [ "$os" = windows ]; then
    for f in adb.exe AdbWinApi.dll AdbWinUsbApi.dll; do
      cp "$tmp/$os/platform-tools/$f" "$stage/bin/$dest/$f"
    done
  else
    cp "$tmp/$os/platform-tools/adb" "$stage/bin/$dest/adb"
    chmod 755 "$stage/bin/$dest/adb"
  fi
}

echo "[*] building exploit"
make -C "$here" clean >/dev/null
make -C "$here"

echo "[*] staging $name"
rm -rf "$stage"
mkdir -p "$stage"
cp "$here/build/ghostlock_root" "$stage/"
cp "$here/root.sh" "$stage/"
cp "$here/root.ps1" "$stage/"
cp "$here/root.bat" "$stage/"
cp "$here/VERSION" "$stage/"
chmod 755 "$stage/root.sh"

echo "[*] bundling adb"
for os in linux darwin windows; do
  fetch "https://dl.google.com/android/repository/platform-tools-latest-$os.zip" \
        "$cache/platform-tools-$os.zip" || true
done
missing=""
bundle linux linux || missing="$missing linux"
bundle darwin mac || missing="$missing mac"
bundle windows windows || missing="$missing windows"

echo "[*] zipping"
rm -f "$here/dist/$name.zip"
( cd "$here/dist" && zip -qr "$name.zip" "$name" )
unzip -l "$here/dist/$name.zip" | tail -n +4 | head -n -2
sha256sum "$here/dist/$name.zip"
if [ -n "$missing" ]; then
  echo "[!] no bundled adb for:$missing (users on those hosts need adb on PATH)"
fi
echo "[*] wrote dist/$name.zip"
