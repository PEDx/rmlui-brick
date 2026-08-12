#!/bin/sh

EMU_EXE=picodrive

EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
MINARCH_MD_SHADER=${MINARCH_MD_SHADER:-sharp}
MD_SHADER_PRESET_FILE="$USERDATA_PATH/MD-shader"
if [ -f "$MD_SHADER_PRESET_FILE" ]; then
	IFS= read -r MINARCH_MD_SHADER < "$MD_SHADER_PRESET_FILE"
fi
case "$MINARCH_MD_SHADER" in
	sharp|crt|consumer|composite|off) ;;
	*) MINARCH_MD_SHADER=sharp ;;
esac
export MINARCH_MD_SHADER
cd "$HOME"
minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" &> "$LOGS_PATH/$EMU_TAG.txt"
