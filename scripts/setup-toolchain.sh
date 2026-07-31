#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEPS_DIR="$PROJECT_DIR/.deps"
SYSROOT_DIR="$DEPS_DIR/tg5040-sysroot"
ARCHIVE="$DEPS_DIR/SDK_usr_tg5040_a133p.tgz"
SDK_URL="https://github.com/trimui/toolchain_sdk_smartpro/releases/download/20231018/SDK_usr_tg5040_a133p.tgz"
SDK_MD5="44a4cf5adb8fbff2fb0f83a30d2f96fb"

if ! command -v zig >/dev/null 2>&1; then
    echo "zig is required. On macOS: brew install zig" >&2
    exit 1
fi

mkdir -p "$DEPS_DIR"
if [ ! -f "$ARCHIVE" ]; then
    curl -L -o "$ARCHIVE" "$SDK_URL"
fi

ACTUAL_MD5=$(md5 -q "$ARCHIVE")
if [ "$ACTUAL_MD5" != "$SDK_MD5" ]; then
    echo "TrimUI SDK checksum mismatch: $ACTUAL_MD5" >&2
    exit 1
fi

if [ ! -f "$SYSROOT_DIR/usr/include/SDL2/SDL.h" ]; then
    mkdir -p "$SYSROOT_DIR"
    tar -xzf "$ARCHIVE" -C "$SYSROOT_DIR"
fi

echo "Toolchain ready: zig $(zig version) + $SYSROOT_DIR"
