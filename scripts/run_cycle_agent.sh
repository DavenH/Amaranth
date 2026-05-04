#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
SCRIPT_PATH="${1:-${CYCLE_AGENT_SCRIPT:-}}"
REPORT_PATH="${2:-${CYCLE_AGENT_REPORT:-/tmp/cycle-agent-report.json}}"
LOG_PATH="${3:-${CYCLE_LOG_PATH:-/tmp/cycle-agent-logs.txt}}"
RAW_LOG_PATH="${CYCLE_RAW_LOG_PATH:-$LOG_PATH.raw}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-20}"
QUIT_WAIT_SECONDS="${CYCLE_QUIT_WAIT_SECONDS:-3}"
REUSE_EXISTING="${CYCLE_REUSE_EXISTING:-0}"
FILTER_LOGS="${CYCLE_FILTER_LOGS:-1}"
ALLOW_FAILURES="${CYCLE_AGENT_ALLOW_FAILURES:-0}"

if [[ -z "$SCRIPT_PATH" ]]; then
    echo "Usage: scripts/run_cycle_agent.sh <script.json> [report.json] [log.txt]" >&2
    exit 2
fi

if [[ ! -d "$APP_PATH" ]]; then
    echo "Cycle app not found: $APP_PATH" >&2
    exit 1
fi

if [[ ! -f "$SCRIPT_PATH" ]]; then
    echo "Automation script not found: $SCRIPT_PATH" >&2
    exit 1
fi

SCRIPT_PATH="${SCRIPT_PATH:A}"
REPORT_PATH="${REPORT_PATH:A}"
LOG_PATH="${LOG_PATH:A}"
RAW_LOG_PATH="${RAW_LOG_PATH:A}"

mkdir -p "$(dirname "$REPORT_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
mkdir -p "$(dirname "$RAW_LOG_PATH")"

rm -f "$REPORT_PATH"
: > "$LOG_PATH"
: > "$RAW_LOG_PATH"

process_exists() {
    osascript -e "tell application \"System Events\" to exists process \"$PROCESS_NAME\"" | grep -q true
}

if [[ "$REUSE_EXISTING" != "1" ]] && process_exists; then
    osascript -e "tell application \"$PROCESS_NAME\" to quit" || true

    quit_deadline=$((SECONDS + QUIT_WAIT_SECONDS))
    while (( SECONDS < quit_deadline )); do
        if ! process_exists; then
            break
        fi
        sleep 0.25
    done
fi

open -n \
    --stdout "$RAW_LOG_PATH" \
    --stderr "$RAW_LOG_PATH" \
    "$APP_PATH" \
    --args \
    --agent-script "$SCRIPT_PATH" \
    --agent-report "$REPORT_PATH"

deadline=$((SECONDS + WAIT_SECONDS))
while (( SECONDS < deadline )); do
    if [[ -s "$REPORT_PATH" ]]; then
        break
    fi

    if ! process_exists; then
        sleep 0.5
        break
    fi

    sleep 0.25
done

if [[ "$FILTER_LOGS" == "1" ]]; then
    awk '
        function keep(line) {
            return line ~ /(JUCE v|AppClass::|MainAppWindow|CycleAutomation|FileManager::|Directories::|Document::open|Update:|Error|error|Warning|warning|fail|Fail|assert|Assert|exception|Exception|crash|Crash)/
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

if [[ ! -s "$REPORT_PATH" ]]; then
    echo "Cycle agent report was not written within ${WAIT_SECONDS}s: $REPORT_PATH" >&2
    echo "$LOG_PATH" >&2
    echo "$RAW_LOG_PATH" >&2
    exit 1
fi

if [[ "$ALLOW_FAILURES" != "1" ]] && grep -q '"ok": false' "$REPORT_PATH"; then
    echo "Cycle agent report contains failed commands: $REPORT_PATH" >&2
    echo "$LOG_PATH" >&2
    echo "$RAW_LOG_PATH" >&2
    exit 1
fi

echo "$REPORT_PATH"
echo "$LOG_PATH"
echo "$RAW_LOG_PATH"
