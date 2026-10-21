"""Summarize exported effective pitch-path frequencies without detecting pitch."""

import json
import math
from pathlib import Path
from statistics import median, quantiles

from cycle_audio_diff import midi_frequency
from cycle_performance_suite import identifier
from cycle_reference_study import finite_number


MAX_FRAMES = 100000
EVIDENCE = "Effective frequencies rebuilt from the current pitch path; not raw tracker observations"
STATISTICS = ("medianHz", "medianCents", "lowCents", "highCents", "spreadCents")


def pitch_evidence(analysis, start, end, midi_note):
    """Select [start, end) in original elapsed time; pitch labels remain inferred."""
    if not isinstance(analysis, dict):
        raise ValueError("Pitch analysis must be an exported object")
    duration = finite_number(analysis.get("durationSeconds"), "analysis duration")
    start = finite_number(start, "interval start", 0, duration)
    end = finite_number(end, "interval end", 0, duration)
    if end <= start or end - start > 120:
        raise ValueError("Pitch interval must be positive and at most 120 seconds")
    note = finite_number(midi_note, "inferred standard MIDI note", 0, 127)
    rate = finite_number(analysis.get("sampleRate"), "sample rate", 8000, 192000)
    insecurity = finite_number(analysis.get("trackingInsecurity"), "tracking insecurity")
    flags = {key: analysis.get(key) for key in ("lowConfidence", "trackingUsedFallback")}
    if any(type(value) is not bool for value in flags.values()):
        raise ValueError("Exported pitch confidence flags must be explicit booleans")
    frames = analysis.get("pitchFrames")
    if not isinstance(frames, list) or len(frames) > MAX_FRAMES:
        raise ValueError("Pitch analysis requires at most 100000 exported frames")
    state = analysis.get("pitchState")
    if not isinstance(state, str) or not state:
        raise ValueError("Exported effective pitch state must be explicit")
    selected, previous = [], -1
    for frame in frames:
        if not isinstance(frame, dict):
            raise ValueError("Each exported pitch frame must be an object")
        time = finite_number(frame.get("timeSeconds"), "frame time", 0, duration)
        frequency = finite_number(frame.get("frequencyHz"), "frame frequency", 0, rate / 2)
        if time <= previous or frequency <= 0:
            raise ValueError("Pitch frames require increasing times and positive frequencies")
        previous = time
        if start <= time < end:
            cents = 1200 * (math.log2(frequency) - math.log2(midi_frequency(note)))
            selected.append({"timeSeconds": time, "frequencyHz": frequency, "cents": cents})
    result = {"available": False, "reason": "Fewer than three effective pitch frames in the interval",
              "frameCount": len(selected), "startSeconds": start, "endSeconds": end,
              "standardMidiInferred": note, "trackingInsecurity": insecurity, **flags,
              "pitchState": state, "evidence": EVIDENCE, "frames": selected,
              "percentilePolicy": "Unweighted frame percentiles, inclusive linear interpolation; 5–95%",
              **dict.fromkeys(STATISTICS)}
    if any(flags.values()):
        result["reason"] = "Exported tracking is low confidence or used fallback; comparison suppressed"
    elif len(selected) >= 3:
        cents = [frame["cents"] for frame in selected]
        percentiles = quantiles(cents, n=20, method="inclusive")
        low, high = percentiles[0], percentiles[-1]
        result.update(available=True, reason="Effective pitch-path evidence with inferred nominal pitch",
                      medianHz=median(frame["frequencyHz"] for frame in selected),
                      medianCents=median(cents), lowCents=low, highCents=high, spreadCents=high - low)
    return result


def plot_pitch_evidence(cases, output):
    """Save cases, including caller provenance, and plot their unchanged timelines."""
    output = Path(output)
    if output.exists():
        raise ValueError("Pitch output already exists; choose a fresh directory")
    if not isinstance(cases, list) or not 1 <= len(cases) <= 64:
        raise ValueError("Pitch plots require between 1 and 64 cases")
    identifiers, total = set(), 0
    for case in cases:
        if not isinstance(case, dict):
            raise ValueError("Pitch plot cases must be objects")
        key = identifier(case.get("id"), "pitch case id")
        note = finite_number(case.get("standardMidiInferred"), "inferred standard MIDI note", 0, 127)
        if key in identifiers or case.get("partition") not in ("objective", "withheld"):
            raise ValueError("Pitch case IDs must be unique and partitions explicit")
        identifiers.add(key)
        for side in ("source", "baseline", "candidate"):
            evidence = case.get(side)
            if (not isinstance(evidence, dict) or type(evidence.get("available")) is not bool
                    or evidence.get("standardMidiInferred") != note
                    or not isinstance(evidence.get("frames"), list)
                    or evidence.get("frameCount") != len(evidence["frames"])):
                raise ValueError("Pitch case requires matching source/baseline/candidate summaries")
            total += len(evidence["frames"])
    if total > 500000:
        raise ValueError("Pitch plot collection exceeds 500000 selected frames")
    encoded = json.dumps({"schema": "cycle-reference-pitch-evidence.v1", "evidence": EVIDENCE,
                          "cases": cases}, indent=2, allow_nan=False)
    import matplotlib
    matplotlib.use("Agg")
    from matplotlib import pyplot

    output.mkdir(parents=True, exist_ok=False)
    report = json.loads(encoded)
    report["plots"] = {}
    for case in cases:
        figure, axis = pyplot.subplots(figsize=(10, 3.5))
        try:
            for side, colour in (("source", "#303030"), ("baseline", "#2869a6"), ("candidate", "#ce662d")):
                evidence = case[side]
                label = side + ": " + (f"effective pitch, tracking insecurity {evidence['trackingInsecurity']:g}"
                                       if evidence["available"] else evidence["reason"])
                frames = evidence["frames"] if evidence["available"] else []
                axis.plot([frame["timeSeconds"] for frame in frames], [frame["cents"] for frame in frames],
                          color=colour, label=label, linewidth=1)
            axis.axhline(0, color="#888888", linewidth=.7, linestyle=":")
            axis.set_title(case["id"] + " — " + case["partition"])
            axis.set_xlabel("Original elapsed recording/render time (seconds); no alignment")
            axis.set_ylabel(f"Cents from inferred standard MIDI {case['standardMidiInferred']:g}")
            axis.grid(alpha=.2)
            axis.legend(fontsize=8)
            figure.text(.01, .01, "Effective pitch-path evidence; inferred pitch, not raw detector observations. "
                        "Flagged traces suppressed; confidence flags retained in JSON.", fontsize=8)
            figure.tight_layout(rect=(0, .08, 1, 1))
            report["plots"][case["id"]] = {}
            for extension in ("png", "svg"):
                name = case["id"] + "." + extension
                figure.savefig(output / name, dpi=140)
                report["plots"][case["id"]][extension] = name
        finally:
            pyplot.close(figure)
    (output / "pitch-evidence.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    return report
