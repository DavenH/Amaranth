#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
SOCKET_PATH="${1:-${CYCLE_AGENT_SESSION:-/tmp/cycle-agent.sock}}"
LOG_PATH="${2:-${CYCLE_LOG_PATH:-/tmp/cycle-agent-session.log}}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-20}"

if [[ ! -d "$APP_PATH" ]]; then
    echo "Cycle app not found: $APP_PATH" >&2
    exit 1
fi

SOCKET_PATH="${SOCKET_PATH:A}"
LOG_PATH="${LOG_PATH:A}"

mkdir -p "$(dirname "$SOCKET_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
rm -f "$SOCKET_PATH"
: > "$LOG_PATH"

open -n "$APP_PATH" --args --agent-session "$SOCKET_PATH" >> "$LOG_PATH" 2>&1

deadline=$((SECONDS + WAIT_SECONDS))
while (( SECONDS < deadline )); do
    if [[ -S "$SOCKET_PATH" ]]; then
        echo "$SOCKET_PATH"
        echo "$LOG_PATH"
        exit 0
    fi

    sleep 0.25
done

echo "Cycle session socket was not created: $SOCKET_PATH" >&2
exit 1
