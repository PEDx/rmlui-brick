#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEVICE_HOST=${DEVICE_HOST:-root@192.168.31.117}
RUNTIME_DIR=${RUNTIME_DIR:-"$PROJECT_DIR/vendor/tg5040"}
REMOTE_SYSTEM=${REMOTE_SYSTEM:-/mnt/SDCARD/.system/tg5040}
REMOTE_RES=${REMOTE_RES:-/mnt/SDCARD/.system/res}
REMOTE_STAGE=${REMOTE_STAGE:-/mnt/SDCARD/.system/.rmlui-runtime-stage}

for required in \
    "$RUNTIME_DIR/bin/minarch.elf" \
    "$RUNTIME_DIR/cores/gambatte_libretro.so" \
    "$RUNTIME_DIR/cores/gpsp_libretro.so" \
    "$RUNTIME_DIR/cores/snes9x2005_plus_libretro.so" \
    "$RUNTIME_DIR/cores/picodrive_libretro.so" \
    "$RUNTIME_DIR/paks/Emus/GB.pak/launch.sh" \
    "$RUNTIME_DIR/paks/Emus/GBC.pak/launch.sh" \
    "$RUNTIME_DIR/paks/Emus/GBA.pak/launch.sh" \
    "$RUNTIME_DIR/paks/Emus/SFC.pak/launch.sh" \
    "$RUNTIME_DIR/paks/Emus/MD.pak/launch.sh" \
    "$RUNTIME_DIR/paks/Emus/GG.pak/launch.sh" \
    "$RUNTIME_DIR/res/gb-brick-mask.png" \
    "$RUNTIME_DIR/res/gba-brick-mask.png" \
    "$RUNTIME_DIR/res/gbc-brick-mask.png" \
    "$RUNTIME_DIR/res/sfc-brick-mask.png" \
    "$RUNTIME_DIR/res/md-brick-mask.png" \
    "$RUNTIME_DIR/res/gg-brick-mask.png"; do
    if [ ! -f "$required" ]; then
        echo "Missing emulator runtime file: $required" >&2
        exit 1
    fi
done

ssh "$DEVICE_HOST" "mkdir -p \
    '$REMOTE_STAGE/bin' '$REMOTE_STAGE/cores' '$REMOTE_STAGE/res' \
    '$REMOTE_STAGE/paks/Emus/GB.pak' '$REMOTE_STAGE/paks/Emus/GBC.pak' \
    '$REMOTE_STAGE/paks/Emus/GBA.pak' '$REMOTE_STAGE/paks/Emus/SFC.pak' '$REMOTE_STAGE/paks/Emus/MD.pak' '$REMOTE_STAGE/paks/Emus/GG.pak' \
    '$REMOTE_SYSTEM/bin' '$REMOTE_SYSTEM/cores' '$REMOTE_RES' \
    '$REMOTE_SYSTEM/paks/Emus/GB.pak' '$REMOTE_SYSTEM/paks/Emus/GBC.pak' \
    '$REMOTE_SYSTEM/paks/Emus/GBA.pak' '$REMOTE_SYSTEM/paks/Emus/SFC.pak' '$REMOTE_SYSTEM/paks/Emus/MD.pak' '$REMOTE_SYSTEM/paks/Emus/GG.pak' \
    /mnt/SDCARD/Roms/GB /mnt/SDCARD/Roms/GBC /mnt/SDCARD/Roms/GBA /mnt/SDCARD/Roms/SFC /mnt/SDCARD/Roms/MD /mnt/SDCARD/Roms/GG \
    /mnt/SDCARD/Saves/GB /mnt/SDCARD/Saves/GBC /mnt/SDCARD/Saves/GBA /mnt/SDCARD/Saves/SFC /mnt/SDCARD/Saves/MD /mnt/SDCARD/Saves/GG"

scp "$RUNTIME_DIR/bin/minarch.elf" "$DEVICE_HOST:$REMOTE_STAGE/bin/minarch.elf"
scp "$RUNTIME_DIR/cores/"*.so "$DEVICE_HOST:$REMOTE_STAGE/cores/"
scp "$RUNTIME_DIR/res/"*.png "$DEVICE_HOST:$REMOTE_STAGE/res/"
for system in GB GBC GBA SFC MD GG; do
    scp "$RUNTIME_DIR/paks/Emus/$system.pak/"* "$DEVICE_HOST:$REMOTE_STAGE/paks/Emus/$system.pak/"
done

ssh "$DEVICE_HOST" "set -e
if [ -f '$REMOTE_SYSTEM/bin/minarch.elf' ] && [ ! -f '$REMOTE_SYSTEM/bin/minarch.elf.pre-rmlui-vendor' ]; then
    cp '$REMOTE_SYSTEM/bin/minarch.elf' '$REMOTE_SYSTEM/bin/minarch.elf.pre-rmlui-vendor'
fi
chmod +x '$REMOTE_STAGE/bin/minarch.elf'
mv '$REMOTE_STAGE/bin/minarch.elf' '$REMOTE_SYSTEM/bin/minarch.elf'
for file in '$REMOTE_STAGE'/cores/*.so; do mv \"\$file\" '$REMOTE_SYSTEM/cores/'; done
for file in '$REMOTE_STAGE'/res/*.png; do mv \"\$file\" '$REMOTE_RES/'; done
for system in GB GBC GBA SFC MD GG; do
    for file in '$REMOTE_STAGE'/paks/Emus/\$system.pak/*; do
        mv \"\$file\" '$REMOTE_SYSTEM'/paks/Emus/\$system.pak/
    done
    chmod +x '$REMOTE_SYSTEM'/paks/Emus/\$system.pak/launch.sh
done
sha256sum \
    '$REMOTE_SYSTEM/bin/minarch.elf' \
    '$REMOTE_SYSTEM/cores/gambatte_libretro.so' \
    '$REMOTE_SYSTEM/cores/gpsp_libretro.so' \
    '$REMOTE_SYSTEM/cores/snes9x2005_plus_libretro.so' \
    '$REMOTE_SYSTEM/cores/picodrive_libretro.so' \
    '$REMOTE_RES/gba-brick-mask.png' \
    '$REMOTE_RES/md-brick-mask.png' \
    '$REMOTE_RES/gg-brick-mask.png'"

echo "GB/GBC/GBA/SFC/MD/GG runtime installed on $DEVICE_HOST"
