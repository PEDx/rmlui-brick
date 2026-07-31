#!/bin/sh
set -eu

DEVICE=${TRIMUI_DEVICE:-root@192.168.31.117}
REMOTE_DIR=${TRIMUI_REMOTE_DIR:-/mnt/UDISK/trimui-dev/rmlui-prototype/current}
ACTION=${1:-start}

case "$ACTION" in
    start|stop|status|logs) ;;
    *)
        echo "Usage: $0 [start|stop|status|logs]" >&2
        exit 2
        ;;
esac

# These values become remote shell arguments. Reject shell metacharacters rather
# than trying to build an unsafe quoted command.
case "$DEVICE" in
    *[!A-Za-z0-9_.@:-]*)
        echo "TRIMUI_DEVICE contains unsupported characters: $DEVICE" >&2
        exit 2
        ;;
esac
case "$REMOTE_DIR" in
    *[!A-Za-z0-9_./-]*)
        echo "TRIMUI_REMOTE_DIR contains unsupported characters: $REMOTE_DIR" >&2
        exit 2
        ;;
esac

ssh "$DEVICE" sh -s -- "$ACTION" "$REMOTE_DIR" <<'REMOTE_SCRIPT'
set -eu

action=$1
app_dir=$2
launcher="$app_dir/launch.sh"
log_file="$(dirname "$app_dir")/latest.log"

if [ ! -x "$launcher" ]; then
    echo "Launcher is missing or not executable: $launcher" >&2
    exit 1
fi

case "$action" in
    start)
        if "$launcher" status >/dev/null 2>&1; then
            "$launcher" status
            exit 0
        fi

        # runtrimui.sh owns the display lifecycle. It runs this hand-off script
        # after MainUI exits, then restores MainUI when the prototype finishes.
        case "$launcher" in
            *"'"*|*"
"*)
                echo "Launcher path cannot be represented safely" >&2
                exit 1
                ;;
        esac
        printf '#!/bin/sh\nexec '\''%s'\'' > '\''%s'\'' 2>&1\n' "$launcher" "$log_file" > /tmp/cmd_to_run.sh
        chmod 755 /tmp/cmd_to_run.sh

        mainui_pid=$(pidof MainUI 2>/dev/null || true)
        if [ -z "$mainui_pid" ]; then
            echo "MainUI is not running; refusing to compete for the display" >&2
            rm -f /tmp/cmd_to_run.sh
            exit 1
        fi

        kill -9 $mainui_pid
        echo "Start requested. Press B to exit; use './scripts/run-brick.sh stop' for emergency exit."
        ;;
    stop)
        "$launcher" stop
        ;;
    status)
        "$launcher" status
        ;;
    logs)
        if [ -f "$log_file" ]; then
            tail -n 100 "$log_file"
        else
            echo "No log file yet: $log_file" >&2
            exit 1
        fi
        ;;
esac
REMOTE_SCRIPT
