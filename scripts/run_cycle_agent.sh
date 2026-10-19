#!/bin/zsh
set -euo pipefail
SCRIPT_DIR="${0:A:h}"
if [[ $# -lt 1 ]]; then
    echo "Usage: run_cycle_agent.sh <fixture.json> [report.json] [log.txt] [screenshot.png]" >&2
    exit 2
fi
args=(run --product cycle1 --fixture "$1")
[[ $# -lt 2 ]] || args+=(--report "$2")
[[ $# -lt 3 ]] || args+=(--app-log "$3")
if [[ -n "${CYCLE_OS_SCREENSHOT_PATH:-}" ]]; then
    args+=(--screenshot "$CYCLE_OS_SCREENSHOT_PATH")
fi
if [[ -n "${CYCLE_OS_SCREENSHOT_AREA:-}" ]]; then
    args+=(--area "$CYCLE_OS_SCREENSHOT_AREA" --target "${CYCLE_OS_SCREENSHOT_TARGET:-}")
fi
[[ -z "${CYCLE_OS_VIDEO_PATH:-}" ]] || args+=(--video "$CYCLE_OS_VIDEO_PATH")
[[ "${CYCLE_AGENT_ALLOW_FAILURES:-0}" != 1 ]] || args+=(--allow-failures)
exec python3 "$SCRIPT_DIR/cycle_automation.py" "${args[@]}"
