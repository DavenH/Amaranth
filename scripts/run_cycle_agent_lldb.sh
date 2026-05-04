#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
SCRIPT_PATH="${1:-${CYCLE_AGENT_SCRIPT:-}}"
LLDB_COMMAND_PATH="${2:-${CYCLE_LLDB_COMMANDS:-}}"
REPORT_PATH="${3:-${CYCLE_AGENT_REPORT:-/tmp/cycle-agent-lldb-report.json}}"
LOG_PATH="${4:-${CYCLE_LOG_PATH:-/tmp/cycle-agent-lldb-app.log}}"
LLDB_LOG_PATH="${5:-${CYCLE_LLDB_LOG_PATH:-/tmp/cycle-agent-lldb.log}}"
RAW_LOG_PATH="${CYCLE_RAW_LOG_PATH:-$LOG_PATH.raw}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
REUSE_EXISTING="${CYCLE_REUSE_EXISTING:-0}"
QUIT_WAIT_SECONDS="${CYCLE_QUIT_WAIT_SECONDS:-3}"
LAUNCH_DELAY_SECONDS="${CYCLE_LLDB_LAUNCH_DELAY_SECONDS:-0.5}"
AUTO_CONTINUE="${CYCLE_LLDB_AUTO_CONTINUE:-1}"
RECORD_SESSION="${CYCLE_LLDB_RECORD_SESSION:-1}"
SUPPRESS_CRASH_DIALOG="${CYCLE_SUPPRESS_CRASH_DIALOG:-1}"
CRASH_REPORTER_DOMAIN="com.apple.CrashReporter"
CRASH_REPORTER_ORIGINAL_DIALOG_TYPE=""
CRASH_REPORTER_HAD_DIALOG_TYPE=0

if [[ -z "$SCRIPT_PATH" ]]; then
    echo "Usage: scripts/run_cycle_agent_lldb.sh <script.json> [lldb-commands.txt|-] [report.json] [app-log.txt] [lldb-log.txt]" >&2
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

if ! command -v lldb >/dev/null 2>&1; then
    echo "lldb was not found on PATH." >&2
    exit 1
fi

SCRIPT_PATH="${SCRIPT_PATH:A}"
REPORT_PATH="${REPORT_PATH:A}"
LOG_PATH="${LOG_PATH:A}"
LLDB_LOG_PATH="${LLDB_LOG_PATH:A}"
RAW_LOG_PATH="${RAW_LOG_PATH:A}"

if [[ -n "$LLDB_COMMAND_PATH" && "$LLDB_COMMAND_PATH" != "-" ]]; then
    if [[ ! -f "$LLDB_COMMAND_PATH" ]]; then
        echo "LLDB command file not found: $LLDB_COMMAND_PATH" >&2
        exit 1
    fi

    LLDB_COMMAND_PATH="${LLDB_COMMAND_PATH:A}"
else
    LLDB_COMMAND_PATH=""
fi

mkdir -p "$(dirname "$REPORT_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
mkdir -p "$(dirname "$LLDB_LOG_PATH")"
mkdir -p "$(dirname "$RAW_LOG_PATH")"

rm -f "$REPORT_PATH"
: > "$LOG_PATH"
: > "$RAW_LOG_PATH"
: > "$LLDB_LOG_PATH"

restore_crash_reporter_dialog_type() {
    [[ "$SUPPRESS_CRASH_DIALOG" == "1" ]] || return 0

    if [[ "$CRASH_REPORTER_HAD_DIALOG_TYPE" == "1" ]]; then
        defaults write "$CRASH_REPORTER_DOMAIN" DialogType "$CRASH_REPORTER_ORIGINAL_DIALOG_TYPE" >/dev/null 2>&1 || true
    else
        defaults delete "$CRASH_REPORTER_DOMAIN" DialogType >/dev/null 2>&1 || true
    fi
}

suppress_crash_reporter_dialog_type() {
    [[ "$SUPPRESS_CRASH_DIALOG" == "1" ]] || return 0

    if CRASH_REPORTER_ORIGINAL_DIALOG_TYPE="$(defaults read "$CRASH_REPORTER_DOMAIN" DialogType 2>/dev/null)"; then
        CRASH_REPORTER_HAD_DIALOG_TYPE=1
    fi

    defaults write "$CRASH_REPORTER_DOMAIN" DialogType none >/dev/null 2>&1 || true
}

process_exists() {
    osascript -e "tell application \"System Events\" to exists process \"$PROCESS_NAME\"" 2>/dev/null | grep -q true
}

quit_process() {
    osascript -e "tell application \"$PROCESS_NAME\" to quit" || true

    local quit_deadline=$((SECONDS + QUIT_WAIT_SECONDS))
    while (( SECONDS < quit_deadline )); do
        if ! process_exists; then
            break
        fi
        sleep 0.25
    done
}

DRIVER_PATH="$(mktemp "${TMPDIR:-/tmp}/cycle-agent-lldb.XXXXXX")"
cleanup() {
    restore_crash_reporter_dialog_type
    rm -f "$DRIVER_PATH"
}
trap cleanup EXIT

{
    echo "process attach --name \"$PROCESS_NAME\" --waitfor"
    echo "process handle SIGTRAP --stop true --notify true --pass true"
    echo "process handle SIGSEGV --stop true --notify true --pass true"
    echo "process handle SIGBUS --stop true --notify true --pass true"
    echo "process handle SIGABRT --stop true --notify true --pass true"

    if [[ -n "$LLDB_COMMAND_PATH" ]]; then
        echo "command source \"$LLDB_COMMAND_PATH\""
    fi

    if [[ "$AUTO_CONTINUE" == "1" ]]; then
        echo "continue"
    fi
} > "$DRIVER_PATH"

if [[ "$REUSE_EXISTING" != "1" ]] && process_exists; then
    quit_process
fi

suppress_crash_reporter_dialog_type

echo "LLDB driver: $DRIVER_PATH"
echo "App log: $LOG_PATH"
echo "Raw app log: $RAW_LOG_PATH"
echo "Automation report: $REPORT_PATH"
echo "LLDB transcript: $LLDB_LOG_PATH"

(
    sleep "$LAUNCH_DELAY_SECONDS"
    open -n \
        --stdout "$RAW_LOG_PATH" \
        --stderr "$RAW_LOG_PATH" \
        "$APP_PATH" \
        --args \
        --agent-script "$SCRIPT_PATH" \
        --agent-report "$REPORT_PATH"
) &

if [[ "$RECORD_SESSION" == "1" && -t 0 && -t 1 && "$(command -v script || true)" != "" ]]; then
    script -q "$LLDB_LOG_PATH" lldb -s "$DRIVER_PATH"
else
    lldb -s "$DRIVER_PATH" 2>&1 | tee "$LLDB_LOG_PATH"
fi

if [[ -f "$RAW_LOG_PATH" ]]; then
    cp "$RAW_LOG_PATH" "$LOG_PATH"
fi

echo "$REPORT_PATH"
echo "$LOG_PATH"
echo "$RAW_LOG_PATH"
echo "$LLDB_LOG_PATH"
