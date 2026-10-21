#!/usr/bin/env python3
"""Produce local A/B audition reports from saved Cycle suites and reference studies."""

import argparse
import hashlib
import html
import json
from pathlib import Path
import sys
import wave

from cycle_audition_analysis import DESCRIPTORS, FFT_SIZE, HOP_SIZE, analyze_case
from cycle_audition_study import load_study
from cycle_reference_study import file_digest


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def write_wav(path, channels, rate, gain=1):
    encoded = bytearray()
    for frame in zip(*channels):
        for value in frame:
            scaled = value * gain
            if not -1 <= scaled < 1:
                raise ValueError("Audition export would clip")
            sample = min(8388607, round(scaled * 8388608))
            encoded.extend(sample.to_bytes(3, "little", signed=True))
    with wave.open(str(path), "wb") as target:
        target.setparams((len(channels), 3, rate, len(channels[0]), "NONE", "not compressed"))
        target.writeframes(encoded)


def plot_case(case, destination, pyplot):
    keys = ["rms", "centroidHz", "flatness"]
    if case["pitch"]["used"]:
        keys += ["harmonicBandFraction"]
    figure, axes = pyplot.subplots(len(keys), 1, figsize=(10, 2.0 * len(keys)), sharex=True)
    for axis, key in zip(axes, keys):
        for index, phase in enumerate(case["intervals"]):
            for side, color in (("a", "#2869a6"), ("b", "#ce662d")):
                rows = phase["raw"][side]
                axis.plot([row["timeSeconds"] for row in rows], [row[key] for row in rows],
                          color=color, label=side.upper() + " raw" if index == 0 else None)
                if phase.get("aligned", {}).get("available"):
                    rows = phase["aligned"][side]
                    axis.plot([row["timeSeconds"] for row in rows], [row[key] for row in rows],
                              color=color, linestyle=":", alpha=0.7,
                              label=side.upper() + " fixed lag" if index == 0 else None)
            axis.axvspan(phase["startSeconds"], phase["endSeconds"], alpha=0.04, color="black")
        axis.set_ylabel(key)
        axis.grid(alpha=0.2)
    axes[0].legend(loc="upper right", ncol=2)
    axes[0].set_title(f"{case['id']} — {case['partition']}; raw channel energy")
    axes[-1].set_xlabel("Seconds from selected crop origin (A timeline)")
    figure.tight_layout()
    figure.savefig(destination, dpi=140)
    figure.savefig(destination.with_suffix(".svg"))
    pyplot.close(figure)


def case_html(case):
    escape = html.escape
    players = []
    for label, paths in case["audio"].items():
        players.append(f"<h4>{escape(label)}</h4><div class='pair'>" + "".join(
            f"<div>{side.upper()} <audio controls preload='none' src='{escape(path)}'></audio></div>"
            for side, path in paths.items()) + "</div>")
    rows = []
    for phase in case["intervals"]:
        measures = ", ".join(f"{key}: {value:.6g}" for key, value in phase["raw"]["meanAbsoluteDifference"].items())
        rows.append(f"<tr><td>{escape(phase['id'])} ({escape(phase['role'])})</td>"
                    f"<td>{phase['startSeconds']:g}–{phase['endSeconds']:g} s</td><td>{escape(measures)}</td></tr>")
    sources = json.dumps({side: case["sources"][side] for side in ("a", "b")}, indent=2)
    levels = json.dumps({"raw": case["levels"], "matching": case["rmsMatching"],
                         "alignment": case["alignment"]}, indent=2)
    return (f"<section id='{case['id']}'><h3>{case['id']}</h3><p>{escape(case['pitch']['reason'])}</p>"
            + "".join(players) + f"<img src='{case['id']}.png' alt='Raw descriptor traces for {case['id']}'>"
            + "<table><tr><th>Interval</th><th>Elapsed window</th><th>Raw mean absolute differences</th></tr>"
            + "".join(rows) + "</table><details><summary>Level and fixed-lag policy</summary><pre>"
            + escape(levels) + "</pre></details><details><summary>Exact sources, labels and confidence</summary><pre>"
            + escape(sources) + "</pre></details></section>")


def objective_summary(cases):
    phases = [phase for case in cases if case["partition"] == "objective" for phase in case["intervals"]]
    return {key: sum(values) / len(values) for key in DESCRIPTORS if
            (values := [phase["raw"]["meanAbsoluteDifference"][key] for phase in phases
                        if key in phase["raw"]["meanAbsoluteDifference"]])}


def generate_report(plan, output):
    import matplotlib
    matplotlib.use("Agg")
    from matplotlib import pyplot

    if output.exists():
        raise ValueError("Output directory already exists; choose a fresh directory")
    source_bytes = Path(plan["sourcePath"]).read_bytes()
    if hashlib.sha256(source_bytes).hexdigest() != plan["sourceSha256"]:
        raise ValueError("Audition manifest changed after validation")
    output.mkdir(parents=True, exist_ok=False)
    (output / "study.json").write_bytes(source_bytes)
    report = {"schema": "cycle-audition-report.v1", "status": "running", "study": plan, "cases": [],
              "analysis": {"fftSize": FFT_SIZE, "hopSize": HOP_SIZE,
                           "window": "Hann; final short windows zero padded for spectrum only",
                           "levels": "mean channel energy, not mono mixdown or LUFS",
                           "spectra": "mean channel powers; harmonic bands ±1.5 bins, harmonics 1–16",
                           "summary": "equal weight per raw objective interval; descriptors remain separate",
                           "remainingBandEnergy": "spectral energy outside declared bands; not measured noise/HNR"}}
    try:
        for case in plan["cases"]:
            evidence, channels = analyze_case(case, plan["alignment"])
            evidence["audio"] = {"Raw cropped audio (24-bit PCM)": {}}
            evidence["artifacts"] = {}
            matching = evidence["rmsMatching"]
            if plan["rmsMatch"] and matching["available"]:
                evidence["audio"]["RMS matched, one gain per full crop"] = {}
            for index, side in enumerate(("a", "b")):
                for label, paths in evidence["audio"].items():
                    suffix = "raw" if label.startswith("Raw") else "rms"
                    name = f"{case['id']}-{side}-{suffix}.wav"
                    gain = 1 if suffix == "raw" else matching["gains"][index]
                    write_wav(output / name, channels[index], case["a"]["sampleRate"], gain)
                    paths[side] = name
                    evidence["artifacts"][name] = file_digest(output / name)
            plot_case(evidence, output / f"{case['id']}.png", pyplot)
            for extension in ("png", "svg"):
                name = f"{case['id']}.{extension}"
                evidence["artifacts"][name] = file_digest(output / name)
            report["cases"].append(evidence)
        report.update(status="complete", objectiveSummary=objective_summary(report["cases"]))
        summary = ", ".join(f"{key}: {value:.6g}" for key, value in report["objectiveSummary"].items())
        sections = "<h2>Objective raw differences</h2><p>Equal weight per objective interval: " + html.escape(summary or "No objective intervals") + "</p>" + "".join(f"<h2>{partition.title()} cases</h2>" + "".join(
            case_html(case) for case in report["cases"] if case["partition"] == partition)
            for partition in ("objective", "withheld"))
        title = html.escape(plan["name"])
        page = ("<!doctype html><html lang='en'><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
                f"<title>{title}</title><style>body{{font:15px system-ui;margin:2rem auto;max-width:1100px;padding:0 1rem;"
                "color:#20252b;background:#fafbfc}section{border-top:1px solid #cbd0d5;padding:1rem 0}"
                "img{width:100%;height:auto}.pair{display:flex;gap:2rem;flex-wrap:wrap}audio{vertical-align:middle}"
                "td,th{text-align:left;padding:.5rem;border-bottom:1px solid #ddd}pre{overflow:auto;font-size:12px}"
                "details{margin:1rem 0}</style><body>"
                f"<h1>{title}</h1><p>Raw phase and trajectory evidence; no taste score. Withheld cases are excluded "
                "from objective summaries. RMS matching is channel-energy matching, not perceived loudness. "
                "Playback retains timing; optional fixed lag applies only to explicitly labelled analysis traces.</p>"
                "<p><a href='report.json'>Full numerical evidence and provenance</a> · <a href='study.json'>Input manifest</a> · "
                "<a href='human-preferences.template.json'>Separate human preference template</a></p>"
                + sections + "</body></html>")
        (output / "index.html").write_text(page, encoding="utf-8")
        write_json(output / "report.json", report)
        write_json(output / "human-preferences.template.json", {
            "schema": "cycle-audition-preferences.v1", "reportSha256": file_digest(output / "report.json"),
            "instructions": "Save responses separately; preference is subjective and does not alter similarity evidence",
            "cases": [{"id": case["id"], "artifactSha256": case["artifacts"],
                       "preferred": None, "notes": ""} for case in report["cases"]]})
    except Exception as error:
        report.update(status="failed", error=str(error))
        write_json(output / "report.json", report)
        raise
    return output / "index.html"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("study", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args(argv)
    try:
        plan = load_study(args.study)
        if args.validate_only:
            print(json.dumps(plan, indent=2, allow_nan=False))
        elif args.output is None:
            raise ValueError("--output is required unless --validate-only is used")
        else:
            print(generate_report(plan, args.output.resolve()))
        return 0
    except ImportError:
        parser.exit(1, "Audition plots require matplotlib; install scripts/requirements-auditions.txt in your host environment\n")
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, wave.Error) as error:
        parser.exit(1, f"Audition report failed: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
