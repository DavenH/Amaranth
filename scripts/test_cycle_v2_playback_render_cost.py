#!/usr/bin/env python3
"""Verify playback redraws reuse sampled data, graph state and prepared surfaces."""

import argparse
import json
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
FIXTURE = REPO / "scripts/fixtures/cycle-v2-agent-playback-render-cost.json"
ARTIFACTS = REPO / ".context/playback-render-cost"


def verify(report):
    assert report["ok"], [result for result in report["results"] if not result["ok"]][:1]
    states = [result["data"] for result in report["results"] if result.get("type") == "snapshotState"]
    metrics = [result["data"] for result in report["results"] if result.get("type") == "inspectCanvasPerformance"]
    assert len(states) == 42, len(states)
    assert len(metrics) == 3, len(metrics)
    summaries = []
    for group, metric in zip((states[:14], states[14:28], states[28:]), metrics):
        before = group[0]
        node_id = before["expandedNodeControls"]["nodeId"]
        envelope = "effect2D" in before["expandedNodeControls"]
        def panel(state):
            editor = state["expandedNodeControls"]
            return editor["effect2D"]["panelState"]["morphSurface"] if envelope else editor["trimesh"]
        initial = panel(before)
        stable_fields = ("rebuildCount", "textureUploadCount", "snapshotReadbackCount") if envelope else (
            "surfaceRebuildCount", "sliceRebuildCount", "surfaceDrawCount", "guideTablePreparationCount", "backgroundPreparationCount")
        positions = set()
        for index, state in enumerate(group):
            current = panel(state)
            assert current["playbackVoiceCount"] == (0 if index == 13 else 2), (node_id, index)
            positions.add(tuple(line["position"] for line in current["playbackCursors"])
                          + tuple((marker["x"], marker["y"]) for marker in current["playbackDots"]))
            for field in stable_fields:
                assert current[field] == initial[field], (node_id, index, field, initial[field], current[field])
            for field in ("documentRevision", "documentGraphRevision", "documentDirty", "graphRevision",
                          "canUndo", "canRedo", "previewRenderCount", "previewStats"):
                assert state[field] == before[field], (node_id, index, field)
            assert state["performance"]["audioPlanCopyCount"] == before["performance"]["audioPlanCopyCount"]
        assert len(positions) > 2, f"Playback overlay stopped animating for {node_id}"
        if envelope:
            assert initial["snapshotReadbackCount"] == 0
            assert panel(group[-2])["expandedDirectRenderCount"] > initial["expandedDirectRenderCount"]
        frames = metric["frames"]["openGlRender"]
        assert frames["count"] > 5, (node_id, frames)
        summaries.append({"nodeId": node_id, "openGL": frames, "stable": {key: initial[key] for key in stable_fields}})
    return summaries


def verify_invalidation(report):
    assert report["ok"], [result for result in report["results"] if not result["ok"]][:1]
    states = [result["data"] for result in report["results"] if result.get("type") == "snapshotState"]
    assert len(states) == 9, len(states)
    panels = [state["expandedNodeControls"]["trimesh"] for state in states]
    for previous, current in ((0, 1), (1, 2), (2, 3), (3, 4), (5, 6), (7, 8)):
        assert panels[current]["surfaceDrawCount"] > panels[previous]["surfaceDrawCount"], (previous, current)
    assert panels[1]["surfaceRebuildCount"] == panels[0]["surfaceRebuildCount"], "Zoom resampled the mesh"
    assert panels[3]["surfaceRebuildCount"] > panels[2]["surfaceRebuildCount"], "Changed secondary morph was not sampled"
    assert panels[4]["surfaceRebuildCount"] == panels[3]["surfaceRebuildCount"], "Context reload resampled the mesh"
    for previous, current in ((4, 5), (6, 7)):
        for field in ("surfaceDrawCount", "surfaceRebuildCount", "sliceRebuildCount", "guideTablePreparationCount"):
            assert panels[current][field] == panels[previous][field], (previous, current, field)
    return {"invalidation": "zoom, resize, data edits, context reload and domain changes refresh; unchanged frames reuse"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--invalidation", action="store_true", help="Check surface invalidation dependencies")
    mode.add_argument("--scaled", action="store_true", help="Add 64 disconnected offscreen graph nodes")
    parser.add_argument("--report", type=Path, help="Verify an existing report")
    args = parser.parse_args()
    artifacts = ARTIFACTS / ("invalidation" if args.invalidation else "scaled" if args.scaled else "ordinary")
    artifacts.mkdir(parents=True, exist_ok=True)
    report_path = args.report or artifacts / "report.json"
    if args.report is None:
        fixture = REPO / "scripts/fixtures/cycle-v2-agent-playback-surface-invalidation.json" if args.invalidation else FIXTURE
        if args.scaled:
            graph = json.loads((REPO / "cycle-v2/content/presets/brass-section.cyclegraph").read_text())
            for index in range(64):
                graph["nodes"].append({"id": f"unusedAdd{index}", "kind": "add", "definitionVersion": 1,
                                       "position": {"x": 15000 + index * 200, "y": 15000}, "parameters": {}})
            graph_path = artifacts / "scaled.cyclegraph"
            graph_path.write_text(json.dumps(graph))
            commands = json.loads(FIXTURE.read_text())
            commands["commands"][0]["path"] = str(graph_path)
            fixture = artifacts / "cycle-v2-agent-playback-render-cost-scaled.json"
            fixture.write_text(json.dumps(commands))
        command = [str(REPO / "scripts/cycle_automation.py"), "run", "--product", "cycle-v2", "--clean",
                   "--fixture", str(fixture), "--report", str(report_path), "--json"]
        if args.no_build:
            command.append("--no-build")
        completed = subprocess.run(command, cwd=REPO, check=True, capture_output=True, text=True)
        (artifacts / "run.json").write_text(completed.stdout)
    verifier = verify_invalidation if args.invalidation else verify
    summaries = verifier(json.loads(report_path.read_text()))
    (artifacts / "summary.json").write_text(json.dumps(summaries, indent=2))
    print(json.dumps(summaries, indent=2))
    if not args.invalidation:
        print("Playback animates with zero graph, preview, slice, surface, palette or repeated surface-preparation work.")


if __name__ == "__main__":
    main()
