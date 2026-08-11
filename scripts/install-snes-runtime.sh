#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEVICE_HOST=${DEVICE_HOST:-root@192.168.31.117}
REMOTE_SYSTEM=${REMOTE_SYSTEM:-/mnt/SDCARD/.system/tg5040}
CORE="$PROJECT_DIR/vendor/tg5040/cores/snes9x2005_plus_libretro.so"
MINARCH="$PROJECT_DIR/vendor/tg5040/bin/minarch.elf"
PAK="$PROJECT_DIR/vendor/tg5040/paks/Emus/SFC.pak"
RES="$PROJECT_DIR/../MinUI/skeleton/SYSTEM/res"

for required in "$CORE" "$MINARCH" "$PAK/launch.sh" "$RES/sfc-brick-mask.png" "$RES/sfc-crt-3x.png"; do
    if [ ! -f "$required" ]; then
        echo "Missing SNES runtime file: $required" >&2
        exit 1
    fi
done

ssh "$DEVICE_HOST" "mkdir -p '$REMOTE_SYSTEM/cores' '$REMOTE_SYSTEM/paks/Emus/SFC.pak' '$REMOTE_SYSTEM/res' /mnt/SDCARD/Roms/SFC /mnt/SDCARD/Saves/SFC && if [ -f '$REMOTE_SYSTEM/bin/minarch.elf' ] && [ ! -f '$REMOTE_SYSTEM/bin/minarch.elf.pre-sfc' ]; then cp '$REMOTE_SYSTEM/bin/minarch.elf' '$REMOTE_SYSTEM/bin/minarch.elf.pre-sfc'; fi"
scp "$CORE" "$DEVICE_HOST:$REMOTE_SYSTEM/cores/snes9x2005_plus_libretro.so"
scp "$MINARCH" "$DEVICE_HOST:$REMOTE_SYSTEM/bin/minarch.elf"
scp "$PAK/launch.sh" "$PAK/default.cfg" "$PAK/default-brick.cfg" "$DEVICE_HOST:$REMOTE_SYSTEM/paks/Emus/SFC.pak/"
scp "$RES/sfc-brick-mask.png" "$RES/sfc-crt-3x.png" "$DEVICE_HOST:$REMOTE_SYSTEM/res/"
ssh "$DEVICE_HOST" "chmod +x '$REMOTE_SYSTEM/bin/minarch.elf' '$REMOTE_SYSTEM/paks/Emus/SFC.pak/launch.sh' && sha256sum '$REMOTE_SYSTEM/cores/snes9x2005_plus_libretro.so' '$REMOTE_SYSTEM/bin/minarch.elf' '$REMOTE_SYSTEM/res/sfc-brick-mask.png' '$REMOTE_SYSTEM/res/sfc-crt-3x.png'"

echo "SNES runtime installed on $DEVICE_HOST"
