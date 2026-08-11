#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
RMLUI_SOURCE=${RMLUI_SOURCE:-"$PROJECT_DIR/.deps/RmlUi-6.2"}
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build/macos"}
MACOS_SDK=$(xcrun --sdk macosx --show-sdk-path)

RMLUI_SOURCE="$RMLUI_SOURCE" "$PROJECT_DIR/scripts/setup-rmlui.sh"
if [ ! -f "$RMLUI_SOURCE/CMakeLists.txt" ]; then
    echo "RmlUi 6.2 source not found at: $RMLUI_SOURCE" >&2
    exit 1
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-isystem $MACOS_SDK/usr/include/c++/v1" \
    -DFETCHCONTENT_SOURCE_DIR_RMLUI="$RMLUI_SOURCE"
cmake --build "$BUILD_DIR" --parallel
