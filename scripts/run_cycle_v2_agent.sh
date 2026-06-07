#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
REPO_ROOT="${SCRIPT_DIR:h}"

export CYCLE_APP_PATH="${CYCLE_APP_PATH:-$REPO_ROOT/build/standalone-debug/cycle-v2/CycleV2.app}"
export CYCLE_PROCESS_NAME="${CYCLE_PROCESS_NAME:-CycleV2}"
export CYCLE_APP_BUNDLE_ID="${CYCLE_APP_BUNDLE_ID:-com.amaranthaudio.cycle-v2}"
export CYCLE_LOG_FILTER_PATTERN="${CYCLE_LOG_FILTER_PATTERN:-CycleV2|Cycle V2|CycleV2Automation|Error|error|Warning|warning|fail|Fail|assert|Assert|exception|Exception|crash|Crash|quit unexpectedly|DiagnosticReports|ips}"

exec "$SCRIPT_DIR/run_cycle_agent.sh" "$@"
