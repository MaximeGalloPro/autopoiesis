#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONTEXT_DIR="$ROOT/.context"
TOOLCHAIN_DIR="$CONTEXT_DIR/toolchain"
DEPENDENCY_DIR="$CONTEXT_DIR/deps"
BUILD_DIR="${AUTOPOIESIS_PREVIEW_BUILD_DIR:-$CONTEXT_DIR/build}"

mkdir -p "$TOOLCHAIN_DIR" "$DEPENDENCY_DIR" "$CONTEXT_DIR/preview-data"

download_verified() {
  local url="$1" expected="$2" destination="$3" temporary actual
  if [[ -f "$destination" ]]; then
    actual="$(sha256sum "$destination" | awk '{print $1}')"
    [[ "$actual" == "$expected" ]] && return 0
  fi
  temporary="$(mktemp "$CONTEXT_DIR/download.XXXXXX")"
  trap 'rm -f "$temporary"' RETURN
  curl --fail --location --silent --show-error "$url" --output "$temporary"
  actual="$(sha256sum "$temporary" | awk '{print $1}')"
  if [[ "$actual" != "$expected" ]]; then
    printf 'Checksum invalide pour %s\n' "$url" >&2
    return 1
  fi
  mv "$temporary" "$destination"
  trap - RETURN
}

if command -v cmake >/dev/null 2>&1; then
  CMAKE_BIN="$(command -v cmake)"
else
  if [[ "$(uname -m)" != "x86_64" ]]; then
    printf 'CMake absent et architecture portable non prise en charge: %s\n' "$(uname -m)" >&2
    exit 1
  fi
  CMAKE_VERSION="3.31.8"
  CMAKE_HOME="$TOOLCHAIN_DIR/cmake-$CMAKE_VERSION-linux-x86_64"
  CMAKE_ARCHIVE="$TOOLCHAIN_DIR/cmake-$CMAKE_VERSION-linux-x86_64.tar.gz"
  if [[ ! -x "$CMAKE_HOME/bin/cmake" ]]; then
    download_verified \
      "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-linux-x86_64.tar.gz" \
      "630615d8e98ac33eba7fbe472626dff5c899c85af3c024585ae109166a6909d0" \
      "$CMAKE_ARCHIVE"
    tar -xzf "$CMAKE_ARCHIVE" -C "$TOOLCHAIN_DIR"
  fi
  CMAKE_BIN="$CMAKE_HOME/bin/cmake"
fi

CURL_VERSION="7.88.1"
CURL_HOME="$DEPENDENCY_DIR/curl-$CURL_VERSION"
CURL_ARCHIVE="$DEPENDENCY_DIR/curl-$CURL_VERSION.tar.gz"
if [[ ! -f "$CURL_HOME/include/curl/curl.h" ]]; then
  download_verified \
    "https://curl.se/download/curl-$CURL_VERSION.tar.gz" \
    "cdb38b72e36bc5d33d5b8810f8018ece1baa29a8f215b4495e495ded82bbf3c7" \
    "$CURL_ARCHIVE"
  tar -xzf "$CURL_ARCHIVE" -C "$DEPENDENCY_DIR"
fi

if [[ -f /usr/lib/x86_64-linux-gnu/libcurl.so.4 ]]; then
  CURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so.4
elif [[ -f /lib/x86_64-linux-gnu/libcurl.so.4 ]]; then
  CURL_LIBRARY=/lib/x86_64-linux-gnu/libcurl.so.4
else
  printf 'Bibliothèque runtime libcurl.so.4 introuvable.\n' >&2
  exit 1
fi

"$CMAKE_BIN" -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCURL_INCLUDE_DIR="$CURL_HOME/include" \
  -DCURL_LIBRARY="$CURL_LIBRARY"
"$CMAKE_BIN" --build "$BUILD_DIR" --target autopoiesis_backend -j2

bun install --cwd "$ROOT/web" --frozen-lockfile
bun run --cwd "$ROOT/web" typecheck
bun test --cwd "$ROOT/web"
bun run --cwd "$ROOT/web" build

printf 'Nuagent prêt: backend=%s, frontend=%s\n' \
  "$BUILD_DIR/autopoiesis_backend" "$ROOT/web/dist"
