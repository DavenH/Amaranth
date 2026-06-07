#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
REPO_ROOT="${SCRIPT_DIR:h}"

export CYCLE_APP_PATH="${CYCLE_APP_PATH:-$REPO_ROOT/build/standalone-debug/cycle-v2/CycleV2.app}"
export CYCLE_PROCESS_NAME="${CYCLE_PROCESS_NAME:-CycleV2}"
export CYCLE_LOG_PATH="${CYCLE_LOG_PATH:-/tmp/cycle-v2-agent-session.log}"

exec "$SCRIPT_DIR/run_cycle_agent_session.sh" "$@"
