#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build/brick-zig"}
PACKAGE_DIR=${PACKAGE_DIR:-"$PROJECT_DIR/build/package/rmlui-prototype"}
BINARY="$BUILD_DIR/trimui-rmlui-prototype"

if [ ! -x "$BINARY" ]; then
    echo "Brick binary is missing. Run scripts/build-brick.sh first." >&2
    exit 1
fi

mkdir -p "$PACKAGE_DIR/assets"
mkdir -p "$PACKAGE_DIR/runtime/tg5040/bin" "$PACKAGE_DIR/runtime/tg5040/cores" \
    "$PACKAGE_DIR/runtime/tg5040/paks/Emus" "$PACKAGE_DIR/runtime/tg5040/res"
cp "$BINARY" "$PACKAGE_DIR/trimui-rmlui-prototype"
if command -v llvm-strip >/dev/null 2>&1; then
    llvm-strip "$PACKAGE_DIR/trimui-rmlui-prototype"
elif [ -x /opt/homebrew/opt/llvm@21/bin/llvm-strip ]; then
    /opt/homebrew/opt/llvm@21/bin/llvm-strip "$PACKAGE_DIR/trimui-rmlui-prototype"
else
    echo "warning: llvm-strip was not found; packaging an unstripped binary" >&2
fi
cp -R "$PROJECT_DIR/assets/." "$PACKAGE_DIR/assets/"
cp "$PROJECT_DIR/assets/launch-brick.sh" "$PACKAGE_DIR/launch.sh"
cp "$PROJECT_DIR/vendor/tg5040/bin/minarch.elf" "$PACKAGE_DIR/runtime/tg5040/bin/minarch.elf"
cp "$PROJECT_DIR/vendor/tg5040/cores/"*.so "$PACKAGE_DIR/runtime/tg5040/cores/"
cp -R "$PROJECT_DIR/vendor/tg5040/paks/Emus/." "$PACKAGE_DIR/runtime/tg5040/paks/Emus/"
cp -R "$PROJECT_DIR/vendor/tg5040/res/." "$PACKAGE_DIR/runtime/tg5040/res/"
chmod +x "$PACKAGE_DIR/trimui-rmlui-prototype" "$PACKAGE_DIR/launch.sh"
find "$PACKAGE_DIR/runtime/tg5040/paks/Emus" -name launch.sh -exec chmod +x {} \;
chmod +x "$PACKAGE_DIR/runtime/tg5040/bin/minarch.elf"

echo "Package ready: $PACKAGE_DIR"
du -sh "$PACKAGE_DIR"
