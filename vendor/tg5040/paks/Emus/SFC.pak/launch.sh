#!/bin/sh

EMU_EXE=snes9x2005_plus

EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
MINARCH_SFC_SHADER=${MINARCH_SFC_SHADER:-sharp}
SFC_SHADER_PRESET_FILE="$USERDATA_PATH/SFC-shader"
if [ -f "$SFC_SHADER_PRESET_FILE" ]; then
	IFS= read -r MINARCH_SFC_SHADER < "$SFC_SHADER_PRESET_FILE"
fi
case "$MINARCH_SFC_SHADER" in
	sharp|crt|consumer|composite|off) ;;
	*) MINARCH_SFC_SHADER=sharp ;;
esac
export MINARCH_SFC_SHADER
cd "$HOME"
minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" &> "$LOGS_PATH/$EMU_TAG.txt"
