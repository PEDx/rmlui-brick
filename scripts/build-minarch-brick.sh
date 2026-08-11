#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO_DIR=$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)
MINUI_DIR="$REPO_DIR/MinUI"
SYSROOT="$PROJECT_DIR/.deps/tg5040-sysroot"
OUTPUT_DIR="$PROJECT_DIR/vendor/tg5040/bin"
OUTPUT="$OUTPUT_DIR/minarch.elf"

if [ ! -f "$SYSROOT/usr/include/SDL2/SDL.h" ]; then
    echo "TrimUI SDK is missing. Run scripts/setup-toolchain.sh first." >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
export ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-/tmp/trimui-zig-cache}

cd "$MINUI_DIR/workspace/all/minarch"
zig cc -target aarch64-linux-gnu \
    minarch.c ../common/scaler.c ../common/utils.c ../common/api.c ../../tg5040/platform/platform.c \
    -o "$OUTPUT" \
    -I. -I./libretro-common/include -I../common -I../../tg5040/platform \
    -I../../tg5040/libmsettings -isystem "$SYSROOT/usr/include" -isystem "$SYSROOT/usr/include/SDL2" \
    -L"$SYSROOT/usr/lib" -Wl,-rpath-link,"$SYSROOT/usr/lib" \
    -DPLATFORM=\"tg5040\" -DUSE_SDL2 -DBUILD_DATE=\"brick-sfc\" -DBUILD_HASH=\"brick-sfc\" \
    -O2 -std=gnu99 -fomit-frame-pointer \
    -ldl -lmsettings -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lpthread -lm -lz

chmod +x "$OUTPUT"
file "$OUTPUT"
