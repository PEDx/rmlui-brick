#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PID_FILE=${TRIMUI_RMLUI_PID_FILE:-"$APP_DIR/desktop.pid"}
export LD_LIBRARY_PATH="/usr/trimui/lib:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

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
    rm -f "$PID_FILE"
}

terminate() {
    trap - HUP INT TERM
    if [ -n "$CHILD_PID" ] && kill -0 "$CHILD_PID" 2>/dev/null; then
        kill -TERM "$CHILD_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT
trap terminate HUP INT TERM

"$APP_DIR/trimui-rmlui-prototype" \
    --assets "$APP_DIR/assets" \
    --font /usr/trimui/res/regular.ttf \
    --renderer opengles2 \
    --fullscreen \
    "$@" &
CHILD_PID=$!

set +e
wait "$CHILD_PID"
STATUS=$?
set -e
exit "$STATUS"
