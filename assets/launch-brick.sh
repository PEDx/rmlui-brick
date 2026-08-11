#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PID_FILE=${TRIMUI_RMLUI_PID_FILE:-"$APP_DIR/desktop.pid"}
MODE_FILE=${TRIMUI_RMLUI_MODE_FILE:-"$APP_DIR/desktop.mode"}
REQUEST_FILE=${TRIMUI_RMLUI_REQUEST_FILE:-/tmp/rmlui-next}
STATE_FILE=${TRIMUI_RMLUI_STATE_FILE:-/tmp/rmlui-state}
SDCARD_PATH=${TRIMUI_SDCARD_PATH:-/mnt/SDCARD}
ROMS_PATH="$SDCARD_PATH/Roms"
SYSTEM_PATH="$SDCARD_PATH/.system/tg5040"
CORES_PATH="$SYSTEM_PATH/cores"
BIOS_PATH="$SDCARD_PATH/Bios"
SAVES_PATH="$SDCARD_PATH/Saves"
USERDATA_PATH="$SDCARD_PATH/.userdata/tg5040"
SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
LOGS_PATH="$USERDATA_PATH/logs"
export SDCARD_PATH ROMS_PATH SYSTEM_PATH CORES_PATH BIOS_PATH SAVES_PATH
export USERDATA_PATH SHARED_USERDATA_PATH LOGS_PATH
export PLATFORM=tg5040
export DEVICE=brick
export LD_LIBRARY_PATH="$SYSTEM_PATH/lib:/usr/trimui/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$SYSTEM_PATH/bin:/usr/trimui/bin:$PATH"

read_running_pid() {
    if [ -f "$PID_FILE" ]; then
        RUNNING_PID=$(sed -n '1p' "$PID_FILE")
        if [ -n "$RUNNING_PID" ] && kill -0 "$RUNNING_PID" 2>/dev/null; then
            return 0
        fi
    fi

    # Also recognize a process started by an older launcher without a PID file.
    for PROCESS_DIR in /proc/[0-9]*; do
        if [ "$(readlink "$PROCESS_DIR/exe" 2>/dev/null || true)" = "$APP_DIR/trimui-rmlui-prototype" ]; then
            RUNNING_PID=${PROCESS_DIR#/proc/}
            return 0
        fi
    done
    return 1
}

case "${1:-}" in
    stop|--stop)
        if read_running_pid; then
            if [ -f "$MODE_FILE" ] && [ "$(sed -n '1p' "$MODE_FILE")" = emulator ]; then
                echo "A game is running; exit from the MinArch menu instead of terminating it remotely." >&2
                exit 2
            fi
            kill -TERM "$RUNNING_PID"
            echo "Stopping RmlUi desktop (pid $RUNNING_PID)"
        else
            rm -f "$PID_FILE"
            echo "RmlUi desktop is not running"
        fi
        exit 0
        ;;
    status|--status)
        if read_running_pid; then
            echo "RmlUi desktop is running (pid $RUNNING_PID)"
            exit 0
        fi
        rm -f "$PID_FILE"
        echo "RmlUi desktop is not running"
        exit 1
        ;;
esac

if read_running_pid; then
    echo "RmlUi desktop is already running (pid $RUNNING_PID)" >&2
    exit 1
fi

echo "$$" > "$PID_FILE"
CHILD_PID=

cleanup() {
    rm -f "$PID_FILE" "$MODE_FILE"
}

terminate() {
    trap - HUP INT TERM
    if [ -n "$CHILD_PID" ] && kill -0 "$CHILD_PID" 2>/dev/null; then
        kill -TERM "$CHILD_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT
trap terminate HUP INT TERM

mkdir -p "$BIOS_PATH" "$SAVES_PATH" "$USERDATA_PATH" "$SHARED_USERDATA_PATH/.minui" "$LOGS_PATH"

while :; do
    rm -f "$REQUEST_FILE" "$REQUEST_FILE.tmp"
    echo ui > "$MODE_FILE"
    trap terminate HUP INT TERM
    "$APP_DIR/trimui-rmlui-prototype" \
        --assets "$APP_DIR/assets" \
        --font /usr/trimui/res/regular.ttf \
        --renderer opengles2 \
        --rom-root "$ROMS_PATH" \
        --request "$REQUEST_FILE" \
        --state "$STATE_FILE" \
        --fullscreen \
        "$@" &
    CHILD_PID=$!

    set +e
    wait "$CHILD_PID"
    STATUS=$?
    set -e
    CHILD_PID=

    if [ ! -f "$REQUEST_FILE" ]; then
        exit "$STATUS"
    fi

    SYSTEM_ID=
    ROM_PATH=
    {
        IFS= read -r SYSTEM_ID
        IFS= read -r ROM_PATH
    } < "$REQUEST_FILE"
    rm -f "$REQUEST_FILE"

    case "$ROM_PATH" in
        "$ROMS_PATH"/*) ;;
        *)
            echo "Rejected ROM path outside $ROMS_PATH: $ROM_PATH" >&2
            continue
            ;;
    esac

    case "$SYSTEM_ID:$ROM_PATH" in
        GB:*.gb|GB:*.GB|GB:*.zip|GB:*.ZIP)
            CORE_PATH="$CORES_PATH/gambatte_libretro.so"
            ;;
        GBC:*.gbc|GBC:*.GBC|GBC:*.zip|GBC:*.ZIP)
            CORE_PATH="$CORES_PATH/gambatte_libretro.so"
            ;;
        GBA:*.gba|GBA:*.GBA|GBA:*.zip|GBA:*.ZIP)
            CORE_PATH="$CORES_PATH/gpsp_libretro.so"
            ;;
        SFC:*.sfc|SFC:*.SFC|SFC:*.smc|SFC:*.SMC|SFC:*.zip|SFC:*.ZIP)
            CORE_PATH="$CORES_PATH/snes9x2005_plus_libretro.so"
            ;;
        *)
            echo "Rejected unsupported system or ROM extension: $SYSTEM_ID" >&2
            continue
            ;;
    esac

    EMULATOR_LAUNCHER="$SYSTEM_PATH/paks/Emus/$SYSTEM_ID.pak/launch.sh"
    if [ ! -x "$SYSTEM_PATH/bin/minarch.elf" ] || [ ! -f "$CORE_PATH" ] || [ ! -f "$EMULATOR_LAUNCHER" ]; then
        echo "Missing MinArch runtime for $SYSTEM_ID under $SYSTEM_PATH" >&2
        continue
    fi

    echo "Launching $SYSTEM_ID: $ROM_PATH"
    echo emulator > "$MODE_FILE"
	# MinArch needs the system identity because GB and GBC share 160x144 output.
	export MINARCH_SYSTEM="$SYSTEM_ID"
    # MinArch owns device power, audio, input and graphics state while a game is
    # running. It must leave through its own menu; an external SIGTERM can leave
    # the Brick in an unsafe power state.
    trap '' HUP INT TERM
    sh "$EMULATOR_LAUNCHER" "$ROM_PATH" &
    CHILD_PID=$!
    set +e
    wait "$CHILD_PID"
    EMULATOR_STATUS=$?
    set -e
    CHILD_PID=
    trap terminate HUP INT TERM
    echo "$SYSTEM_ID emulator exited with status $EMULATOR_STATUS; returning to library"
done
