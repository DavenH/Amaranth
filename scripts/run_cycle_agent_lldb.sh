#!/bin/zsh
set -euo pipefail
SCRIPT_DIR="${0:A:h}"
if [[ $# -lt 1 ]]; then
    echo "Usage: run_cycle_agent_lldb.sh <fixture.json> [lldb-commands.txt|-] [report.json] [app-log.txt] [lldb-log.txt]" >&2
    exit 2
fi
args=(debug --product cycle1 --fixture "$1")
if [[ $# -ge 2 && "$2" != - ]]; then
    args+=(--commands "$2")
fi
[[ $# -lt 3 ]] || args+=(--report "$3")
[[ $# -lt 4 ]] || args+=(--app-log "$4")
[[ $# -lt 5 ]] || args+=(--lldb-log "$5")
exec python3 "$SCRIPT_DIR/cycle_automation.py" "${args[@]}"
