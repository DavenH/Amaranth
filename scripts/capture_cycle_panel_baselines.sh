#!/bin/zsh
set -euo pipefail

REPO_ROOT="${CYCLE_REPO_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
OUT_DIR="${1:-$REPO_ROOT/docs/media/images/cycle-panel-baselines}"
ARTIFACT_DIR="${CYCLE_PANEL_BASELINE_ARTIFACT_DIR:-/private/tmp/cycle-panel-baselines}"
PRESET_NAME="${CYCLE_PANEL_BASELINE_PRESET:-AfricanHorn}"
BUILD_FIRST="${CYCLE_PANEL_BASELINE_BUILD_FIRST:-0}"
PARK_MOUSE_POINT="${CYCLE_PANEL_BASELINE_PARK_MOUSE_POINT:-20,20}"

mkdir -p "$OUT_DIR"
mkdir -p "$ARTIFACT_DIR"

if [[ "$BUILD_FIRST" == "1" ]]; then
    cmake --build --preset standalone-debug --target Cycle
fi

capture_os_panel() {
    local area="$1"
    local output_name="$2"
    local fixture="$ARTIFACT_DIR/cycle-agent-panel-baselines-$output_name.json"
    local report="$ARTIFACT_DIR/cycle-agent-panel-baselines-$output_name-report.json"
    local log="$ARTIFACT_DIR/cycle-agent-panel-baselines-$output_name-logs.txt"
    local output="$OUT_DIR/$output_name.png"

    cat > "$fixture" <<JSON
{
  "commands": [
    { "command": "inspectTargets", "area": "$area" },
    { "command": "waitForIdle", "delayMs": 300 }
  ],
  "quit": false
}
JSON

    CYCLE_OS_SCREENSHOT_AREA="$area" \
    CYCLE_OS_SCREENSHOT_PATH="$output" \
    CYCLE_OS_SCREENSHOT_PARK_MOUSE_POINT="$PARK_MOUSE_POINT" \
        scripts/run_cycle_agent.sh "$fixture" "$report" "$log"
}

capture_os_panel "AreaWshpEditor" "waveform2d-africanhorn"
capture_os_panel "AreaSpectrum" "spectrum2d-africanhorn"
capture_os_panel "AreaWfrmWaveform3D" "waveform3d-africanhorn"
capture_os_panel "AreaSpectrogram" "spectrum3d-africanhorn"
capture_os_panel "AreaGuideCurves" "guidecurves-africanhorn"
capture_os_panel "AreaWaveshaper" "waveshaper-fx-africanhorn"

CAPTURED_AT="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

cat > "$OUT_DIR/manifest.json" <<JSON
{
  "preset": "$PRESET_NAME",
  "capturedAt": "$CAPTURED_AT",
  "osCroppedScreenshots": [
    {
      "area": "AreaWshpEditor",
      "path": "waveform2d-africanhorn.png",
      "notes": "Waveform2D OS crop using automation-reported screen bounds."
    },
    {
      "area": "AreaSpectrum",
      "path": "spectrum2d-africanhorn.png",
      "notes": "Spectrum2D OS crop using automation-reported screen bounds."
    },
    {
      "area": "AreaWfrmWaveform3D",
      "path": "waveform3d-africanhorn.png",
      "notes": "Waveform3D OS crop using automation-reported screen bounds."
    },
    {
      "area": "AreaSpectrogram",
      "path": "spectrum3d-africanhorn.png",
      "notes": "Spectrum3D OS crop using automation-reported screen bounds."
    },
    {
      "area": "AreaGuideCurves",
      "path": "guidecurves-africanhorn.png",
      "notes": "Guide curve panel OS crop using automation-reported screen bounds."
    },
    {
      "area": "AreaWaveshaper",
      "path": "waveshaper-fx-africanhorn.png",
      "notes": "Waveshaper FX panel OS crop using automation-reported screen bounds."
    }
  ]
}
JSON

echo "$OUT_DIR"
