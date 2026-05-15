#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
REPO_ROOT="${SCRIPT_DIR:h}"
APP_PATH="${CYCLE_APP_PATH:-$REPO_ROOT/build/standalone-debug/cycle/Cycle.app}"
SOCKET_PATH="${1:-${CYCLE_AGENT_SESSION:-/tmp/cycle-agent.sock}}"
LOG_PATH="${2:-${CYCLE_LOG_PATH:-/tmp/cycle-agent-session.log}}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-20}"
APPEND_LOG="${CYCLE_SESSION_APPEND_LOG:-0}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
FOCUS_SECONDS="${CYCLE_FOCUS_SECONDS:-2}"

if [[ ! -d "$APP_PATH" ]]; then
    echo "Cycle app not found: $APP_PATH" >&2
    exit 1
fi

SOCKET_PATH="${SOCKET_PATH:A}"
LOG_PATH="${LOG_PATH:A}"

mkdir -p "$(dirname "$SOCKET_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
rm -f "$SOCKET_PATH"

if [[ "$APPEND_LOG" != "1" ]]; then
    : > "$LOG_PATH"
fi

process_exists() {
    osascript -e "tell application \"System Events\" to exists process \"$PROCESS_NAME\"" 2>/dev/null | grep -q true
}

process_frontmost() {
    osascript -e "tell application \"System Events\" to frontmost of process \"$PROCESS_NAME\"" 2>/dev/null | grep -q true
}

focus_process() {
    local app_bundle_id="$1"
    local focus_deadline=$((SECONDS + FOCUS_SECONDS))

    while (( SECONDS < focus_deadline )); do
        if process_exists; then
            osascript -e "tell application id \"$app_bundle_id\" to activate" \
                || osascript -e "tell application \"$PROCESS_NAME\" to activate" \
                || true
            osascript -e "tell application \"System Events\" to set frontmost of process \"$PROCESS_NAME\" to true" || true
        fi

        if process_frontmost; then
            return 0
        fi

        sleep 0.25
    done

    process_frontmost
}

open -n \
    --stdout "$LOG_PATH" \
    --stderr "$LOG_PATH" \
    "$APP_PATH" \
    --args \
    --agent-session "$SOCKET_PATH"

APP_BUNDLE_ID="$(plutil -extract CFBundleIdentifier raw -o - "$APP_PATH/Contents/Info.plist")"

deadline=$((SECONDS + WAIT_SECONDS))
while (( SECONDS < deadline )); do
    if [[ -S "$SOCKET_PATH" ]]; then
        focus_process "$APP_BUNDLE_ID" || true
        echo "$SOCKET_PATH"
        echo "$LOG_PATH"
        exit 0
    fi

    sleep 0.25
done

echo "Cycle session socket was not created: $SOCKET_PATH" >&2
exit 1
