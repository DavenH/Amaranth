#!/bin/zsh
set -euo pipefail
SCRIPT_DIR="${0:A:h}"
exec python3 "$SCRIPT_DIR/cycle_automation.py" suite --product cycle1 \
    --manifest "$SCRIPT_DIR/fixtures/cycle1-smoke-suite.json" "$@"
