#!/bin/zsh
set -euo pipefail

APP_PATH="${CYCLE_APP_PATH:-/Users/daven/repos/Amaranth/build/standalone-debug/cycle/Cycle.app}"
OUT_PATH="${1:-/tmp/cycle-ui.png}"
LOG_PATH="${2:-${CYCLE_LOG_PATH:-/tmp/cycle-logs.txt}}"
RAW_LOG_PATH="${CYCLE_RAW_LOG_PATH:-$LOG_PATH.raw}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
WAIT_SECONDS="${CYCLE_WAIT_SECONDS:-8}"
FOCUS_SECONDS="${CYCLE_FOCUS_SECONDS:-2}"
REUSE_EXISTING="${CYCLE_REUSE_EXISTING:-0}"
QUIT_AFTER_CAPTURE="${CYCLE_QUIT_AFTER_CAPTURE:-1}"
QUIT_WAIT_SECONDS="${CYCLE_QUIT_WAIT_SECONDS:-3}"
FILTER_LOGS="${CYCLE_FILTER_LOGS:-1}"
DIRECT_LAUNCH="${CYCLE_DIRECT_LAUNCH:-1}"
CAPTURE_RECT="${CYCLE_CAPTURE_RECT:-}"
PREFLIGHT_PERMISSIONS="${CYCLE_PREFLIGHT_PERMISSIONS:-1}"

if [[ ! -d "$APP_PATH" ]]; then
    echo "Cycle app not found: $APP_PATH" >&2
    exit 1
fi

APP_BUNDLE_ID="${CYCLE_APP_BUNDLE_ID:-$(plutil -extract CFBundleIdentifier raw -o - "$APP_PATH/Contents/Info.plist")}"
APP_EXECUTABLE_NAME="${CYCLE_APP_EXECUTABLE_NAME:-$(plutil -extract CFBundleExecutable raw -o - "$APP_PATH/Contents/Info.plist")}"
APP_EXECUTABLE_PATH="$APP_PATH/Contents/MacOS/$APP_EXECUTABLE_NAME"

if [[ "$DIRECT_LAUNCH" == "1" && ! -x "$APP_EXECUTABLE_PATH" ]]; then
    echo "Cycle executable not found: $APP_EXECUTABLE_PATH" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT_PATH")"
mkdir -p "$(dirname "$LOG_PATH")"
mkdir -p "$(dirname "$RAW_LOG_PATH")"

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

    echo "Cycle UI capture requires macOS Accessibility permission for the calling terminal/app." >&2
    echo "Grant permission in System Settings > Privacy & Security > Accessibility, then rerun this script." >&2
    open_privacy_pane "Privacy_Accessibility"
    return 1
}

preflight_screen_capture_permission() {
    local probe_path="${TMPDIR:-/tmp}/cycle-ui-screen-permission-probe.png"
    rm -f "$probe_path"

    if screencapture -x "$probe_path" >/dev/null 2>&1 && [[ -s "$probe_path" ]]; then
        rm -f "$probe_path"
        return 0
    fi

    echo "Cycle UI capture requires macOS Screen Recording permission for the calling terminal/app." >&2
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

if [[ "$PREFLIGHT_PERMISSIONS" == "1" ]]; then
    preflight_accessibility_permission
    preflight_screen_capture_permission
fi

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

if [[ "$DIRECT_LAUNCH" == "1" ]]; then
    "$APP_EXECUTABLE_PATH" > "$RAW_LOG_PATH" 2>&1 &
    APP_PID=$!
else
    open -n -o "$RAW_LOG_PATH" --stderr "$RAW_LOG_PATH" "$APP_PATH"
    APP_PID=""
fi

deadline=$((SECONDS + WAIT_SECONDS))
while (( SECONDS < deadline )); do
    if process_exists; then
        break
    fi
    sleep 0.25
done

if ! process_exists; then
    echo "Cycle did not appear within ${WAIT_SECONDS}s" >&2
    exit 1
fi

osascript -e "tell application id \"$APP_BUNDLE_ID\" to activate" \
    || osascript -e "tell application \"$PROCESS_NAME\" to activate" \
    || true
osascript -e "tell application \"System Events\" to set frontmost of process \"$PROCESS_NAME\" to true" || true

focus_deadline=$((SECONDS + FOCUS_SECONDS))
while (( SECONDS < focus_deadline )); do
    if process_frontmost; then
        break
    fi
    sleep 0.25
done

if ! process_frontmost; then
    echo "Cycle did not become frontmost within ${FOCUS_SECONDS}s" >&2
    exit 1
fi

if [[ -n "$CAPTURE_RECT" ]]; then
    screencapture -x -R "$CAPTURE_RECT" "$OUT_PATH"
else
    screencapture -x "$OUT_PATH"
fi

if [[ "$QUIT_AFTER_CAPTURE" == "1" ]]; then
    osascript -e "tell application \"$PROCESS_NAME\" to quit" || true

    quit_deadline=$((SECONDS + QUIT_WAIT_SECONDS))
    while (( SECONDS < quit_deadline )); do
        if ! process_exists; then
            break
        fi
        sleep 0.25
    done

    if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
        kill "$APP_PID" 2>/dev/null || true
    fi
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
