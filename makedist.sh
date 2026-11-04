#!/bin/sh
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
PROJECT=${PROJECT:-sunstone}
EXPLOIT_NAME=preload
EXPLOIT_BUILD=build/$PROJECT/bin/preload
EXTRAS=

CACHE=${CACHE:-$HOME/android-ndk-cache}
NDK_VER=${NDK_VER:-android-ndk-r29}
NDK_DIR=${NDK_DIR:-$CACHE/$NDK_VER}
VERSION=${VERSION:-$(cat "$HERE/VERSION")}
[ -n "$VERSION" ] || { echo "No version in $HERE/VERSION"; exit 1; }
NAME=ghostlock-$PROJECT-v$VERSION
DIST=$HERE/dist
STAGE=$DIST/$NAME
ZIP=$DIST/$NAME.zip

fetch() {
  if [ -f "$2" ]; then
    echo "Cached $(basename "$2")"
    return 0
  fi
  echo "Fetching $(basename "$2")"
  mkdir -p "$(dirname "$2")"
  if curl -fsSL --retry 3 -o "$2.part" "$1"; then
    mv "$2.part" "$2"
  else
    rm -f "$2.part"
    return 1
  fi
}

mkdir -p "$CACHE"

case "$(uname -s)/$(uname -m)" in
  Linux/x86_64|Linux/amd64) NDK_HOST=linux ;;
  Darwin/*) NDK_HOST=darwin ;;
  *) NDK_HOST= ;;
esac
if [ -d "$NDK_DIR/toolchains/llvm/prebuilt/$NDK_HOST-x86_64" ] || { [ -z "$NDK_HOST" ] && [ -d "$NDK_DIR" ]; }; then
  echo "Toolchain present at $NDK_DIR"
elif [ -n "$NDK_HOST" ]; then
  fetch "https://dl.google.com/android/repository/$NDK_VER-$NDK_HOST.zip" "$CACHE/$NDK_VER-$NDK_HOST.zip"
  echo "Unpacking toolchain"
  unzip -q -o "$CACHE/$NDK_VER-$NDK_HOST.zip" -d "$CACHE"
else
  echo "No prebuilt NDK for $(uname -s) $(uname -m); set NDK_DIR to an NDK that runs here"
  exit 1
fi
[ -d "$NDK_DIR" ] || { echo "Toolchain missing after unpack: $NDK_DIR"; exit 1; }

for os in linux darwin windows; do
  fetch "https://dl.google.com/android/repository/platform-tools-latest-$os.zip" \
        "$CACHE/platform-tools-$os.zip" || echo "Could not fetch platform-tools for $os"
done

echo "Building GhostLock $VERSION for $PROJECT"
ANDROID_NDK_HOME="$NDK_DIR" make -C "$HERE" -B PROJECT="$PROJECT" >/dev/null
BIN=$HERE/$EXPLOIT_BUILD
[ -f "$BIN" ] || { echo "Build produced no $EXPLOIT_NAME"; exit 1; }

rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE/bin"
cp "$BIN" "$STAGE/bin/$EXPLOIT_NAME"
cp "$HERE/root.sh" "$HERE/root.ps1" "$HERE/root.bat" "$HERE/README.md" "$STAGE/"
chmod 755 "$STAGE/root.sh"
echo "$VERSION" > "$STAGE/VERSION"

if [ -n "$EXTRAS" ]; then
  set -- $EXTRAS
  if [ -f "$HERE/$1" ]; then
    for f in "$@"; do
      [ -f "$HERE/$f" ] || { echo "$1 is present but $f is missing"; exit 1; }
      cp "$HERE/$f" "$STAGE/"
      case "$f" in
        *.sh) chmod 755 "$STAGE/$f" ;;
      esac
    done
  else
    echo "No $1, packaging without: $EXTRAS"
  fi
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

MISSING=
for os in linux darwin windows; do
  case "$os" in
    darwin) dest=mac ;;
    *) dest=$os ;;
  esac
  if [ ! -f "$CACHE/platform-tools-$os.zip" ]; then
    MISSING="$MISSING $dest"
    continue
  fi
  unzip -q -o "$CACHE/platform-tools-$os.zip" -d "$TMP/$os"
  mkdir -p "$STAGE/bin/$dest"
  if [ "$os" = windows ]; then
    for f in adb.exe AdbWinApi.dll AdbWinUsbApi.dll; do
      cp "$TMP/$os/platform-tools/$f" "$STAGE/bin/$dest/$f"
    done
  else
    cp "$TMP/$os/platform-tools/adb" "$STAGE/bin/$dest/adb"
    chmod 755 "$STAGE/bin/$dest/adb"
  fi
done

( cd "$DIST" && zip -qr "$NAME.zip" "$NAME" )

echo
echo "Wrote $ZIP"
unzip -l "$ZIP" | sed '1,3d' | sed '$d' | sed '$d'
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$ZIP"
else
  shasum -a 256 "$ZIP"
fi
if [ -n "$MISSING" ]; then
  echo "No bundled adb for:$MISSING (users on those hosts need adb on PATH)"
fi
