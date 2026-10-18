#!/bin/zsh
set -euo pipefail
SCRIPT_DIR="${0:A:h}"
exec python3 "$SCRIPT_DIR/cycle_automation.py" suite --product cycle-v2 \
    --manifest "$SCRIPT_DIR/fixtures/cycle-v2-smoke-suite.json" "$@"
