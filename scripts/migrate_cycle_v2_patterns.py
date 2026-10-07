#!/usr/bin/env python3
"""Ship basic sound-type audition patterns and retarget clean factory presets."""

import json
import pathlib
import re
import subprocess

from generate_cycle_v2_preset_sequences import family


ROOT = pathlib.Path(__file__).resolve().parents[1]
PRESETS = ROOT / "cycle-v2/content/presets"
PATTERNS = ROOT / "cycle-v2/content/patterns"


def pattern(tag, duration, notes, values):
    identifier = "factory-basic-" + tag.lower()
    return {
        "version": 1,
        "id": identifier,
        "name": "Basic " + tag,
        "tag": tag,
        "sequence": {
            "durationSeconds": duration,
            "notes": [
                {"pitch": pitch, "velocity": velocity,
                 "startSeconds": start, "durationSeconds": length}
                for pitch, start, length, velocity in notes
            ],
            "controls": [
                {"controller": 1, "value": value, "timeSeconds": time}
                for time, value in values
            ],
        },
    }


def specs():
    return [
        pattern("Bass", 4, [
            (36, 0, .38, 110), (36, .75, .24, 76),
            (43, 1, .35, 96), (39, 1.5, .32, 85),
            (36, 2, .45, 112), (46, 2.75, .2, 72),
            (43, 3, .35, 99), (36, 3.5, .36, 90),
        ], [(0, 18), (2, 82), (4, 18)]),
        pattern("Lead", 4, [
            (60, 0, .45, 85), (63, .5, .45, 97),
            (67, 1, .8, 110), (70, 2, .45, 84),
            (72, 2.5, 1.2, 105),
        ], [(0, 18), (1.5, 42), (3, 92), (4, 25)]),
        pattern("Pad", 8, [
            (48, 0, 3.7, 78), (55, 0, 3.7, 71), (62, 0, 3.7, 68),
            (50, 4, 3.7, 82), (57, 4, 3.7, 74), (64, 4, 3.7, 70),
        ], [(0, 15), (4, 70), (8, 25)]),
        pattern("Keys", 4, [
            (57, 0, .7, 92), (61, 0, .7, 78), (64, 0, .7, 84),
            (60, 2, .7, 87), (64, 2, .7, 73), (67, 2, .7, 81),
        ], [(0, 25), (2, 60), (4, 25)]),
        pattern("Sustained", 6, [
            (58, 0, 1.1, 80), (65, 1.4, .85, 96),
            (68, 2.6, 1.4, 105), (65, 4.4, 1.2, 88),
        ], [(0, 18), (3, 85), (6, 22)]),
        pattern("Rhythm", 4, [
            (48, 0, .17, 113), (55, .5, .13, 72),
            (51, 1, .18, 98), (48, 2, .17, 110),
            (55, 2.5, .13, 68), (51, 3, .18, 94),
        ], [(0, 20), (2, 76), (4, 20)]),
    ]


def selected_id(stem, graph):
    voice = next((node for node in graph["nodes"]
                  if node["kind"] == "voiceContext"), None)
    length = voice["parameters"].get("voiceLength", .5) if voice else .5
    tag = family(stem, length)
    if tag == "percussion":
        tag = "rhythm"
    return "factory-basic-" + tag


def main():
    dirty = set(subprocess.check_output(
        ["git", "diff", "--name-only", "--", str(PRESETS)],
        cwd=ROOT, text=True).splitlines())
    available = specs()
    PATTERNS.mkdir(parents=True, exist_ok=True)
    keep = {entry["id"] for entry in available}
    for path in PATTERNS.glob("factory-*.cyclepattern"):
        if path.stem not in keep:
            path.unlink()
    for entry in available:
        path = PATTERNS / (entry["id"] + ".cyclepattern")
        path.write_text(json.dumps(entry, indent=2) + "\n")

    migrated = 0
    for path in sorted(PRESETS.glob("*.cyclegraph")):
        if str(path.relative_to(ROOT)) in dirty:
            continue
        text = path.read_text()
        graph = json.loads(text)
        presentation = graph.get("presetPresentation", {})
        if "patternId" not in presentation and "sequence" not in presentation:
            continue
        identifier = selected_id(path.stem, graph)
        if "patternId" in presentation:
            updated = re.sub(r'"patternId"\s*:\s*"[^"]+"',
                             '"patternId": "' + identifier + '"', text, count=1)
        else:
            marker = re.search(r'"sequence"\s*:\s*\{', text)
            assert marker
            object_start = marker.end() - 1
            _, consumed = json.JSONDecoder().raw_decode(text[object_start:])
            updated = text[:marker.start()] + '"patternId": "' + identifier + '"' \
                + text[object_start + consumed:]
        assert json.loads(updated)["presetPresentation"]["patternId"] == identifier
        if updated != text:
            path.write_text(updated)
            migrated += 1
    print(f"{len(available)} basic patterns; {migrated} presets retargeted; "
          f"{len(dirty)} modified presets preserved")


if __name__ == "__main__":
    main()
