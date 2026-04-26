#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
OUT_PATH="${1:-/tmp/cycle-ui.png}"
LOG_PATH="${2:-${CYCLE_LOG_PATH:-/tmp/cycle-logs.txt}}"
RAW_LOG_PATH="${CYCLE_RAW_LOG_PATH:-$LOG_PATH.raw}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-8}"
FOCUS_SECONDS="${CYCLE_FOCUS_SECONDS:-2}"
QUIT_AFTER_CAPTURE="${CYCLE_QUIT_AFTER_CAPTURE:-1}"
QUIT_WAIT_SECONDS="${CYCLE_QUIT_WAIT_SECONDS:-3}"
FILTER_LOGS="${CYCLE_FILTER_LOGS:-1}"

if [[ ! -d "$APP_PATH" ]]; then
    echo "Cycle app not found: $APP_PATH" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
mkdir -p "$(dirname "$RAW_LOG_PATH")"

: > "$LOG_PATH"
: > "$RAW_LOG_PATH"

open -n -o "$RAW_LOG_PATH" --stderr "$RAW_LOG_PATH" "$APP_PATH"

deadline=$((SECONDS + WAIT_SECONDS))
while (( SECONDS < deadline )); do
    if osascript -e "tell application \"System Events\" to exists process \"$PROCESS_NAME\"" | grep -q true; then
        break
    fi
    sleep 0.25
done

if ! osascript -e "tell application \"System Events\" to exists process \"$PROCESS_NAME\"" | grep -q true; then
    echo "Cycle did not appear within ${WAIT_SECONDS}s" >&2
    exit 1
fi

osascript -e "tell application \"$PROCESS_NAME\" to activate"
sleep "$FOCUS_SECONDS"

screencapture -x "$OUT_PATH"

if [[ "$QUIT_AFTER_CAPTURE" == "1" ]]; then
    osascript -e "tell application \"$PROCESS_NAME\" to quit" || true

    quit_deadline=$((SECONDS + QUIT_WAIT_SECONDS))
    while (( SECONDS < quit_deadline )); do
        if ! osascript -e "tell application \"System Events\" to exists process \"$PROCESS_NAME\"" | grep -q true; then
            break
        fi
        sleep 0.25
    done
fi

if [[ "$FILTER_LOGS" == "1" ]]; then
    awk '
        function keep(line) {
            return line ~ /(JUCE v|AppClass::|MainAppWindow|FileManager::|Directories::|Document::open|Update:|Error|error|Warning|warning|fail|Fail|assert|Assert|exception|Exception|crash|Crash)/
        }

        /^Error Domain=/ {
            in_error = 1
            print
            next
        }

        in_error {
            if ($0 !~ /^[[:space:]")]/) {
                in_error = 0
            } else {
                print
                if ($0 ~ /\}\}$/) {
                    in_error = 0
                }
                next
            }
        }

        keep($0) {
            print
        }
    ' "$RAW_LOG_PATH" > "$LOG_PATH"
else
    cp "$RAW_LOG_PATH" "$LOG_PATH"
fi

echo "$OUT_PATH"
echo "$LOG_PATH"
echo "$RAW_LOG_PATH"
