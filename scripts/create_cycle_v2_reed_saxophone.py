#!/usr/bin/env python3
"""Author and audition one compact doubled-reed source/filter saxophone core."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from cycle_automation_support.session import Session
from cycle_automation_support.identity import process_identity
from cycle_reference_study import file_digest
from cycle_saxophone_anchor_model import VOICE_SECONDS
from cycle_saxophone_reed_layout import apply_expressive_reed_layout
from cycle_saxophone_reed_color import KEY_NOTES, edge_model, edge_resources
from cycle_saxophone_reed_air import air_model, air_resources
from cycle_saxophone_reed_model import BODY_RECIPE, body_impulse, grain_resources, pitch_model, reed_model, sequence, volume_model
from cycle_saxophone_reed_verification import render_articulations, verify_reed
from cycle_study_artifacts import write


ROOT = Path(__file__).resolve().parents[1]


def run(output, impulse):
    with Session(os.environ["CYCLE_AGENT_SESSION"], timeout=120) as session:
        return author(output, impulse, session)


def author(output, impulse, session):
    output.mkdir(parents=True, exist_ok=False)
    authored_body = impulse is None
    if authored_body:
        impulse = output / "body.wav"
        body_impulse(impulse)
    write(output / "body-recipe.json", {**BODY_RECIPE, "importedPath": str(impulse),
        "importedSha256": file_digest(impulse), "recipeUsed": authored_body})
    def invoke(payload):
        response = session.command(payload, require_ok=False)
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
        for role, kind in (("controls", "modulationTriple"), ("articulationControls", "modulationTriple"),
            ("instability", "modulationSource"),
            ("pitch", "envelope"), ("key", "modulationSource"), ("reed", "trilinearMesh"),
            ("fft", "fft"), ("color", "trilinearMesh"), ("colorMultiply", "multiply"), ("ifft", "ifft"),
            ("air", "trilinearMesh"), ("airAdd", "add"),
            ("body", "impulseResponse"), ("volume", "envelope"),
            ("volumeMultiply", "multiply"), ("breathMultiply", "multiply"),
            ("pressure", "modulationSource"))}
    settings = {"voice": {"pitch": 0, "octave": -1, "oversampling": "1x", "noteMode": "legato",
            "portamento": True, "portamentoSeconds": .030, "controlInterval": "64", "envelopeModulation": "live"},
        "output": {"gain": .5},
        ids["controls"]: {"yellowSource": "voiceTime", "redSource": "midiCC", "redController": 5,
            "blueSource": "midiCC", "blueController": 2},
        ids["articulationControls"]: {"yellowSource": "voiceTime",
            "redSource": "midiCC", "redController": 76, "blueSource": "modWheel"},
        ids["instability"]: {"source": "midiCC", "controller": 3},
        ids["pitch"]: {"purpose": "pitch", "logarithmic": False, "declick": False, "level": 1},
        ids["key"]: {"source": "keyScale"},
        ids["pressure"]: {"source": "midiCC", "controller": 2},
        ids["reed"]: {"signalType": "time", "polarity": "bipolar", "range": .5},
        ids["color"]: {"signalType": "spectralMagnitude", "polarity": "bipolar", "range": .5, "link.red": True},
        ids["air"]: {"signalType": "time", "polarity": "bipolar", "range": .5, "link.blue": True},
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
    physical(ids["air"], "trilinearMesh", "gain", .1)
    physical(ids["body"], "impulseResponse", "post", 1)
    physical(ids["body"], "impulseResponse", "highPass", 20)
    resources = grain_resources(factory, ids["reed"])
    graph = export("configured-nodes.cyclegraph")
    graph["guides"] = resources["guides"]
    graph["presetPresentation"].pop("preview", None)
    edges = [("globalInput", "time", "output", "time"),
        ("controls", "modulation", "voice", "modulation"),
        ("instability", "value", "reed", "red"),
        ("articulationControls", "red", "pitch", "red"),
        ("articulationControls", "blue", "pitch", "blue"), ("pitch", "env", "voice", "pitch"),
        ("reed", "out", "fft", "time"),
        ("key", "value", "color", "red"),
        ("fft", "mag", "colorMultiply", "left"), ("color", "out", "colorMultiply", "right"),
        ("colorMultiply", "out", "ifft", "mag"), ("fft", "phase", "ifft", "phase"),
        ("ifft", "time", "airAdd", "left"),
        ("air", "out", "airAdd", "right"), ("airAdd", "out", "body", "time"),
        ("body", "time", "breathMultiply", "left"),
        ("pressure", "value", "breathMultiply", "right"),
        ("breathMultiply", "out", "volumeMultiply", "left"),
        ("volume", "env", "volumeMultiply", "right"), ("volumeMultiply", "out", "voiceOutput", "time")]
    graph["edges"] = [{"sourceNodeId": ids.get(source, source), "sourcePortId": source_port,
        "destNodeId": ids.get(dest, dest), "destPortId": dest_port,
        "connectionKind": "configurationAttachment" if dest_port == "modulation" else "signal",
        "attachmentType": "modulationTriple" if dest_port == "modulation" else "none"}
        for source, source_port, dest, dest_port in edges]
    write(output / "layout.json", apply_expressive_reed_layout(graph, ids))
    topology = output / "topology.cyclegraph"
    write(topology, graph)
    invoke({"command": "openGraph", "path": str(topology), "includeSnapshot": False})
    coordinates = {}
    for played in KEY_NOTES:
        def spectral(note):
            return invoke({"command": "inspectNodeControls", "nodeId": ids["color"],
                "midiNote": note, "cycleFrames": 2048})
        effective = spectral(played - 12)["spectralAuthoring"]
        coordinates[played] = {"effectiveMidi": played - 12, "positions": effective["positions"],
            "keyPosition": spectral(played)["spectralAuthoring"]["keyPosition"]}
    write(output / "color-coordinates.json", coordinates)
    color_controls = spectral(48)
    color_guides = edge_resources(resources["guides"][-1], color_controls, coordinates)
    air_controls = inspect(ids["air"])
    aspiration = air_resources(color_guides["guides"][-1], air_controls)
    edits = [reed_model(inspect(ids["reed"])), volume_model(inspect(ids["volume"])),
        pitch_model(inspect(ids["pitch"])), air_model(air_controls),
        edge_model(color_controls, coordinates, BODY_RECIPE["excitation"])]
    invoke({"command": "applyNodeEdits", "expectedDocumentRevision": inspect("voice")["documentRevision"],
            "edits": edits})
    guided = export("authored-shapes.cyclegraph")
    guided["guides"].extend(color_guides["guides"] + aspiration["guides"])
    guided["guideAssignments"] = resources["guideAssignments"] + color_guides["guideAssignments"] + aspiration["guideAssignments"]
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
        "color": "A smooth reed edge, shared across notes: CC2 opens its corner from about 3 to 4.5 played harmonics. "
            "Sparse key Guides move the same six landmarks across the register. Multiplication shapes the pulse spectrum "
            "while retaining its phase and missing half-harmonics.",
        "body": "One shared body: +2 dB low warmth, +5 dB at 550 Hz, -8.5 dB near 1.1 kHz, "
            "+5 dB at 2.5 kHz and -14 dB upper damping. "
            "Convolution multiplies the reed spectrum, preserving absent odd bins. No additive harmonic filling. "
            "Physical resonances are preserved across playback rates.",
        "volume": "CC5 moves from tongue to breath attack; quieter CC2 adds reed inertia. "
            "Release is about 90-110 ms. The breath response stays gentle at the top of the controller range.",
        "pitch": "Delayed live vibrato: mod wheel (CC1) depth, CC76 rate (4.5-7.5 Hz). Zero wheel gives straight pitch. "
            "Overlapping notes stay connected with a short 30 ms glide.",
        "air": "Quiet turbulent air joins after the reed edge and passes through the same body. "
            "The Air Guide sets aspiration independently of the closing-edge grain; quiet pressure is relatively breathier.",
        "articulationControls": "CC76 controls vibrato rate and the mod wheel controls depth; both update during a held note.",
        "instability": "CC3 controls alternating reed closure. Use a little for roughness and reserve larger values "
            "for a deliberate effect. Zero restores the settled tonal symmetry.",
        "key": "Key-following reed edge: move one shared shape across the register, without fitting separate note spectra."}
    explanations["pressure"] = "CC2 breath gain, separate from normalized reed geometry. Zero breath silences the instrument."
    if not authored_body:
        explanations["body"] = ("One shared imported body response. Convolution multiplies the reed spectrum, "
            "preserving absent odd bins. No additive harmonic filling. "
            "The retained response is embedded; physical resonances follow the IR playback-rate policy.")
    for role, explanation in explanations.items():
        invoke({"command": "setExplanation", "scope": "node", "nodeId": ids[role], "text": explanation})
    invoke({"command": "setExplanation", "scope": "preset", "text":
        "Expressive solo reed saxophone. CC2 breath/color, mod wheel vibrato depth, CC5 tongue-to-breath attack, "
        "CC76 vibrato rate and CC3 reed roughness. Overlap notes for slurs; separate them for a new tongue. "
        "A shared reed edge and body carry the register, with independent closure grain and quiet aspiration. "
        "Press PLAY for the 18-second performance."})
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
                    for cc, value in ((1, 0), (2, pressure), (3, 0), (5, 0), (76, 64))], "path": str(path)})
            results["renders"].append({"note": note, "cc2": pressure, "path": str(path),
                "sha256": file_digest(path), "capture": capture})
            write(output / "results.json", results)
            print(path.name, flush=True)
    schedule = invoke({"command": "exportPatternSchedule", "sequence": sequence(), "channel": 1})
    write(output / "phrase-schedule.json", schedule)
    phrase = output / "phrase.wav"
    results["demonstration"] = invoke({"command": "captureAudio", "sampleRate": 48000,
        "blockSize": 256, "channels": 2, "randomSeed": 1729, "outputGain": 1,
        "voiceDurationSeconds": VOICE_SECONDS, "durationMs": 19000,
        "events": schedule["events"], "path": str(phrase)})
    verify_reed(output, invoke)
    results["articulations"] = render_articulations(output, invoke)
    results.update(ok=True, verification="verification.json", demonstrationSha256=file_digest(phrase))
    write(output / "results.json", results)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--impulse", type=Path, help="Optional alternative body response; default is five broad controls")
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
