#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
ARTIFACT_DIR="${CYCLE_AGENT_ARTIFACT_DIR:-${SCRIPT_DIR:h}/.context/cycle-automation/cycle1/smokes}"
REPORT_PATH="$ARTIFACT_DIR/cycle-agent-smokes-report.json"
RUN_OS_SCREENSHOT="${CYCLE_AGENT_SMOKE_OS_SCREENSHOT:-0}"
RESET_PRESET="${CYCLE_AGENT_SMOKE_RESET_PRESET-ooh-aah}"
RESUME="${CYCLE_AGENT_SMOKE_RESUME:-0}"
SKIP_MAIN_TAB_RESET="${CYCLE_AGENT_SMOKE_SKIP_MAIN_TAB_RESET:-0}"
BOUNDARY_LOG="${CYCLE_AGENT_SMOKE_BOUNDARY_LOG:-0}"
OPENGL_DIAGNOSTICS_EACH_FIXTURE="${CYCLE_AGENT_SMOKE_OPENGL_DIAGNOSTICS_EACH_FIXTURE:-0}"

mkdir -p "$ARTIFACT_DIR"

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
    opengl-diagnostics=scripts/fixtures/cycle-agent-opengl-diagnostics.json
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
    volume-envelope=scripts/fixtures/cycle-agent-volume-envelope.json
    inactive-volume=scripts/fixtures/cycle-agent-inactive-volume.json
    pitch-envelope=scripts/fixtures/cycle-agent-pitch-envelope.json
    voice-mode-switch=scripts/fixtures/cycle-agent-voice-mode-switch.json
    load-reference-sax-growl=scripts/fixtures/cycle-agent-load-reference-sax-growl.json
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

suite_args=(suite --product cycle1 --report "$REPORT_PATH")
for fixture in "${fixtures[@]}"; do
    suite_args+=(--fixture "$fixture")
done
suite_args+=(--smoke-arg=--reset-preset --smoke-arg="$RESET_PRESET")
suite_args+=(--smoke-arg=--expected-failure --smoke-arg=assert-failure)
[[ "$RESUME" != 1 ]] || suite_args+=(--smoke-arg=--resume)
[[ "$SKIP_MAIN_TAB_RESET" != 1 ]] || suite_args+=(--smoke-arg=--skip-main-tab-reset)
[[ "$BOUNDARY_LOG" != 1 ]] || suite_args+=(--smoke-arg=--boundary-log)
[[ "$OPENGL_DIAGNOSTICS_EACH_FIXTURE" != 1 ]] || suite_args+=(--smoke-arg=--opengl-diagnostics-each-fixture)

scripts/cycle_automation.py "${suite_args[@]}"

if [[ "$RUN_OS_SCREENSHOT" == 1 ]]; then
    rect="$(jq -er '
        [ .fixtures[] | select(.name == "waveform3d-os-screenshot")
          | .results[] | .response.result
          | select(.type == "inspectTargets" and .ok == true)
          | .data.targets[]
          | select(.area == "AreaWfrmWaveform3D" and .resolved == true)
          | .screenBounds ][0]
        | "\(.x),\(.y),\(.width),\(.height)"
    ' "$REPORT_PATH")"
    scripts/cycle_automation.py capture --product cycle1 --no-build \
        --rect "$rect" --output "$ARTIFACT_DIR/waveform3d-os.png"
fi

echo "$ARTIFACT_DIR"
echo "$REPORT_PATH"
