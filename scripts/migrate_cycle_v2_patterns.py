#!/usr/bin/env python3
"""Build a compact factory pattern set and reference it from clean presets."""

import hashlib
import json
import pathlib
import re
import subprocess

from generate_cycle_v2_preset_sequences import family


ROOT = pathlib.Path(__file__).resolve().parents[1]
PRESETS = ROOT / "cycle-v2/content/presets"
PATTERNS = ROOT / "cycle-v2/content/patterns"


def notes(items, root):
    return [dict(pitch=root + pitch, velocity=velocity,
                 startSeconds=start, durationSeconds=length)
            for pitch, start, length, velocity in items]


def pattern(name, family_name, root, duration, items, motion):
    controls = [dict(controller=1, value=value,
                     timeSeconds=round(duration * index / 4, 3))
                for index, value in enumerate(motion)]
    return dict(version=1, id="factory-" + name, name=name.replace("-", " ").title(),
                family=family_name,
                sequence=dict(durationSeconds=duration,
                              notes=notes(items, root), controls=controls))


def pad_chords(chords):
    return [(pitch, round(start + voice * .12, 3), length,
             72 + ((chord_index * 3 + voice * 7) % 20))
            for chord_index, (start, length, pitches) in enumerate(chords)
            for voice, pitch in enumerate(pitches)]


def specs():
    result = []

    def add(name, group, root, duration, items, motion):
        result.append(pattern(name, group, root, duration, items, motion))

    add("deep-pocket", "bass", 34, 4, [
        (0, 0, .36, 112), (0, .75, .22, 74), (7, 1, .35, 98),
        (3, 1.5, .34, 87), (0, 2, .45, 115), (10, 2.75, .2, 73),
        (7, 3, .35, 101), (0, 3.5, .34, 94)], [18, 28, 72, 39, 16])
    add("acid-turn", "bass", 36, 4, [
        (0, 0, .3, 113), (12, .375, .16, 66), (0, .75, .2, 97),
        (3, 1.125, .28, 111), (7, 1.75, .2, 78), (0, 2, .37, 119),
        (10, 2.5, .2, 71), (7, 2.75, .2, 93), (3, 3.25, .23, 109)],
        [12, 88, 24, 105, 18])
    add("walking-blue", "bass", 38, 6, [
        (0, 0, .42, 99), (4, .75, .36, 72), (7, 1.5, .38, 91),
        (10, 2.25, .38, 80), (12, 3, .4, 105), (9, 3.75, .35, 74),
        (7, 4.5, .4, 90), (3, 5.25, .37, 85)], [24, 39, 58, 43, 21])
    add("dub-space", "bass", 33, 6, [
        (0, 0, .72, 112), (7, 1.4, .28, 65), (0, 2, .5, 94),
        (12, 3.25, .35, 78), (10, 4, .44, 106), (0, 5, .7, 87)],
        [8, 12, 50, 69, 13])
    add("octave-engine", "bass", 36, 4, [
        (0, 0, .3, 110), (12, .5, .25, 72), (0, 1, .32, 102),
        (7, 1.5, .25, 81), (0, 2, .35, 117), (12, 2.5, .25, 68),
        (10, 3, .3, 96), (7, 3.5, .26, 88)], [29, 45, 83, 52, 31])

    add("skyward-arc", "lead", 60, 5, [
        (0, 0, .42, 83), (3, .5, .42, 96), (7, 1, .7, 108),
        (10, 2, .45, 81), (12, 2.5, .82, 116), (7, 3.5, .42, 89),
        (3, 4, .76, 101)], [18, 38, 81, 100, 35])
    add("minor-hook", "lead", 62, 4, [
        (0, 0, .25, 101), (3, .375, .3, 79), (7, .875, .4, 111),
        (5, 1.5, .28, 93), (3, 1.875, .3, 74), (10, 2.5, .38, 116),
        (7, 3, .25, 89), (0, 3.375, .46, 106)], [25, 94, 40, 73, 29])
    add("wide-legato", "lead", 57, 6, [
        (0, 0, 1.1, 82), (7, 1.2, .85, 97), (12, 2.2, 1.2, 114),
        (10, 3.55, .7, 76), (5, 4.4, 1.35, 103)], [13, 32, 56, 92, 50])
    add("trance-ladder", "lead", 64, 4, [
        (0, 0, .18, 94), (3, .25, .18, 72), (7, .5, .18, 99),
        (12, .75, .2, 116), (7, 1, .18, 78), (3, 1.25, .2, 97),
        (5, 1.5, .18, 81), (10, 1.75, .2, 107), (12, 2, .3, 119),
        (7, 2.5, .18, 85), (3, 2.75, .2, 76), (0, 3.25, .4, 110)],
        [20, 70, 104, 47, 26])
    add("blues-answer", "lead", 60, 6, [
        (0, 0, .38, 97), (3, .5, .27, 76), (5, .9, .4, 108),
        (6, 1.5, .18, 79), (7, 1.75, .75, 113), (0, 3, .33, 91),
        (10, 3.5, .3, 73), (7, 4, .4, 104), (3, 4.75, .3, 85),
        (0, 5.25, .55, 109)], [38, 23, 68, 90, 27])

    add("suspended-cloud", "pad", 48, 10,
        pad_chords([(0, 2.85, [0, 5, 10]), (3, 2.8, [3, 7, 12]),
                    (6, 3.2, [5, 10, 14])]), [10, 23, 57, 94, 34])
    add("open-horizon", "pad", 45, 12,
        pad_chords([(0, 3.5, [0, 7, 14]), (4, 3.5, [5, 12, 19]),
                    (8, 3.5, [3, 10, 17])]), [16, 44, 88, 56, 21])
    add("minor-drift", "pad", 50, 10,
        pad_chords([(0, 2.8, [0, 3, 7]), (3.15, 2.8, [5, 8, 12]),
                    (6.3, 3, [3, 7, 10])]), [31, 19, 69, 100, 42])
    add("fifth-drone", "pad", 43, 12,
        pad_chords([(0, 5.5, [0, 7, 12]), (6, 5.5, [2, 9, 14])]),
        [8, 26, 61, 85, 37])
    add("luminous-rise", "pad", 52, 10,
        pad_chords([(0, 2.7, [0, 4, 9]), (3.1, 2.9, [2, 7, 11]),
                    (6.3, 3.1, [4, 9, 14])]), [12, 35, 75, 108, 46])

    add("swing-comp", "keys", 57, 6, [
        (0, 0, .6, 92), (4, 0, .6, 76), (10, 0, .6, 83),
        (3, 1.5, .45, 85), (7, 1.5, .45, 72), (12, 1.5, .45, 79),
        (5, 3.15, .55, 96), (9, 3.15, .55, 78), (12, 3.15, .55, 88),
        (7, 4.65, .58, 87), (10, 4.65, .58, 73), (14, 4.65, .58, 80)],
        [28, 35, 62, 45, 25])
    add("jazz-run", "keys", 60, 6, [
        (0, 0, .28, 92), (3, .4, .2, 66), (4, .65, .23, 85),
        (7, 1, .32, 101), (10, 1.5, .22, 75), (12, 1.8, .38, 109),
        (11, 2.5, .2, 78), (9, 2.8, .22, 84), (7, 3.1, .3, 96),
        (4, 3.65, .25, 80), (3, 4.05, .26, 73), (0, 4.5, .75, 104)],
        [19, 55, 80, 41, 20])
    add("neo-soul-keys", "keys", 55, 6, [
        (0, 0, .65, 87), (3, 0, .65, 69), (10, 0, .65, 78),
        (7, 1.65, .28, 97), (5, 2, .4, 72),
        (2, 3, .66, 90), (5, 3, .66, 74), (9, 3, .66, 82),
        (12, 4.45, .28, 103), (10, 4.8, .45, 77)],
        [17, 40, 70, 31, 20])
    add("glass-arpeggio", "keys", 64, 4, [
        (0, 0, .23, 91), (4, .25, .22, 68), (7, .5, .2, 81),
        (12, .75, .3, 102), (7, 1.25, .25, 76), (4, 1.5, .23, 89),
        (2, 2, .23, 95), (5, 2.25, .22, 70), (9, 2.5, .23, 86),
        (14, 2.75, .34, 108), (9, 3.25, .23, 75), (5, 3.5, .24, 93)],
        [24, 61, 38, 87, 29])
    add("offbeat-organ", "keys", 57, 4, [
        (0, .5, .24, 97), (7, .5, .24, 79), (3, 1.5, .22, 91),
        (10, 1.5, .22, 72), (5, 2.5, .23, 101), (12, 2.5, .23, 81),
        (3, 3.5, .24, 94), (10, 3.5, .24, 74)],
        [35, 29, 75, 56, 33])
    add("vibraphone-drops", "keys", 67, 6, [
        (0, 0, .8, 99), (7, .75, .6, 73), (10, 1.65, .85, 87),
        (3, 2.8, .62, 69), (12, 3.5, .9, 106),
        (5, 4.75, .85, 79)], [18, 32, 64, 48, 22])

    add("sax-blue-hour", "sustained", 58, 6, [
        (0, 0, .72, 81), (3, .88, .35, 69), (5, 1.35, .43, 96),
        (6, 2, .18, 74), (7, 2.25, 1.1, 108), (10, 3.65, .3, 71),
        (7, 4.1, .42, 89), (3, 4.7, .36, 76), (0, 5.2, .65, 100)],
        [15, 47, 86, 57, 25])
    add("sax-night-walk", "sustained", 55, 8, [
        (0, 0, 1.15, 75), (7, 1.45, .65, 91), (10, 2.35, .42, 68),
        (12, 2.95, 1.2, 107), (9, 4.45, .48, 77),
        (7, 5.1, .62, 94), (3, 6, .36, 72), (0, 6.6, 1.05, 101)],
        [22, 34, 79, 62, 20])
    add("brass-fanfare", "sustained", 55, 6, [
        (0, 0, .65, 104), (7, .8, .38, 78), (12, 1.4, 1.25, 116),
        (10, 3, .56, 91), (7, 3.7, .42, 77), (0, 4.35, 1.25, 110)],
        [30, 75, 102, 69, 28])
    add("flute-current", "sustained", 67, 8, [
        (0, 0, 1.2, 75), (2, 1.35, .7, 88), (7, 2.2, 1.3, 100),
        (5, 3.8, .65, 70), (9, 4.7, 1.15, 94), (7, 6.05, 1.4, 82)],
        [12, 31, 72, 43, 18])
    add("strings-dialogue", "sustained", 52, 8, [
        (0, 0, 1.3, 87), (7, 1.25, 1.3, 76), (3, 2.7, 1.35, 97),
        (10, 4.2, 1.4, 79), (7, 5.8, 1.8, 103)],
        [16, 40, 74, 58, 27])

    add("hand-drum-dialogue", "percussion", 48, 4, [
        (0, 0, .16, 117), (7, .375, .12, 69), (3, .75, .16, 91),
        (0, 1, .16, 105), (5, 1.5, .13, 78), (7, 1.75, .12, 67),
        (0, 2, .16, 119), (3, 2.5, .14, 85), (5, 2.75, .12, 73),
        (0, 3, .18, 110), (7, 3.5, .13, 79)],
        [22, 48, 85, 39, 24])
    add("metallic-sparks", "percussion", 60, 4, [
        (0, 0, .11, 107), (5, .25, .09, 68), (9, .75, .12, 88),
        (12, 1.5, .15, 117), (5, 1.875, .09, 73),
        (9, 2.25, .1, 96), (0, 2.75, .12, 82),
        (12, 3.25, .17, 111)], [18, 60, 99, 35, 20])
    add("tom-steps", "percussion", 43, 4, [
        (0, 0, .2, 116), (3, .5, .16, 77), (7, 1, .19, 98),
        (10, 1.5, .16, 67), (7, 2, .18, 109),
        (3, 2.5, .16, 82), (0, 3, .22, 120),
        (10, 3.5, .17, 74)], [15, 37, 72, 51, 19])
    return result


def select_id(stem, graph, available):
    voice = next((node for node in graph["nodes"]
                  if node["kind"] == "voiceContext"), None)
    length = voice["parameters"].get("voiceLength", 0.5) if voice else 0.5
    group = family(stem, length)
    choices = [entry for entry in available if entry["family"] == group]
    if group == "keys":
        if re.search(r"piano|whurl|keys", stem):
            names = {"swing-comp", "jazz-run", "neo-soul-keys"}
            choices = [entry for entry in choices if entry["id"][8:] in names]
        elif "organ" in stem:
            choices = [entry for entry in choices if "offbeat-organ" in entry["id"]]
        elif "vibra" in stem:
            choices = [entry for entry in choices if "vibraphone-drops" in entry["id"]]
        elif re.search(r"harpsichord|ping|pluck", stem):
            choices = [entry for entry in choices if "glass-arpeggio" in entry["id"]]
    elif group == "bass":
        if re.search(r"acid|squelch", stem):
            choices = [entry for entry in choices if entry["id"] in {
                "factory-acid-turn", "factory-octave-engine"}]
        elif "subbass" in stem:
            choices = [entry for entry in choices if "dub-space" in entry["id"]]
    elif group == "lead" and "trance" in stem:
        choices = [entry for entry in choices if entry["id"] in {
            "factory-trance-ladder", "factory-minor-hook"}]
    elif group == "percussion":
        if "cymbal" in stem:
            choices = [entry for entry in choices if "metallic-sparks" in entry["id"]]
        elif "conga" in stem:
            choices = [entry for entry in choices if "hand-drum-dialogue" in entry["id"]]
    if group == "sustained":
        keyword = next((word for word in ("sax", "brass", "flute", "string")
                        if word in stem), None)
        if keyword:
            choices = [entry for entry in choices if keyword in entry["id"]]
    seed = hashlib.sha256(stem.encode()).digest()
    return choices[int.from_bytes(seed[:4], "big") % len(choices)]["id"]


def main():
    changed = set(subprocess.check_output(
        ["git", "diff", "--name-only", "--", str(PRESETS)],
        cwd=ROOT, text=True).splitlines())
    available = specs()
    PATTERNS.mkdir(parents=True, exist_ok=True)
    for entry in available:
        path = PATTERNS / (entry["id"] + ".cyclepattern")
        stored = {key: value for key, value in entry.items() if key != "family"}
        path.write_text(json.dumps(stored, indent=2) + "\n")

    migrated = 0
    for path in sorted(PRESETS.glob("*.cyclegraph")):
        relative = str(path.relative_to(ROOT))
        if relative in changed:
            continue
        text = path.read_text()
        graph = json.loads(text)
        presentation = graph.get("presetPresentation", {})
        if "sequence" not in presentation or "patternId" in presentation:
            continue
        pattern_id = select_id(path.stem, graph, available)
        marker = re.search(r'"sequence"\s*:\s*\{', text)
        assert marker
        start = marker.start()
        object_start = marker.end() - 1
        _, consumed = json.JSONDecoder().raw_decode(text[object_start:])
        replacement = '"patternId": ' + json.dumps(pattern_id)
        updated = text[:start] + replacement + text[object_start + consumed:]
        assert json.loads(updated)["presetPresentation"]["patternId"] == pattern_id
        path.write_text(updated)
        migrated += 1
    print(f"{len(available)} distinct patterns; {migrated} presets migrated; "
          f"{len(changed)} modified presets preserved")


if __name__ == "__main__":
    main()
