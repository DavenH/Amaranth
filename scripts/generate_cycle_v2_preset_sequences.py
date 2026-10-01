#!/usr/bin/env python3
"""Fill missing factory preview phrases without replacing hand edited sequences."""

import hashlib
import json
import pathlib
import re
import sys


PRESETS = pathlib.Path(__file__).resolve().parents[1] / "cycle-v2/content/presets"


def family(name, voice_length):
    if re.search(r"bass|ebass|subbass|acidic|acid-stab|squelch|didge", name):
        return "bass"
    if re.search(r"drum|conga|tabla|cymbal|crash|kicker|stomper", name):
        return "percussion"
    if re.search(r"pad|astral|spherical|mysterie|warmth|ambi|storm|buddha|cosmo|mellifluous|omnius|ooh|tanpura|gondwana|mellow|rise-and-shine|coral|esurience|vigil|divining", name):
        return "pad"
    if re.search(r"lead|saw|pwm|square|stab|syncro|sequence|juno|fuzz|pierce|pricky|snarky|trance", name):
        return "lead"
    if re.search(r"horn|brass|sax|trombone|reed|flute|trumpet|mouth-harp|cello|string|sitar", name):
        return "sustained"
    if re.search(r"pluck|guitar|harpsichord|ping|solenoid|spank|electric|piano|keys|vibra|organ|rotary|whurl", name):
        return "keys"
    return "pad" if voice_length > 0.72 else "keys"


def phrase(name, graph):
    seed = int.from_bytes(hashlib.sha256(name.encode()).digest()[:4], "big")
    voice = next((node for node in graph["nodes"] if node["kind"] == "voiceContext"), None)
    length = voice["parameters"].get("voiceLength", 0.5) if voice else 0.5
    kind = family(name, length)
    root = {
        "bass": 36, "percussion": 48, "pad": 48,
        "lead": 60, "sustained": 55, "keys": 57,
    }[kind] + (seed % 3) * 2
    mode = [0, 2, 3, 5, 7, 10, 12] if seed % 4 == 0 else [0, 2, 4, 5, 7, 9, 12]
    notes = []

    def add(pitch, start, duration, velocity):
        notes.append(dict(pitch=pitch, velocity=velocity,
                          startSeconds=round(start, 3),
                          durationSeconds=round(duration, 3)))

    if kind == "pad":
        duration = 10.0
        progression = [0, 5, 3] if seed % 2 else [0, 3, 4]
        for index, degree in enumerate(progression):
            start = index * 3.0
            chord = [0, mode[2], mode[4]]
            for voice_index, interval in enumerate(chord):
                add(root + degree + interval, start + voice_index * 0.12,
                    2.9 - voice_index * 0.1, 65 + (seed + index + voice_index) % 15)
    elif kind == "bass":
        duration = 4.0
        rhythm = [0, 0.5, 0.75, 1.5, 2, 2.5, 3, 3.5]
        degrees = [0, 0, 4, 0, 3, 0, 4, 2] if seed % 2 else [0, 4, 0, 2, 3, 0, 4, 0]
        for index, (start, degree) in enumerate(zip(rhythm, degrees)):
            add(root + mode[degree], start, 0.25 if index % 3 else 0.4,
                82 + (seed + index * 7) % 27)
    elif kind == "percussion":
        duration = 4.0
        for index in range(12):
            step = index * 0.3 + (0.1 if index % 3 == 2 else 0.0)
            add(root + [0, 5, 7, 3][(index + seed) % 4], step, 0.13,
                70 + (index * 11 + seed) % 38)
    elif kind == "lead":
        duration = 5.0
        contour = [0, 2, 4, 2, 5, 4, 3, 1, 0]
        if seed % 2:
            contour = [0, 3, 4, 6, 4, 2, 3, 1, 0]
        for index, degree in enumerate(contour):
            start = index * 0.5 + (0.125 if index % 4 == 3 else 0.0)
            add(root + mode[degree], start, 0.32 if index % 4 else 0.57,
                75 + (seed + index * 5) % 28)
    elif kind == "sustained":
        duration = 6.0
        for index, degree in enumerate([0, 2, 4, 3, 2]):
            add(root + mode[degree], index * 1.15, 0.98,
                68 + (seed + index * 3) % 22)
    else:
        duration = 4.0
        degrees = [0, 2, 4, 6, 4, 2, 3, 1]
        for index, degree in enumerate(degrees):
            add(root + mode[degree], index * 0.5, 0.36 if index % 2 else 0.43,
                70 + (seed + index * 9) % 24)

    controls = []
    if "modWheel" in str(graph["nodes"]):
        values = [12, 48, 91, 34, 76, 18] if kind == "pad" else [8, 21, 84, 61, 16]
        for index, value in enumerate(values):
            controls.append(dict(controller=1, value=value,
                                 timeSeconds=round(duration * index / len(values), 3)))
    return dict(durationSeconds=duration, notes=notes, controls=controls), kind


def insert_sequence(text, sequence):
    match = re.search(r'"presetPresentation"\s*:\s*\{', text)
    if match is None:
        insertion = '\n    "presetPresentation": {\n        "version": 1,\n        "sequence": ' + format_sequence(sequence) + '\n    }\n'
        return text.rstrip()[:-1].rstrip() + ',' + insertion + '}\n'
    start = match.end() - 1
    _, consumed = json.JSONDecoder().raw_decode(text[start:])
    end = start + consumed - 1
    body = text[start + 1:end].rstrip()
    comma = ',' if body else ''
    encoded = format_sequence(sequence)
    return text[:end].rstrip() + comma + '\n        "sequence": ' + encoded + '\n    ' + text[end:]


def format_sequence(sequence):
    lines = [
        '{',
        f'    "durationSeconds": {json.dumps(sequence["durationSeconds"])},',
        '    "notes": [',
    ]
    lines.extend('        ' + json.dumps(note) + (',' if index + 1 < len(sequence['notes']) else '')
                 for index, note in enumerate(sequence['notes']))
    lines.extend(['    ],', '    "controls": ['])
    lines.extend('        ' + json.dumps(control) + (',' if index + 1 < len(sequence['controls']) else '')
                 for index, control in enumerate(sequence['controls']))
    lines.extend(['    ]', '}'])
    return '\n        '.join(lines)


def compact_generated_sequence(text, generated):
    presentation = re.search(r'"presetPresentation"\s*:\s*\{', text)
    if presentation is None:
        return text
    match = re.search(r'"sequence"\s*:\s*\{', text[presentation.end():])
    if match is None:
        return text
    start = presentation.end() + match.end() - 1
    existing, consumed = json.JSONDecoder().raw_decode(text[start:])
    if existing != generated:
        return text
    return text[:start] + format_sequence(generated) + text[start + consumed:]


def main():
    counts = {}
    compact_generated = '--compact-generated' in sys.argv[1:]
    for path in sorted(PRESETS.glob('*.cyclegraph')):
        text = path.read_text()
        graph = json.loads(text)
        sequence, kind = phrase(path.stem, graph)
        if graph.get('presetPresentation', {}).get('sequence') is not None:
            if compact_generated:
                result = compact_generated_sequence(text, sequence)
                if result != text:
                    assert json.loads(result)['presetPresentation']['sequence'] == sequence
                    path.write_text(result)
            continue
        result = insert_sequence(text, sequence)
        assert json.loads(result)['presetPresentation']['sequence'] == sequence
        path.write_text(result)
        counts[kind] = counts.get(kind, 0) + 1
    print(counts)


if __name__ == '__main__':
    main()
