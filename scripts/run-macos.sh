#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build/macos"}
FONT=${FONT:-/tmp/trimui-regular.ttf}

exec "$BUILD_DIR/trimui-rmlui-prototype" \
    --assets "$PROJECT_DIR/assets" \
    --font "$FONT" \
    "$@"
