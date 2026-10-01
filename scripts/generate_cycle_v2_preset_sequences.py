#!/usr/bin/env python3
"""Fill missing factory preview phrases without replacing hand edited sequences."""

import hashlib
import json
import pathlib
import random
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
    rng = random.Random(seed)
    voice = next((node for node in graph["nodes"] if node["kind"] == "voiceContext"), None)
    length = voice["parameters"].get("voiceLength", 0.5) if voice else 0.5
    kind = family(name, length)
    root = {
        "bass": 36, "percussion": 48, "pad": 48,
        "lead": 60, "sustained": 55, "keys": 57,
    }[kind] + rng.choice([-2, 0, 2, 4])
    mode = rng.choice([[0, 2, 3, 5, 7, 10, 12],
                       [0, 2, 4, 5, 7, 9, 12],
                       [0, 3, 5, 7, 8, 10, 12]])
    notes = []
    attached = {edge["sourceNodeId"] for edge in graph["edges"]
                if edge.get("connectionKind") == "configurationAttachment"
                and edge.get("attachmentType") == "modulationTriple"}
    sources = [(node["parameters"].get(axis + "Source", default),
                node["parameters"].get(axis + "Controller", 1))
               for node in graph["nodes"] if node["kind"] == "modulationTriple"
               and node["id"] in attached
               for axis, default in (("blue", "modWheel"),
                                     ("red", "keyScale"),
                                     ("yellow", "voiceTime"))]
    controller = next((1 if source == "modWheel" else int(number)
                       for source, number in sources
                       if source in ("modWheel", "midiCC")), None)
    velocity_mapped = any(source in ("velocity", "inverseVelocity")
                          for source, _ in sources)

    def add(pitch, start, duration, velocity):
        notes.append(dict(pitch=pitch, velocity=velocity,
                          startSeconds=round(start, 3),
                          durationSeconds=round(duration, 3)))

    if kind == "pad":
        duration = 10.0
        progression = rng.choice([[0, 5, 3], [0, 3, 4], [0, 4, 5],
                                  [0, 3, 1], [0, 5, 4]])
        for index, degree in enumerate(progression):
            start = index * 3.0
            chord = rng.choice([[0, mode[2], mode[4]],
                                [0, mode[3], mode[5]],
                                [0, mode[2], mode[5]]])
            for voice_index, interval in enumerate(chord):
                add(root + degree + interval, start + voice_index * 0.16,
                    rng.choice([2.5, 2.7, 2.85]),
                    rng.randint(48, 112) if velocity_mapped else rng.randint(65, 86))
    elif kind == "bass":
        duration = 4.0
        rhythm = rng.choice([[0, .5, .75, 1.5, 2, 2.5, 3, 3.5],
                             [0, .375, 1, 1.5, 1.75, 2.5, 3, 3.25],
                             [0, .75, 1, 1.5, 2.25, 2.5, 3.25, 3.5]])
        degrees = rng.choice([[0, 0, 4, 0, 3, 0, 4, 2],
                              [0, 4, 0, 2, 3, 0, 4, 0],
                              [0, 0, 3, 4, 0, 2, 4, 0]])
        for index, (start, degree) in enumerate(zip(rhythm, degrees)):
            add(root + mode[degree], start, rng.choice([.22, .3, .38]),
                rng.randint(51, 120) if velocity_mapped else rng.randint(78, 112))
    elif kind == "percussion":
        duration = 4.0
        rhythm = rng.choice([[0, .3, .6, 1, 1.3, 1.6, 2, 2.3, 2.6, 3, 3.3, 3.6],
                             [0, .25, .75, 1, 1.5, 1.75, 2, 2.5, 2.75, 3, 3.5, 3.75]])
        for index, step in enumerate(rhythm):
            add(root + rng.choice([0, 3, 5, 7]), step, rng.choice([.1, .14, .2]),
                rng.randint(54, 122))
    elif kind == "lead":
        duration = 5.0
        contour = rng.choice([[0, 2, 4, 2, 5, 4, 3, 1, 0],
                              [0, 3, 4, 6, 4, 2, 3, 1, 0],
                              [0, 4, 3, 2, 5, 6, 4, 2, 0],
                              [0, 1, 3, 5, 4, 2, 1, 3, 0]])
        for index, degree in enumerate(contour):
            start = index * .5 + (.125 if index % 4 == rng.randrange(4) else 0)
            add(root + mode[degree], start, rng.choice([.27, .34, .53]),
                rng.randint(53, 119) if velocity_mapped else rng.randint(72, 110))
    elif kind == "sustained":
        duration = 6.0
        for index, degree in enumerate(rng.choice([[0, 2, 4, 3, 2],
                                                    [0, 3, 5, 4, 1],
                                                    [0, 4, 2, 3, 0]])):
            add(root + mode[degree], index * 1.15, rng.choice([.9, 1.02, 1.1]),
                rng.randint(52, 113) if velocity_mapped else rng.randint(68, 94))
    else:
        duration = 4.0
        degrees = rng.choice([[0, 2, 4, 6, 4, 2, 3, 1],
                              [0, 3, 4, 2, 5, 4, 2, 0],
                              [0, 4, 2, 3, 5, 3, 1, 0]])
        for index, degree in enumerate(degrees):
            add(root + mode[degree], index * .5 + (.125 if index % 4 == 3 else 0),
                rng.choice([.25, .34, .42]),
                rng.randint(51, 119) if velocity_mapped else rng.randint(69, 101))

    controls = []
    if controller is not None:
        shape = rng.choice([[0.12, 0.3, 0.82, 0.54, 0.18],
                            [0.18, 0.72, 0.32, 0.91, 0.22],
                            [0.69, 0.24, 0.87, 0.4, 0.72],
                            [0.08, 0.24, 0.52, 0.8, 0.18]])
        depth = rng.uniform(.5, .9)
        centre = rng.uniform(.35, .65)
        for index, level in enumerate(shape):
            value = max(0, min(127, round(127 * (centre + depth * (level - .5)))))
            controls.append(dict(controller=controller, value=value,
                                 timeSeconds=round(duration * index / len(shape), 3)))
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
