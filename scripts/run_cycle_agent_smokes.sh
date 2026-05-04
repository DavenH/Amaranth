#!/bin/zsh
set -euo pipefail

ARTIFACT_DIR="${CYCLE_AGENT_ARTIFACT_DIR:-/private/tmp/cycle-agent-smokes}"
RUN_OS_SCREENSHOT="${CYCLE_AGENT_SMOKE_OS_SCREENSHOT:-0}"

mkdir -p "$ARTIFACT_DIR"

run_fixture() {
    local name="$1"
    local fixture="$2"

    scripts/run_cycle_agent.sh \
        "$fixture" \
        "$ARTIFACT_DIR/$name-report.json" \
        "$ARTIFACT_DIR/$name-logs.txt"
}

run_fixture readonly scripts/fixtures/cycle-agent-readonly.json
run_fixture screenshot scripts/fixtures/cycle-agent-screenshot.json
run_fixture set-morph-slider scripts/fixtures/cycle-agent-set-morph-slider.json
run_fixture factory-preset scripts/fixtures/cycle-agent-factory-preset.json
run_fixture general-controls scripts/fixtures/cycle-agent-general-controls.json
run_fixture waveform3d-state scripts/fixtures/cycle-agent-waveform3d-state.json

CYCLE_AGENT_ALLOW_FAILURES=1 run_fixture assert-failure scripts/fixtures/cycle-agent-assert-failure.json

if [[ "$RUN_OS_SCREENSHOT" == "1" ]]; then
    CYCLE_PREFLIGHT_SCREEN_CAPTURE=1 \
    CYCLE_OS_SCREENSHOT_AREA=AreaWfrmWaveform3D \
    CYCLE_OS_SCREENSHOT_PATH="$ARTIFACT_DIR/waveform3d-os.png" \
        run_fixture waveform3d-os-screenshot scripts/fixtures/cycle-agent-waveform3d-os-screenshot.json
fi

echo "$ARTIFACT_DIR"
