#!/bin/zsh
set -euo pipefail
setopt typeset_silent

REPO_ROOT="${CYCLE_REPO_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
OUT_DIR="${1:-$REPO_ROOT/docs/media/images/cycle-panel-baselines}"
ARTIFACT_DIR="${CYCLE_PANEL_BASELINE_ARTIFACT_DIR:-/private/tmp/cycle-panel-baselines}"
PRESET_NAME="${CYCLE_PANEL_BASELINE_PRESET:-bongo}"
EXPECTED_DOCUMENT="${CYCLE_PANEL_BASELINE_EXPECTED_DOCUMENT:-Bongo}"
BUILD_FIRST="${CYCLE_PANEL_BASELINE_BUILD_FIRST:-0}"
PARK_MOUSE_POINT="${CYCLE_PANEL_BASELINE_PARK_MOUSE_POINT:-20,20}"
PROCESS_NAME="${CYCLE_PROCESS_NAME:-Cycle}"
SOCKET_PATH="$ARTIFACT_DIR/cycle-agent-panel-baselines.sock"
SESSION_LOG="$ARTIFACT_DIR/cycle-agent-panel-baselines-session.log"

MAIN_AREAS=(
    AreaWshpEditor
    AreaWfrmWaveform3D
    AreaGuideCurves
    AreaWaveshaper
)

MAIN_OUTPUT_NAMES=(
    waveform2d-bongo
    waveform3d-bongo
    guidecurves-bongo
    waveshaper-fx-bongo
)

SPECTRUM_AREAS=(
    AreaSpectrum
    AreaSpectrogram
)

SPECTRUM_OUTPUT_STEMS=(
    spectrum2d-bongo
    spectrum3d-bongo
)

mkdir -p "$OUT_DIR"
mkdir -p "$ARTIFACT_DIR"

if [[ "$BUILD_FIRST" == "1" ]]; then
    cmake --build --preset standalone-debug --target Cycle
fi

quit_cycle() {
    osascript -e "tell application \"$PROCESS_NAME\" to quit" >/dev/null 2>&1 || true
}

focus_cycle() {
    osascript -e "tell application \"$PROCESS_NAME\" to activate" >/dev/null 2>&1 || true
    osascript -e "tell application \"System Events\" to set frontmost of process \"$PROCESS_NAME\" to true" >/dev/null 2>&1 || true
}

park_mouse() {
    if [[ -z "$PARK_MOUSE_POINT" ]]; then
        return 0
    fi

    python3 - "$PARK_MOUSE_POINT" <<'PY'
import ctypes
import sys

point = sys.argv[1].split(",", 1)
x = float(point[0])
y = float(point[1])

core_graphics = ctypes.CDLL(
        "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")

class CGPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double)]

core_graphics.CGWarpMouseCursorPosition.argtypes = [CGPoint]
core_graphics.CGWarpMouseCursorPosition.restype = ctypes.c_int
core_graphics.CGAssociateMouseAndMouseCursorPosition.argtypes = [ctypes.c_bool]
core_graphics.CGAssociateMouseAndMouseCursorPosition.restype = ctypes.c_int

core_graphics.CGWarpMouseCursorPosition(CGPoint(x, y))
core_graphics.CGAssociateMouseAndMouseCursorPosition(True)
PY

    sleep 0.2
}

run_command() {
    local name="$1"
    local command_file="$2"
    local response_file="$ARTIFACT_DIR/$name-response.json"

    scripts/cycle_agent_session.py "$SOCKET_PATH" -f "$command_file" --id "$name" > "$response_file"

    if ! jq -er '.ok == true and .result.ok == true' "$response_file" >/dev/null; then
        echo "Cycle automation command failed: $name" >&2
        jq . "$response_file" >&2
        return 1
    fi

    echo "$response_file"
}

write_command() {
    local command_path="$1"
    shift

    jq -n "$@" > "$command_path"
}

inspect_area() {
    local state="$1"
    local area="$2"
    local command_file="$ARTIFACT_DIR/inspect-$state-$area.json"

    write_command "$command_file" --arg area "$area" '{ command: "inspectTargets", area: $area }'
    run_command "inspect-$state-$area" "$command_file"
}

capture_from_response() {
    local response_file="$1"
    local output_name="$2"
    local output="$OUT_DIR/$output_name.png"
    local rect

    rect="$(jq -er \
        '
            [
                .result.data.targets[]
                | select(.target == "" and .resolved == true)
                | .screenBounds
            ][0]
            | "\(.x),\(.y),\(.width),\(.height)"
        ' "$response_file")"

    screencapture -x -R "$rect" "$output"

    if [[ ! -s "$output" ]]; then
        echo "OS screenshot was not written: $output" >&2
        return 1
    fi
}

switch_spectrum_mode() {
    local mode="$1"
    local command_file="$ARTIFACT_DIR/switch-spectrum-$mode.json"

    write_command "$command_file" --arg id "$mode" \
        '{ command: "action", actionType: "SwitchMode", area: "AreaSpectrum", id: $id }'
    run_command "switch-spectrum-$mode" "$command_file" >/dev/null

    write_command "$command_file" '{ command: "waitForIdle", delayMs: 500 }'
    run_command "wait-spectrum-$mode" "$command_file" >/dev/null
}

capture_main_panels() {
    local state="$1"

    for (( i = 1; i <= ${#MAIN_AREAS[@]}; ++i )); do
        local response_file
        response_file="$(inspect_area "$state" "${MAIN_AREAS[$i]}")"
        capture_from_response "$response_file" "${MAIN_OUTPUT_NAMES[$i]}"
    done
}

capture_spectrum_panels() {
    local state="$1"

    for (( i = 1; i <= ${#SPECTRUM_AREAS[@]}; ++i )); do
        local response_file
        response_file="$(inspect_area "$state" "${SPECTRUM_AREAS[$i]}")"
        capture_from_response "$response_file" "${SPECTRUM_OUTPUT_STEMS[$i]}-$state"
    done
}

trap quit_cycle EXIT

quit_cycle
scripts/run_cycle_agent_session.sh "$SOCKET_PATH" "$SESSION_LOG" >/dev/null

LOAD_COMMAND="$ARTIFACT_DIR/load-preset.json"
write_command "$LOAD_COMMAND" --arg preset "$PRESET_NAME" \
    '{ command: "openFactoryPreset", preset: $preset }'
run_command load-preset "$LOAD_COMMAND" >/dev/null

ASSERT_COMMAND="$ARTIFACT_DIR/assert-document.json"
write_command "$ASSERT_COMMAND" --arg expected "$EXPECTED_DOCUMENT" \
    '{ command: "assertState", path: "document", equals: $expected }'
run_command assert-document "$ASSERT_COMMAND" >/dev/null

WAIT_COMMAND="$ARTIFACT_DIR/wait-after-load.json"
write_command "$WAIT_COMMAND" '{ command: "waitForIdle", delayMs: 700 }'
run_command wait-after-load "$WAIT_COMMAND" >/dev/null

focus_cycle
park_mouse

switch_spectrum_mode IdModeMagn
capture_main_panels magnitude
capture_spectrum_panels magnitude

switch_spectrum_mode IdModePhase
focus_cycle
park_mouse
capture_spectrum_panels phase

quit_cycle
trap - EXIT

CAPTURED_AT="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

cat > "$OUT_DIR/manifest.json" <<JSON
{
  "preset": "$PRESET_NAME",
  "document": "$EXPECTED_DOCUMENT",
  "capturedAt": "$CAPTURED_AT",
  "osCroppedScreenshots": [
    {
      "area": "AreaWshpEditor",
      "path": "waveform2d-bongo.png",
      "state": "magnitude",
      "notes": "Waveform2D OS crop after opening $PRESET_NAME."
    },
    {
      "area": "AreaWfrmWaveform3D",
      "path": "waveform3d-bongo.png",
      "state": "magnitude",
      "notes": "Waveform3D OS crop after opening $PRESET_NAME."
    },
    {
      "area": "AreaGuideCurves",
      "path": "guidecurves-bongo.png",
      "state": "magnitude",
      "notes": "Guide curve panel OS crop after opening $PRESET_NAME."
    },
    {
      "area": "AreaWaveshaper",
      "path": "waveshaper-fx-bongo.png",
      "state": "magnitude",
      "notes": "Waveshaper FX panel OS crop after opening $PRESET_NAME."
    },
    {
      "area": "AreaSpectrum",
      "path": "spectrum2d-bongo-magnitude.png",
      "state": "magnitude",
      "notes": "Spectrum2D OS crop after switching Spectrum3D to magnitude mode."
    },
    {
      "area": "AreaSpectrogram",
      "path": "spectrum3d-bongo-magnitude.png",
      "state": "magnitude",
      "notes": "Spectrum3D OS crop after switching Spectrum3D to magnitude mode."
    },
    {
      "area": "AreaSpectrum",
      "path": "spectrum2d-bongo-phase.png",
      "state": "phase",
      "notes": "Spectrum2D OS crop after switching Spectrum3D to phase mode."
    },
    {
      "area": "AreaSpectrogram",
      "path": "spectrum3d-bongo-phase.png",
      "state": "phase",
      "notes": "Spectrum3D OS crop after switching Spectrum3D to phase mode."
    }
  ]
}
JSON

echo "$OUT_DIR"
