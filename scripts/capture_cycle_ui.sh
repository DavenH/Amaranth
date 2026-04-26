#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
OUT_PATH="${1:-/tmp/cycle-ui.png}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-8}"
FOCUS_SECONDS="${CYCLE_FOCUS_SECONDS:-2}"
QUIT_AFTER_CAPTURE="${CYCLE_QUIT_AFTER_CAPTURE:-1}"

if [[ ! -d "$APP_PATH" ]]; then
    echo "Cycle app not found: $APP_PATH" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT_PATH")"

open -n "$APP_PATH"

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
fi

echo "$OUT_PATH"
