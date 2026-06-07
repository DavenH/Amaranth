#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
REPO_ROOT="${SCRIPT_DIR:h}"
ARTIFACT_DIR="${CYCLE_V2_AGENT_ARTIFACT_DIR:-/private/tmp/cycle-v2-agent-smokes}"
RUN_OS_SCREENSHOT="${CYCLE_V2_AGENT_SMOKE_OS_SCREENSHOT:-0}"

fixtures=(
    readonly=scripts/fixtures/cycle-v2-agent-readonly.json
    graph-roundtrip=scripts/fixtures/cycle-v2-agent-graph-roundtrip.json
    graph-editing=scripts/fixtures/cycle-v2-agent-graph-editing.json
    screenshot=scripts/fixtures/cycle-v2-agent-screenshot.json
    mesh-controls=scripts/fixtures/cycle-v2-agent-mesh-controls.json
    trimesh-controls=scripts/fixtures/cycle-v2-agent-trimesh-controls.json
    pointer=scripts/fixtures/cycle-v2-agent-pointer.json
)

if [[ "$RUN_OS_SCREENSHOT" == "1" ]]; then
    fixtures+=(mesh-controls-os-screenshot=scripts/fixtures/cycle-v2-agent-mesh-controls-os-screenshot.json)
fi

if (( $# > 0 )); then
    fixtures=()

    for name in "$@"; do
        fixtures+=("$name=scripts/fixtures/cycle-v2-agent-$name.json")
    done
fi

mkdir -p "$ARTIFACT_DIR"

suite_status=0

for fixture in "${fixtures[@]}"; do
    name="${fixture%%=*}"
    fixture_path="${fixture#*=}"
    report="$ARTIFACT_DIR/$name-report.json"
    log="$ARTIFACT_DIR/$name-log.txt"

    echo "cycle-v2 smoke: $name"

    if [[ "$name" == "mesh-controls-os-screenshot" ]]; then
        if ! CYCLE_PREFLIGHT_SCREEN_CAPTURE=1 \
                CYCLE_OS_SCREENSHOT_AREA=canvas \
                CYCLE_OS_SCREENSHOT_PATH="$ARTIFACT_DIR/mesh-controls-os.png" \
                "$SCRIPT_DIR/run_cycle_v2_agent.sh" "$REPO_ROOT/$fixture_path" "$report" "$log"; then
            suite_status=1
        elif ! "$SCRIPT_DIR/assert_png_stats.py" \
                "$ARTIFACT_DIR/mesh-controls-os.png" \
                --label mesh-controls-os \
                --region 0.06,0.12,0.51,0.42 \
                --region 0.06,0.58,0.88,0.32 \
                --min-mean 0.02 \
                --min-stddev 0.015; then
            suite_status=1
        fi
    elif ! "$SCRIPT_DIR/run_cycle_v2_agent.sh" "$REPO_ROOT/$fixture_path" "$report" "$log"; then
        suite_status=1
    fi
done

exit "$suite_status"
