#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
APP_ARGS="${CYCLE_APP_ARGS:-}"
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
OS_SCREENSHOT_PARK_MOUSE_POINT="${CYCLE_OS_SCREENSHOT_PARK_MOUSE_POINT:-}"
OS_SCREENSHOT_PARK_MOUSE_SETTLE_SECONDS="${CYCLE_OS_SCREENSHOT_PARK_MOUSE_SETTLE_SECONDS:-0.2}"
OS_SCREENSHOT_QUIT_AFTER="${CYCLE_OS_SCREENSHOT_QUIT_AFTER:-1}"
CAPTURE_CRASH_REPORTS="${CYCLE_CAPTURE_CRASH_REPORTS:-1}"
DISMISS_CRASH_DIALOG="${CYCLE_DISMISS_CRASH_DIALOG:-1}"
SUPPRESS_CRASH_DIALOG="${CYCLE_SUPPRESS_CRASH_DIALOG:-1}"
CRASH_REPORT_WAIT_SECONDS="${CYCLE_CRASH_REPORT_WAIT_SECONDS:-5}"
CRASH_REPORT_PATH="${CYCLE_CRASH_REPORT_PATH:-$LOG_PATH.ips}"
CRASH_REPORTER_DOMAIN="com.apple.CrashReporter"
CRASH_REPORTER_ORIGINAL_DIALOG_TYPE=""
CRASH_REPORTER_HAD_DIALOG_TYPE=0

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
CRASH_REPORT_PATH="${CRASH_REPORT_PATH:A}"

if [[ -n "$OS_SCREENSHOT_PATH" ]]; then
    OS_SCREENSHOT_PATH="${OS_SCREENSHOT_PATH:A}"
fi

mkdir -p "$(dirname "$REPORT_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
mkdir -p "$(dirname "$RAW_LOG_PATH")"
mkdir -p "$(dirname "$CRASH_REPORT_PATH")"

rm -f "$REPORT_PATH"
: > "$LOG_PATH"
: > "$RAW_LOG_PATH"
rm -f "$CRASH_REPORT_PATH"

restore_crash_reporter_dialog_type() {
    [[ "$SUPPRESS_CRASH_DIALOG" == "1" && "$DISMISS_CRASH_DIALOG" == "1" ]] || return 0

    if [[ "$CRASH_REPORTER_HAD_DIALOG_TYPE" == "1" ]]; then
        defaults write "$CRASH_REPORTER_DOMAIN" DialogType "$CRASH_REPORTER_ORIGINAL_DIALOG_TYPE" >/dev/null 2>&1 || true
    else
        defaults delete "$CRASH_REPORTER_DOMAIN" DialogType >/dev/null 2>&1 || true
    fi
}

trap restore_crash_reporter_dialog_type EXIT

suppress_crash_reporter_dialog_type() {
    [[ "$SUPPRESS_CRASH_DIALOG" == "1" && "$DISMISS_CRASH_DIALOG" == "1" ]] || return 0

    if CRASH_REPORTER_ORIGINAL_DIALOG_TYPE="$(defaults read "$CRASH_REPORTER_DOMAIN" DialogType 2>/dev/null)"; then
        CRASH_REPORTER_HAD_DIALOG_TYPE=1
    fi

    defaults write "$CRASH_REPORTER_DOMAIN" DialogType none >/dev/null 2>&1 || true
}

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

dismiss_crash_dialog() {
    [[ "$DISMISS_CRASH_DIALOG" == "1" ]] || return 1

    osascript - "$PROCESS_NAME" <<'APPLESCRIPT' 2>/dev/null
on run argv
    set appName to item 1 of argv

    set crashPhrases to {appName & " quit unexpectedly", appName & " unexpectedly", "quit unexpectedly", "crashed"}
    set buttonNames to {"Ignore", "OK", "Close", "Cancel", "Done", "Don't Reopen", "Do Not Reopen", "Report...", "Report"}

    tell application "System Events"
        repeat with proc in application processes
            repeat with win in windows of proc
                set winName to ""
                try
                    set winName to name of win as text
                end try

                set combinedText to winName
                try
                    repeat with txt in static texts of win
                        set combinedText to combinedText & " " & (value of txt as text)
                    end repeat
                end try

                repeat with phrase in crashPhrases
                    if combinedText contains (phrase as text) then
                        repeat with buttonName in buttonNames
                            try
                                if exists button (buttonName as text) of win then
                                    click button (buttonName as text) of win
                                    return "Dismissed crash dialog: " & winName
                                end if
                            end try
                        end repeat

                        try
                            click button 1 of win
                            return "Dismissed crash dialog with first button: " & winName
                        end try
                    end if
                end repeat
            end repeat
        end repeat
    end tell

    tell application "System Events"
        repeat with proc in application processes
            set procName to name of proc as text

            if procName is "Problem Reporter" or procName is "ReportCrash" or procName contains "Problem" or procName contains "Crash" then
                repeat with win in windows of proc
                    set winName to name of win as text

                    if winName contains appName or winName contains "quit unexpectedly" then
                        click button 1 of win
                        return "Dismissed crash dialog: " & winName
                    end if
                end repeat
            end if
        end repeat
    end tell

    return ""
end run
APPLESCRIPT
}

copy_recent_crash_report() {
    [[ "$CAPTURE_CRASH_REPORTS" == "1" ]] || return 1

    local launch_time="$1"
    local deadline=$((SECONDS + CRASH_REPORT_WAIT_SECONDS))
    local newest=""

    while (( SECONDS <= deadline )); do
        for report in "$HOME/Library/Logs/DiagnosticReports"/${PROCESS_NAME}-*.ips(N.om); do
            local modified
            modified="$(stat -f %m "$report" 2>/dev/null || echo 0)"
            modified="${modified##*=}"

            if (( modified + 5 >= launch_time )); then
                newest="$report"
                break
            fi
        done

        if [[ -n "$newest" ]]; then
            cp "$newest" "$CRASH_REPORT_PATH"

            {
                echo
                echo "Cycle crash report copied: $CRASH_REPORT_PATH"
                echo "Cycle crash report source: $newest"
                echo
                sed -n '1,220p' "$CRASH_REPORT_PATH"
            } >> "$RAW_LOG_PATH"

            return 0
        fi

        sleep 0.5
    done

    return 1
}

finalize_logs() {
    if [[ "$FILTER_LOGS" == "1" ]]; then
        awk '
            function keep(line) {
                return line ~ /(JUCE v|AppClass::|MainAppWindow|PlaybackPanel::|CycleAutomation|FileManager::|Directories::|Document::open|Update:|Error|error|Warning|warning|fail|Fail|assert|Assert|exception|Exception|crash|Crash|quit unexpectedly|DiagnosticReports|ips)/
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

    if [[ -n "$OS_SCREENSHOT_PARK_MOUSE_POINT" ]]; then
        python3 - "$OS_SCREENSHOT_PARK_MOUSE_POINT" <<'PY'
import ctypes
import sys

point = sys.argv[1].split(",", 1)
x = float(point[0])
y = float(point[1])

core_graphics = ctypes.CDLL(
        "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")

class CGPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double)]

core_graphics.CGWarpMouseCursorPosition.argtypes = [CGPoint]
core_graphics.CGWarpMouseCursorPosition.restype = ctypes.c_int
core_graphics.CGAssociateMouseAndMouseCursorPosition.argtypes = [ctypes.c_bool]
core_graphics.CGAssociateMouseAndMouseCursorPosition.restype = ctypes.c_int

core_graphics.CGWarpMouseCursorPosition(CGPoint(x, y))
core_graphics.CGAssociateMouseAndMouseCursorPosition(True)
PY
        sleep "$OS_SCREENSHOT_PARK_MOUSE_SETTLE_SECONDS"
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

suppress_crash_reporter_dialog_type

if [[ "$REUSE_EXISTING" != "1" ]] && process_exists; then
    quit_process
fi

LAUNCH_TIME="$(date +%s)"
APP_BUNDLE_ID="$(plutil -extract CFBundleIdentifier raw -o - "$APP_PATH/Contents/Info.plist")"
APP_EXECUTABLE_NAME="$(plutil -extract CFBundleExecutable raw -o - "$APP_PATH/Contents/Info.plist")"

launch_args=(
    ${(z)APP_ARGS}
    --agent-script "$SCRIPT_PATH"
    --agent-report "$REPORT_PATH"
)

if ! open -n \
        --stdout "$RAW_LOG_PATH" \
        --stderr "$RAW_LOG_PATH" \
        "$APP_PATH" \
        --args \
        "${launch_args[@]}"; then
    app_executable="$APP_PATH/Contents/MacOS/$APP_EXECUTABLE_NAME"

    if [[ ! -x "$app_executable" ]]; then
        echo "Cycle executable not found: $app_executable" >&2
        exit 1
    fi

    echo "open failed; launching Cycle executable directly: $app_executable" >> "$RAW_LOG_PATH"
    "$app_executable" "${launch_args[@]}" >> "$RAW_LOG_PATH" 2>&1 &
fi

focus_process "$APP_BUNDLE_ID" || true

deadline=$((SECONDS + WAIT_SECONDS))
while (( SECONDS < deadline )); do
    if [[ -s "$REPORT_PATH" ]]; then
        break
    fi

    dialog_message="$(dismiss_crash_dialog || true)"

    if [[ -n "$dialog_message" ]]; then
        echo "$dialog_message" >> "$RAW_LOG_PATH"
        copy_recent_crash_report "$LAUNCH_TIME" || true
        break
    fi

    if ! process_exists; then
        sleep 0.5
        if [[ -s "$REPORT_PATH" ]]; then
            break
        fi
        copy_recent_crash_report "$LAUNCH_TIME" || true
        break
    fi

    sleep 0.25
done

if [[ ! -s "$REPORT_PATH" ]]; then
    finalize_logs
    echo "Cycle agent report was not written within ${WAIT_SECONDS}s: $REPORT_PATH" >&2
    echo "$LOG_PATH" >&2
    echo "$RAW_LOG_PATH" >&2
    if [[ -s "$CRASH_REPORT_PATH" ]]; then
        echo "$CRASH_REPORT_PATH" >&2
    fi
    exit 1
fi

if [[ "$ALLOW_FAILURES" != "1" ]] && grep -q '"ok": false' "$REPORT_PATH"; then
    finalize_logs
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

finalize_logs

echo "$REPORT_PATH"
echo "$LOG_PATH"
echo "$RAW_LOG_PATH"

if [[ -n "$OS_SCREENSHOT_PATH" ]]; then
    echo "$OS_SCREENSHOT_PATH"
fi

if [[ -s "$CRASH_REPORT_PATH" ]]; then
    echo "$CRASH_REPORT_PATH"
fi
