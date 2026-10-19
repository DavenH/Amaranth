#!/bin/zsh
set -euo pipefail
SCRIPT_DIR="${0:A:h}"
args=(capture --product cycle1)
[[ $# -lt 1 ]] || args+=(--output "$1")
[[ $# -lt 2 ]] || args+=(--app-log "$2")
[[ -z "${CYCLE_CAPTURE_RECT:-}" ]] || args+=(--rect "$CYCLE_CAPTURE_RECT")
exec python3 "$SCRIPT_DIR/cycle_automation.py" "${args[@]}"
