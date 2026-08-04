#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=${1:-/Volumes/MINUI/Roms}
DESTINATION=${2:-/tmp/trimui-test-library}
MANIFEST="$SCRIPT_DIR/test-library.tsv"

command -v curl >/dev/null 2>&1 || {
    echo "curl is required" >&2
    exit 1
}
command -v jq >/dev/null 2>&1 || {
    echo "jq is required" >&2
    exit 1
}
command -v file >/dev/null 2>&1 || {
    echo "file is required" >&2
    exit 1
}

while IFS='|' read -r system source_directory rom_filename repository boxart_filename; do
    case "$system" in
        ''|'#'*) continue ;;
    esac

    source_rom="$SOURCE_ROOT/$source_directory/$rom_filename"
    system_destination="$DESTINATION/$system"
    cover_destination="$system_destination/.media/${rom_filename%.*}.png"
    if [ ! -f "$source_rom" ]; then
        echo "Missing ROM: $source_rom" >&2
        exit 1
    fi

    mkdir -p "$system_destination/.media"
    cp -p "$source_rom" "$system_destination/$rom_filename"

    encoded_boxart=$(jq -nr --arg value "$boxart_filename" '$value|@uri')
    boxart_url="https://raw.githubusercontent.com/libretro-thumbnails/$repository/master/Named_Boxarts/$encoded_boxart"
    curl -fsSL --retry 2 -o "$cover_destination" "$boxart_url"

    # A few entries in the thumbnail repositories are aliases whose complete
    # file content is another PNG filename. Resolve that indirection once.
    if ! file "$cover_destination" | grep -q 'PNG image data'; then
        alias_filename=$(sed -n '1p' "$cover_destination")
        case "$alias_filename" in
            *.png)
                encoded_alias=$(jq -nr --arg value "$alias_filename" '$value|@uri')
                alias_url="https://raw.githubusercontent.com/libretro-thumbnails/$repository/master/Named_Boxarts/$encoded_alias"
                curl -fsSL --retry 2 -o "$cover_destination" "$alias_url"
                ;;
        esac
    fi
    if ! file "$cover_destination" | grep -q 'PNG image data'; then
        echo "Downloaded cover is not a PNG: $boxart_filename" >&2
        exit 1
    fi
    printf '%s: %s\n' "$system" "$rom_filename"
done < "$MANIFEST"
