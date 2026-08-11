#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
RMLUI_SOURCE=${RMLUI_SOURCE:-"$PROJECT_DIR/.deps/RmlUi-6.2"}
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build/brick-zig"}

RMLUI_SOURCE="$RMLUI_SOURCE" "$PROJECT_DIR/scripts/setup-rmlui.sh"
if [ ! -f "$RMLUI_SOURCE/CMakeLists.txt" ]; then
    echo "RmlUi 6.2 source not found at: $RMLUI_SOURCE" >&2
    exit 1
fi

"$PROJECT_DIR/scripts/setup-toolchain.sh"
"$PROJECT_DIR/scripts/build-minarch-brick.sh"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/zig-aarch64-linux.cmake" \
    -DCMAKE_AR:FILEPATH="$PROJECT_DIR/scripts/zig-ar.sh" \
    -DCMAKE_RANLIB:FILEPATH="$PROJECT_DIR/scripts/zig-ranlib.sh" \
    -DFETCHCONTENT_SOURCE_DIR_RMLUI="$RMLUI_SOURCE"
cmake --build "$BUILD_DIR" --parallel

file "$BUILD_DIR/trimui-rmlui-prototype"
