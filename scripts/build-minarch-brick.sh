#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SYSROOT="$PROJECT_DIR/.deps/tg5040-sysroot"
MINARCH_DIR="$PROJECT_DIR/vendor/minarch"
SOURCE_DIR="$MINARCH_DIR/src"
COMMON_DIR="$SOURCE_DIR/common"
PLATFORM_DIR="$SOURCE_DIR/platform/tg5040"
INCLUDE_DIR="$MINARCH_DIR/include"
OUTPUT_DIR="$PROJECT_DIR/vendor/tg5040/bin"
OUTPUT="$OUTPUT_DIR/minarch.elf"

if [ ! -f "$SYSROOT/usr/include/SDL2/SDL.h" ]; then
    echo "TrimUI SDK is missing. Run scripts/setup-toolchain.sh first." >&2
    exit 1
fi
if [ ! -f "$SOURCE_DIR/minarch.c" ] || [ ! -f "$PLATFORM_DIR/platform.c" ] || [ ! -f "$INCLUDE_DIR/libretro.h" ]; then
    echo "Vendored MinArch source is incomplete: $MINARCH_DIR" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
export ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-/tmp/trimui-zig-cache}

zig cc -target aarch64-linux-gnu \
    "$SOURCE_DIR/minarch.c" \
    "$COMMON_DIR/scaler.c" "$COMMON_DIR/utils.c" "$COMMON_DIR/api.c" \
    "$PLATFORM_DIR/platform.c" \
    -o "$OUTPUT" \
    -I"$SOURCE_DIR" -I"$COMMON_DIR" -I"$PLATFORM_DIR" -I"$INCLUDE_DIR" \
    -isystem "$SYSROOT/usr/include" -isystem "$SYSROOT/usr/include/SDL2" \
    -L"$SYSROOT/usr/lib" -Wl,-rpath-link,"$SYSROOT/usr/lib" \
    -DPLATFORM=\"tg5040\" -DUSE_SDL2 -DBUILD_DATE=\"rmlui-brick\" -DBUILD_HASH=\"vendored-minarch\" \
    -O2 -std=gnu99 -fomit-frame-pointer \
    -ldl -lmsettings -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lpthread -lm -lz

chmod +x "$OUTPUT"
file "$OUTPUT"
shasum -a 256 "$OUTPUT"
