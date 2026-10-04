#!/usr/bin/env python3
"""Curate Cycle V2 preset browser tags without rewriting graph JSON."""

import collections
import json
import pathlib
import re


ROOT = pathlib.Path(__file__).resolve().parents[1]
PRESETS = ROOT / "cycle-v2/content/presets"
PATTERNS = ROOT / "cycle-v2/content/patterns"

FAMILY_RULES = (
    ("Percussion", r"drum|conga|cymbal|crash|tabla|melodruma"),
    ("Vocal", r"ooh|aah|talker|oh-yeah"),
    ("Wind", r"flute|reed|didge|mouth-harp"),
    ("Brass", r"sax|horn|brass|trumpet|trombone"),
    ("Strings", r"cello|string|tanpura"),
    ("Guitar", r"guitar|pick-chorus|^acoustic"),
    ("Bass", r"bass|ebass|^acid|alkali|squelch|kicker|stomper"),
    ("Pluck", r"pluck|^ping|pinger"),
    ("Pad", r"pad|ambi|astral|coral|divining|mellow|mysterie|omnius|"
            r"spherical|^storm|warmth|honerism|nordic|psi-storm"),
    ("Keys", r"keys|piano|organ|harpsichord|^electric|^green-key"),
    ("Lead", r"lead|^filter-|^pwm|^saw|juno|^sequence|^synpluck"),
    ("Texture", r"aliens|blinding|brane|cerebrate|esurience|^buddha|"
            r"cosmo|infinite-loop|^time$"),
)

TRAIT_RULES = (
    ("Acid", r"acid|alkali|squelch"),
    ("Ambient", r"pad|ambi|astral|spherical|storm|mysterie|omnius|"
            r"coral|nordic|warmth|psi-storm|mellifluous"),
    ("Analog", r"pwm|juno|saw|square|syn-|synth"),
    ("Bright", r"bright|high-|shine|pearl|pierce|fire"),
    ("Distorted", r"fuzz|thrash|violence|noisy|crash|spank"),
    ("Metallic", r"bell|vibra|ping|solenoid|cymbal"),
    ("Soft", r"mellow|warmth|mellifluous|soft|coral"),
    ("Stab", r"stab|trance"),
    ("Rhythmic", r"loop|sequence|syncro|drum|conga|tabla|melodruma"),
)

PATTERN_FAMILIES = {
    "Bass": "Bass",
    "Lead": "Lead",
    "Pad": "Pad",
    "Keys": "Keys",
    "Rhythm": "Percussion",
    "Sustained": "Sustained",
    "Other": "Texture",
}

OVERRIDES = {
    "24-h": "Keys",
    "bass-guitar": "Bass",
    "buddha": "Texture",
    "high-synth-2": "Lead",
    "organ-3": "Keys",
    "sitar-pad": "Pad",
    "sitar-pad-2": "Pad",
    "solo-string": "Strings",
    "synsax": "Brass",
    "synsax-2": "Brass",
    "tanpura-pad-4": "Pad",
    "thick-square": "Bass",
    "time": "Texture",
    "vibra-2": "Keys",
}


def pattern_families():
    result = {}
    for path in PATTERNS.glob("*.cyclepattern"):
        pattern = json.loads(path.read_text())
        tag = pattern.get("tag") or ""
        result[pattern["id"]] = PATTERN_FAMILIES.get(tag, "Texture")
    return result


def tags_for(name, presentation, families):
    lowered = name.lower()
    family = OVERRIDES.get(lowered)
    if family is None:
        for candidate, expression in FAMILY_RULES:
            if re.search(expression, lowered):
                family = candidate
                break
    if family is None:
        family = families.get(presentation.get("patternId"), "Texture")

    tags = [family]
    if family in {"Wind", "Brass", "Strings", "Sustained"}:
        tags.append("Sustained")
    if family == "Guitar" or "pluck" in lowered or "pick" in lowered:
        tags.append("Pluck")
    if family == "Percussion":
        tags.append("Rhythmic")
    for tag, expression in TRAIT_RULES:
        if re.search(expression, lowered) and tag not in tags:
            tags.append(tag)
    return tags


def main():
    families = pattern_families()
    counts = collections.Counter()
    changed = 0
    for path in sorted(PRESETS.glob("*.cyclegraph")):
        text = path.read_text()
        root = json.loads(text)
        presentation = root.get("presetPresentation", {})
        tags = tags_for(path.stem, presentation, families)
        counts.update(tags)
        start = re.search(
            r'("presetPresentation": \{\n[ \t]+"version": 1,\n)', text)
        if start is None:
            raise ValueError(f"Missing preset metadata header: {path}")
        after = text[start.end():]
        tag_line = re.match(r'[ \t]+"tags": \[[^\n]*\],\n', after)
        if tag_line:
            after = after[tag_line.end():]
        indent = " " * 8
        inserted = f'{indent}"tags": {json.dumps(tags)},\n'
        updated = text[:start.end()] + inserted + after
        if updated != text:
            path.write_text(updated)
            changed += 1
    print(f"Tagged {len(list(PRESETS.glob('*.cyclegraph')))} presets; "
          f"updated {changed}: {dict(counts.most_common())}")


if __name__ == "__main__":
    main()
