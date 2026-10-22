#!/usr/bin/env python3
"""Author and audition one compact doubled-reed source/filter saxophone core."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from cycle_automation_support.session import command
from cycle_automation_support.identity import process_identity
from cycle_reference_study import file_digest
from cycle_saxophone_anchor_model import VOICE_SECONDS
from cycle_saxophone_reed_layout import apply_spectral_reed_layout
from cycle_saxophone_reed_color import color_model, color_resources
from cycle_saxophone_reed_model import BODY_RECIPE, grain_resources, modal_impulse, reed_model, sequence, volume_model
from cycle_saxophone_reed_verification import verify_reed
from cycle_study_artifacts import write


ROOT = Path(__file__).resolve().parents[1]


def run(output, impulse):
    output.mkdir(parents=True, exist_ok=False)
    if impulse is None:
        impulse = output / "body.wav"
        modal_impulse(impulse)
    write(output / "body-recipe.json", {**BODY_RECIPE, "importedPath": str(impulse),
        "importedSha256": file_digest(impulse), "recipeUsed": impulse == output / "body.wav"})
    socket = os.environ["CYCLE_AGENT_SESSION"]

    def invoke(payload):
        response = command(socket, payload, timeout=120, require_ok=False)
        with (output / "commands.jsonl").open("a") as log:
            log.write(json.dumps({"request": payload, "response": response}) + "\n")
        if not response.get("ok") or not response.get("result", {}).get("ok"):
            raise RuntimeError(str(response))
        return response["result"].get("data", {})

    def export(name):
        path = output / name
        invoke({"command": "exportGraph", "path": str(path)})
        return json.loads(path.read_text())

    def inspect(node):
        return invoke({"command": "inspectNodeControls", "nodeId": node})

    def set_parameter(node, parameter, value):
        invoke({"command": "setNodeParameter", "nodeId": node, "parameterId": parameter,
                "value": str(int(value)) if isinstance(value, bool) else str(value)})

    def physical(node, kind, parameter, value):
        stored = invoke({"command": "convertSynthesisParameter", "typeId": kind,
            "parameterId": parameter, "direction": "toStored", "value": value})["storedValue"]
        set_parameter(node, parameter, stored)

    capabilities = invoke({"command": "inspectSynthesisCapabilities"})
    if capabilities.get("impulseResponseImport", {}).get("command") != "importImpulseResponse":
        raise RuntimeError("Build an app exposing semantic impulse-response import")

    invoke({"command": "openGraph", "path": str(ROOT / "cycle-v2/content/presets/saxophone-3.cyclegraph"),
            "includeSnapshot": False})
    factory = export("factory-source.cyclegraph")
    for node in factory["nodes"]:
        if node["id"] not in ("voice", "globalInput", "output", "voiceOutput"):
            invoke({"command": "deleteNode", "nodeId": node["id"]})
    for guide in factory["guides"]:
        invoke({"command": "deleteGuideCurve", "guideId": guide["id"]})
    ids = {role: invoke({"command": "addNode", "kind": kind, "x": 0, "y": 0})["nodeId"]
        for role, kind in (("controls", "modulationTriple"), ("reed", "trilinearMesh"),
            ("fft", "fft"), ("color", "trilinearMesh"), ("colorMultiply", "multiply"), ("ifft", "ifft"),
            ("body", "impulseResponse"), ("volume", "envelope"),
            ("volumeMultiply", "multiply"), ("breathMultiply", "multiply"),
            ("pressure", "modulationSource"), ("instability", "modulationSource"))}
    settings = {"voice": {"pitch": 0, "octave": -1, "oversampling": "2x",
            "controlInterval": "64", "envelopeModulation": "live"},
        "output": {"gain": .5},
        ids["controls"]: {"yellowSource": "voiceTime", "redSource": "modWheel",
            "blueSource": "midiCC", "blueController": 2},
        ids["instability"]: {"source": "midiCC", "controller": 3},
        ids["pressure"]: {"source": "midiCC", "controller": 2},
        ids["reed"]: {"signalType": "time", "polarity": "bipolar", "range": .5},
        ids["color"]: {"signalType": "spectralMagnitude", "polarity": "bipolar", "range": .5},
        ids["fft"]: {"cycleFrames": 2048, "mode": "cycle"},
        ids["ifft"]: {"cycleFrames": 2048, "mode": "cyclic"},
        ids["body"]: {"processingScope": "voice"},
        ids["volume"]: {"purpose": "volume", "logarithmic": False, "declick": True}}
    for node, values in settings.items():
        for parameter, value in values.items():
            set_parameter(node, parameter, value)
    physical("voice", "voiceContext", "voiceLength", VOICE_SECONDS)
    physical(ids["reed"], "trilinearMesh", "gain", 7.0)
    physical(ids["color"], "trilinearMesh", "gain", 1.0)
    physical(ids["body"], "impulseResponse", "post", -20)
    physical(ids["body"], "impulseResponse", "highPass", 20)
    resources = grain_resources(factory, ids["reed"])
    graph = export("configured-nodes.cyclegraph")
    graph["guides"] = resources["guides"]
    graph["presetPresentation"].pop("preview", None)
    edges = [("globalInput", "time", "output", "time"),
        ("voice", "context", "reed", "context"),
        ("controls", "modulation", "voice", "modulation"),
        ("instability", "value", "reed", "red"),
        ("reed", "out", "fft", "time"), ("voice", "context", "color", "context"),
        ("fft", "mag", "colorMultiply", "left"), ("color", "out", "colorMultiply", "right"),
        ("colorMultiply", "out", "ifft", "mag"), ("fft", "phase", "ifft", "phase"),
        ("ifft", "time", "body", "time"), ("body", "time", "breathMultiply", "left"),
        ("pressure", "value", "breathMultiply", "right"),
        ("breathMultiply", "out", "volumeMultiply", "left"),
        ("volume", "env", "volumeMultiply", "right"), ("volumeMultiply", "out", "voiceOutput", "time")]
    graph["edges"] = [{"sourceNodeId": ids.get(source, source), "sourcePortId": source_port,
        "destNodeId": ids.get(dest, dest), "destPortId": dest_port,
        "connectionKind": "configurationAttachment" if dest_port == "modulation" else "signal",
        "attachmentType": "modulationTriple" if dest_port == "modulation" else "none"}
        for source, source_port, dest, dest_port in edges]
    write(output / "layout.json", apply_spectral_reed_layout(graph, ids))
    topology = output / "topology.cyclegraph"
    write(topology, graph)
    invoke({"command": "openGraph", "path": str(topology), "includeSnapshot": False})
    color_controls = invoke({"command": "inspectNodeControls", "nodeId": ids["color"],
        "midiNote": 50, "cycleFrames": 2048})
    write(output / "color-coordinates.json", color_controls["spectralAuthoring"])
    color_guides = color_resources(resources["guides"][0], color_controls)
    edits = [reed_model(inspect(ids["reed"])), volume_model(inspect(ids["volume"])), color_model(color_controls)]
    invoke({"command": "applyNodeEdits", "expectedDocumentRevision": inspect("voice")["documentRevision"],
            "edits": edits})
    guided = export("authored-shapes.cyclegraph")
    guided["guides"].extend(color_guides["guides"])
    guided["guideAssignments"] = resources["guideAssignments"] + color_guides["guideAssignments"]
    write(output / "guided.cyclegraph", guided)
    invoke({"command": "openGraph", "path": str(output / "guided.cyclegraph"), "includeSnapshot": False})
    imported = invoke({"command": "importImpulseResponse", "nodeId": ids["body"],
            "path": str(impulse), "mode": "modelled"})
    explanations = {
        "reed": "Two narrow normalized reed pulses per oscillator cycle. Voice octave -1 restores nominal pitch. "
            "The even bins of this lower oscillator are the played note's full integer harmonic series. "
            "Sparse Guide rails delay and weaken the second closure at onset, settling in 120 ms. "
            "CC3 retains alternating closure. Grain is factory component-guide noise on the closing edges. "
            "Waveform stays visibly normalized; gain controls listening level.",
        "color": "Multiplicative spectral color: one simple Guide draws the quiet high-frequency attenuation. "
            "CC2 fades that attenuation to neutral at full breath. The spectrum comes from the pulse FFT; "
            "multiplication preserves its absent half-harmonics and passes its phase to IFFT. No additive filling.",
        "body": "One shared fixed-Hz body impulse response, with broad lower and upper resonances. "
            "Convolution multiplies the reed spectrum, preserving absent odd bins. No additive harmonic filling. "
            "This is a compact design hypothesis, not a uniquely identified acoustic bore. Authored for 48 kHz.",
        "volume": "CC1 chooses a faster tongue or slower breath attack. CC2 also raises amplitude. "
            "Sustain and release use the ordinary live voice envelope.",
        "instability": "CC3 adds persistent asymmetry between the two reed closures; zero restores settled tonal symmetry."}
    explanations["pressure"] = "CC2 breath gain, separate from normalized reed geometry. Zero breath silences the instrument."
    for role, explanation in explanations.items():
        invoke({"command": "setExplanation", "scope": "node", "nodeId": ids[role], "text": explanation})
    invoke({"command": "setExplanation", "scope": "preset", "text":
        "Compact pulse/spectrum/body saxophone study. CC2 breath and spectral brightness; CC1 tongue/breath attack; CC3 alternating closure. "
        "Two narrow pulses, multiplicative spectral color and one shared body response across notes. Play the embedded register/pressure phrase. "
        "Development prototype: register realism, vibrato and fluid connected articulation remain to be refined."})
    invoke({"command": "setPresetSequence", "sequence": sequence()})
    preset = output / "reed-saxophone.cyclegraph"
    invoke({"command": "saveGraph", "path": str(preset)})
    invoke({"command": "openGraph", "path": str(preset), "includeSnapshot": False})
    results = {"schema": "cycle-saxophone-reed-study.v1", "preset": str(preset),
        "presetSha256": file_digest(preset), "impulse": imported,
        "impulseSha256": file_digest(impulse), "ids": ids, "renders": [], "ok": False,
        "acousticTasteVerified": False}
    engine = process_identity(int(os.environ["CYCLE_APP_PID"]))
    results["engine"] = {"identity": engine, "sha256": file_digest(Path(engine["executable"]))}
    write(output / "results.json", results)
    for note in (48, 62, 72):
        for pressure in (32, 127):
            path = output / f"reed-{note}-{pressure}.wav"
            capture = invoke({"command": "captureAudio", "sampleRate": 48000, "blockSize": 256,
                "channels": 2, "randomSeed": 1729, "outputGain": 1, "voiceDurationSeconds": VOICE_SECONDS,
                "note": note, "velocity": 1, "noteDurationMs": 1800, "durationMs": 2300,
                "events": [{"type": "controller", "controller": cc, "value": value, "timeMs": 0}
                    for cc, value in ((1, 0), (2, pressure), (3, 0))], "path": str(path)})
            results["renders"].append({"note": note, "cc2": pressure, "path": str(path),
                "sha256": file_digest(path), "capture": capture})
            write(output / "results.json", results)
            print(path.name, flush=True)
    schedule = invoke({"command": "exportPatternSchedule", "sequence": sequence(), "channel": 1})
    write(output / "phrase-schedule.json", schedule)
    phrase = output / "phrase.wav"
    results["demonstration"] = invoke({"command": "captureAudio", "sampleRate": 48000,
        "blockSize": 256, "channels": 2, "randomSeed": 1729, "outputGain": 1,
        "voiceDurationSeconds": VOICE_SECONDS, "durationMs": 14000,
        "events": schedule["events"], "path": str(phrase)})
    verify_reed(output, invoke)
    results.update(ok=True, verification="verification.json", demonstrationSha256=file_digest(phrase))
    write(output / "results.json", results)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--impulse", type=Path, help="Optional alternative body response; default is the two-mode recipe")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.in_session:
        extra = ["--impulse", str(args.impulse.resolve())] if args.impulse else []
        coordinator = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
        if args.no_build:
            coordinator.append("--no-build")
        return subprocess.run([*coordinator, "--", sys.executable, str(Path(__file__).resolve()),
            "--in-session", "--output-dir", str(args.output_dir.resolve()), *extra], cwd=ROOT, check=False).returncode
    run(args.output_dir.resolve(), args.impulse.resolve() if args.impulse else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
