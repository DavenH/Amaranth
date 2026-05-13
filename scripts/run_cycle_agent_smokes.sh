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
run_fixture assertion-paths scripts/fixtures/cycle-agent-assertion-paths.json
run_fixture mesh-targets scripts/fixtures/cycle-agent-mesh-targets.json
run_fixture calmingkeys-mesh scripts/fixtures/cycle-agent-calmingkeys-mesh.json
run_fixture mesh-mutations scripts/fixtures/cycle-agent-mesh-mutations.json
run_fixture mesh-selection-gesture scripts/fixtures/cycle-agent-mesh-selection-gesture.json
run_fixture screenshot scripts/fixtures/cycle-agent-screenshot.json
run_fixture menu-commands scripts/fixtures/cycle-agent-menu-commands.json
run_fixture main-tabs scripts/fixtures/cycle-agent-main-tabs.json
run_fixture layer-buttons scripts/fixtures/cycle-agent-layer-buttons.json
run_fixture effect-targets scripts/fixtures/cycle-agent-effect-targets.json
run_fixture set-morph-slider scripts/fixtures/cycle-agent-set-morph-slider.json
run_fixture broader-controls scripts/fixtures/cycle-agent-broader-controls.json
run_fixture factory-preset scripts/fixtures/cycle-agent-factory-preset.json
run_fixture general-controls scripts/fixtures/cycle-agent-general-controls.json
run_fixture waveform3d-state scripts/fixtures/cycle-agent-waveform3d-state.json
run_fixture modmatrix-dialog scripts/fixtures/cycle-agent-modmatrix-dialog.json
run_fixture modmatrix-menu scripts/fixtures/cycle-agent-modmatrix-menu.json
run_fixture envelope-config-menu scripts/fixtures/cycle-agent-envelope-config-menu.json
run_fixture midi-note scripts/fixtures/cycle-agent-midi-note.json
run_fixture audio-capture scripts/fixtures/cycle-agent-audio-capture.json

CYCLE_AGENT_ALLOW_FAILURES=1 run_fixture assert-failure scripts/fixtures/cycle-agent-assert-failure.json

if [[ "$RUN_OS_SCREENSHOT" == "1" ]]; then
    CYCLE_PREFLIGHT_SCREEN_CAPTURE=1 \
    CYCLE_OS_SCREENSHOT_AREA=AreaWfrmWaveform3D \
    CYCLE_OS_SCREENSHOT_PATH="$ARTIFACT_DIR/waveform3d-os.png" \
        run_fixture waveform3d-os-screenshot scripts/fixtures/cycle-agent-waveform3d-os-screenshot.json
fi

echo "$ARTIFACT_DIR"
