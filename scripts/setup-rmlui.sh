#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
RMLUI_SOURCE=${RMLUI_SOURCE:-"$PROJECT_DIR/.deps/RmlUi-6.2"}
RMLUI_COMMIT=2230d1a6e8e0848ed87a5761e2a5160b2a175ba4

if [ ! -f "$RMLUI_SOURCE/CMakeLists.txt" ]; then
    command -v git >/dev/null 2>&1 || {
        echo "git is required to fetch RmlUi" >&2
        exit 1
    }
    mkdir -p "$(dirname "$RMLUI_SOURCE")"
    git clone --depth 1 --branch 6.2 https://github.com/mikke89/RmlUi.git "$RMLUI_SOURCE"
fi

if [ -d "$RMLUI_SOURCE/.git" ]; then
    ACTUAL_COMMIT=$(git -C "$RMLUI_SOURCE" rev-parse HEAD)
    if [ "$ACTUAL_COMMIT" != "$RMLUI_COMMIT" ]; then
        echo "RmlUi revision mismatch: expected $RMLUI_COMMIT, found $ACTUAL_COMMIT" >&2
        exit 1
    fi
fi

echo "RmlUi ready: $RMLUI_SOURCE"
