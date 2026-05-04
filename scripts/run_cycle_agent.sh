#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
SCRIPT_PATH="${1:-${CYCLE_AGENT_SCRIPT:-}}"
REPORT_PATH="${2:-${CYCLE_AGENT_REPORT:-/tmp/cycle-agent-report.json}}"
LOG_PATH="${3:-${CYCLE_LOG_PATH:-/tmp/cycle-agent-logs.txt}}"
RAW_LOG_PATH="${CYCLE_RAW_LOG_PATH:-$LOG_PATH.raw}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-20}"
FOCUS_SECONDS="${CYCLE_FOCUS_SECONDS:-2}"
QUIT_WAIT_SECONDS="${CYCLE_QUIT_WAIT_SECONDS:-3}"
REUSE_EXISTING="${CYCLE_REUSE_EXISTING:-0}"
FILTER_LOGS="${CYCLE_FILTER_LOGS:-1}"
ALLOW_FAILURES="${CYCLE_AGENT_ALLOW_FAILURES:-0}"
PREFLIGHT_PERMISSIONS="${CYCLE_PREFLIGHT_PERMISSIONS:-1}"
PREFLIGHT_SCREEN_CAPTURE="${CYCLE_PREFLIGHT_SCREEN_CAPTURE:-0}"
OS_SCREENSHOT_AREA="${CYCLE_OS_SCREENSHOT_AREA:-}"
OS_SCREENSHOT_TARGET="${CYCLE_OS_SCREENSHOT_TARGET:-}"
OS_SCREENSHOT_PATH="${CYCLE_OS_SCREENSHOT_PATH:-}"
OS_SCREENSHOT_QUIT_AFTER="${CYCLE_OS_SCREENSHOT_QUIT_AFTER:-1}"

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

if [[ -n "$OS_SCREENSHOT_PATH" ]]; then
    OS_SCREENSHOT_PATH="${OS_SCREENSHOT_PATH:A}"
fi

mkdir -p "$(dirname "$REPORT_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
mkdir -p "$(dirname "$RAW_LOG_PATH")"

rm -f "$REPORT_PATH"
: > "$LOG_PATH"
: > "$RAW_LOG_PATH"

open_privacy_pane() {
    local pane="$1"
    open "x-apple.systempreferences:com.apple.preference.security?$pane" >/dev/null 2>&1 || true
}

preflight_accessibility_permission() {
    if osascript -e 'tell application "System Events" to get name of first process' >/dev/null 2>&1; then
        return 0
    fi

    echo "Cycle agent focus control requires macOS Accessibility permission for the calling terminal/app." >&2
    echo "Grant permission in System Settings > Privacy & Security > Accessibility, then rerun this script." >&2
    open_privacy_pane "Privacy_Accessibility"
    return 1
}

preflight_screen_capture_permission() {
    local probe_path="${TMPDIR:-/tmp}/cycle-agent-screen-permission-probe.png"
    rm -f "$probe_path"

    if screencapture -x "$probe_path" >/dev/null 2>&1 && [[ -s "$probe_path" ]]; then
        rm -f "$probe_path"
        return 0
    fi

    echo "Cycle agent OS screenshots require macOS Screen Recording permission for the calling terminal/app." >&2
    echo "Grant permission in System Settings > Privacy & Security > Screen Recording, then rerun this script." >&2
    open_privacy_pane "Privacy_ScreenCapture"
    return 1
}

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

capture_os_screenshot() {
    local app_bundle_id="$1"

    if [[ -z "$OS_SCREENSHOT_AREA" || -z "$OS_SCREENSHOT_PATH" ]]; then
        echo "OS screenshot capture requires CYCLE_OS_SCREENSHOT_AREA and CYCLE_OS_SCREENSHOT_PATH." >&2
        return 1
    fi

    mkdir -p "$(dirname "$OS_SCREENSHOT_PATH")"
    focus_process "$app_bundle_id"

    if ! process_frontmost; then
        echo "Cycle did not become frontmost within ${FOCUS_SECONDS}s for OS screenshot capture." >&2
        return 1
    fi

    local rect
    rect="$(jq -er \
        --arg area "$OS_SCREENSHOT_AREA" \
        --arg target "$OS_SCREENSHOT_TARGET" \
        '
            [
                .results[]
                | select(.type == "inspectTargets" and .ok == true)
                | .data.targets[]
                | select(.area == $area and .target == $target and .resolved == true)
                | .screenBounds
            ][0]
            | "\(.x),\(.y),\(.width),\(.height)"
        ' "$REPORT_PATH")"

    if [[ -z "$rect" ]]; then
        echo "Could not find screenBounds for area '$OS_SCREENSHOT_AREA' target '$OS_SCREENSHOT_TARGET' in $REPORT_PATH." >&2
        return 1
    fi

    screencapture -x -R "$rect" "$OS_SCREENSHOT_PATH"

    if [[ ! -s "$OS_SCREENSHOT_PATH" ]]; then
        echo "OS screenshot was not written: $OS_SCREENSHOT_PATH" >&2
        return 1
    fi
}

if [[ "$PREFLIGHT_PERMISSIONS" == "1" ]]; then
    preflight_accessibility_permission

    if [[ "$PREFLIGHT_SCREEN_CAPTURE" == "1" || -n "$OS_SCREENSHOT_AREA" || -n "$OS_SCREENSHOT_PATH" ]]; then
        preflight_screen_capture_permission
    fi
fi

if [[ "$REUSE_EXISTING" != "1" ]] && process_exists; then
    quit_process
fi

open -n \
    --stdout "$RAW_LOG_PATH" \
    --stderr "$RAW_LOG_PATH" \
    "$APP_PATH" \
    --args \
    --agent-script "$SCRIPT_PATH" \
    --agent-report "$REPORT_PATH"

APP_BUNDLE_ID="$(plutil -extract CFBundleIdentifier raw -o - "$APP_PATH/Contents/Info.plist")"
focus_process "$APP_BUNDLE_ID" || true

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

if [[ -n "$OS_SCREENSHOT_AREA" || -n "$OS_SCREENSHOT_PATH" ]]; then
    capture_os_screenshot "$APP_BUNDLE_ID"

    if [[ "$OS_SCREENSHOT_QUIT_AFTER" == "1" ]]; then
        quit_process
    fi
fi

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

echo "$REPORT_PATH"
echo "$LOG_PATH"
echo "$RAW_LOG_PATH"

if [[ -n "$OS_SCREENSHOT_PATH" ]]; then
    echo "$OS_SCREENSHOT_PATH"
fi
