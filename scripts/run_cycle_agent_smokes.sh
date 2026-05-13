#!/bin/zsh
set -euo pipefail

ARTIFACT_DIR="${CYCLE_AGENT_ARTIFACT_DIR:-/private/tmp/cycle-agent-smokes}"
SOCKET_PATH="${CYCLE_AGENT_SESSION:-$ARTIFACT_DIR/cycle-agent.sock}"
SESSION_LOG="$ARTIFACT_DIR/cycle-agent-session.log"
REPORT_PATH="$ARTIFACT_DIR/cycle-agent-smokes-report.json"
RUN_OS_SCREENSHOT="${CYCLE_AGENT_SMOKE_OS_SCREENSHOT:-0}"
RESET_PRESET="${CYCLE_AGENT_SMOKE_RESET_PRESET:-ooh-aah}"
RESUME="${CYCLE_AGENT_SMOKE_RESUME:-0}"
SKIP_MAIN_TAB_RESET="${CYCLE_AGENT_SMOKE_SKIP_MAIN_TAB_RESET:-0}"

mkdir -p "$ARTIFACT_DIR"
rm -f "$SESSION_LOG"

if [[ "$RESUME" != "1" ]]; then
    rm -f "$REPORT_PATH"
fi

fixtures=(
    readonly=scripts/fixtures/cycle-agent-readonly.json
    hover-surfaces=scripts/fixtures/cycle-agent-hover-surfaces.json
    assertion-paths=scripts/fixtures/cycle-agent-assertion-paths.json
    mesh-targets=scripts/fixtures/cycle-agent-mesh-targets.json
    calmingkeys-mesh=scripts/fixtures/cycle-agent-calmingkeys-mesh.json
    mesh-mutations=scripts/fixtures/cycle-agent-mesh-mutations.json
    mesh-selection-gesture=scripts/fixtures/cycle-agent-mesh-selection-gesture.json
    screenshot=scripts/fixtures/cycle-agent-screenshot.json
    menu-commands=scripts/fixtures/cycle-agent-menu-commands.json
    main-tabs=scripts/fixtures/cycle-agent-main-tabs.json
    layer-buttons=scripts/fixtures/cycle-agent-layer-buttons.json
    effect-targets=scripts/fixtures/cycle-agent-effect-targets.json
    set-morph-slider=scripts/fixtures/cycle-agent-set-morph-slider.json
    broader-controls=scripts/fixtures/cycle-agent-broader-controls.json
    factory-preset=scripts/fixtures/cycle-agent-factory-preset.json
    general-controls=scripts/fixtures/cycle-agent-general-controls.json
    waveform3d-state=scripts/fixtures/cycle-agent-waveform3d-state.json
    modmatrix-dialog=scripts/fixtures/cycle-agent-modmatrix-dialog.json
    modmatrix-menu=scripts/fixtures/cycle-agent-modmatrix-menu.json
    envelope-config-menu=scripts/fixtures/cycle-agent-envelope-config-menu.json
    selector-menu=scripts/fixtures/cycle-agent-selector-menu.json
    hover-selector-menu=scripts/fixtures/cycle-agent-hover-selector-menu.json
    midi-keyboard-pointer=scripts/fixtures/cycle-agent-midi-keyboard-pointer.json
    playback-pointer=scripts/fixtures/cycle-agent-playback-pointer.json
    main-draggers=scripts/fixtures/cycle-agent-main-draggers.json
    pullout-hover=scripts/fixtures/cycle-agent-pullout-hover.json
    midi-note=scripts/fixtures/cycle-agent-midi-note.json
    audio-capture=scripts/fixtures/cycle-agent-audio-capture.json
    assert-failure=scripts/fixtures/cycle-agent-assert-failure.json
)

if [[ "$RUN_OS_SCREENSHOT" == "1" ]]; then
    fixtures+=(waveform3d-os-screenshot=scripts/fixtures/cycle-agent-waveform3d-os-screenshot.json)
fi

if [[ -n "${CYCLE_AGENT_SMOKE_FIXTURES:-}" ]]; then
    requested=("${(@s: :)CYCLE_AGENT_SMOKE_FIXTURES}")
    selected=()

    for fixture in "${fixtures[@]}"; do
        name="${fixture%%=*}"

        if (( ${requested[(Ie)$name]} )); then
            selected+=("$fixture")
        fi
    done

    fixtures=("${selected[@]}")
fi

if (( ${#fixtures[@]} == 0 )); then
    echo "No smoke fixtures selected." >&2
    exit 2
fi

resume_args=()

if [[ "$RESUME" == "1" ]]; then
    resume_args+=(--resume)
fi

tab_reset_args=()

if [[ "$SKIP_MAIN_TAB_RESET" == "1" ]]; then
    tab_reset_args+=(--skip-main-tab-reset)
fi

scripts/run_cycle_agent_session.sh "$SOCKET_PATH" "$SESSION_LOG" >/dev/null

scripts/cycle_agent_smoke_session.py \
    "$SOCKET_PATH" \
    --report "$REPORT_PATH" \
    --reset-preset "$RESET_PRESET" \
    --expected-failure assert-failure \
    "${resume_args[@]}" \
    "${tab_reset_args[@]}" \
    "${fixtures[@]}"

if [[ "$RUN_OS_SCREENSHOT" == "1" ]]; then
    screenshot_path="$ARTIFACT_DIR/waveform3d-os.png"
    rect="$(jq -er '
        [
            .fixtures[]
            | select(.name == "waveform3d-os-screenshot")
            | .results[]
            | .response.result
            | select(.type == "inspectTargets" and .ok == true)
            | .data.targets[]
            | select(.area == "AreaWfrmWaveform3D" and .resolved == true)
            | .screenBounds
        ][0]
        | "\(.x),\(.y),\(.width),\(.height)"
    ' "$REPORT_PATH")"

    screencapture -x -R "$rect" "$screenshot_path"
fi

echo "$ARTIFACT_DIR"
echo "$REPORT_PATH"
echo "$SESSION_LOG"
