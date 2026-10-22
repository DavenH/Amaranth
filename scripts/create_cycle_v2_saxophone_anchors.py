#!/usr/bin/env python3
"""Author two measured tenor saxophone anchors and their first shared bridge."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from cycle_automation_support.identity import process_identity
from cycle_automation_support.session import command
from cycle_performance_suite import load_suite
from cycle_reference_study import file_digest
from cycle_saxophone_anchor_analysis import plot_render_comparison
from cycle_saxophone_anchor_model import VOICE_SECONDS, harmonic_model, volume_model
from cycle_study_artifacts import write
from render_cycle_v2_performance_suite import render_suite, wav_identity


ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "cycle-v2/content/presets/saxophone-3.cyclegraph"


def parameters(node, values):
    return {"nodeId": node, "parameters": [{"id": key,
        "value": str(int(value)) if isinstance(value, bool) else str(value)} for key, value in values.items()]}


def sequence():
    return {"durationSeconds": 13, "notes": [
        {"pitch": note, "velocity": 127, "startSeconds": start, "durationSeconds": duration}
        for note, start, duration in ((62, .2, 2), (62, 2.6, 2), (62, 5.2, 4), (60, 10, .8), (64, 11.4, .8))],
        "controls": [{"controller": 2, "timeSeconds": time, "value": value}
            for time, value in ((0, 0), (2.5, 127), (4.9, 0), (5.4, 0), (7, 127), (8.8, 0), (9.8, 64), (12.8, 0))]}


def author(output, anchors, invoke):
    def inspect(node, spectral=False):
        return invoke({"command": "inspectNodeControls", "nodeId": node,
            **({"midiNote": 62, "cycleFrames": 2048} if spectral else {})})

    def export(name):
        path = output / name
        invoke({"command": "exportGraph", "path": str(path)})
        return path.read_bytes()

    def edit(edits):
        revision = inspect("voice")["documentRevision"]
        return invoke({"command": "applyNodeEdits", "expectedDocumentRevision": revision, "edits": edits})

    invoke({"command": "openGraph", "path": str(BASELINE), "includeSnapshot": False})
    original = json.loads(export("factory-source.cyclegraph"))
    for node in original["nodes"]:
        if node["id"] not in ("voice", "globalInput", "voiceOutput", "output"):
            invoke({"command": "deleteNode", "nodeId": node["id"]})
    for guide in original["guides"]:
        invoke({"command": "deleteGuideCurve", "guideId": guide["id"]})
    ids = {}
    for name, kind, x, y in (
            ("controls", "modulationTriple", 100, 60), ("magnitudeL", "trilinearMesh", 100, 340),
            ("phaseL", "trilinearMesh", 440, 340),
            ("ifftL", "ifft", 790, 380), ("magnitudeR", "trilinearMesh", 100, 710),
            ("phaseR", "trilinearMesh", 440, 710),
            ("ifftR", "ifft", 790, 750), ("stereoSum", "add", 1140, 500),
            ("volume", "envelope", 1140, 790), ("volumeMultiply", "multiply", 1490, 500)):
        ids[name] = invoke({"command": "addNode", "kind": kind, "x": x, "y": y})["nodeId"]
    voice_length = invoke({"command": "convertSynthesisParameter", "typeId": "voiceContext",
        "parameterId": "voiceLength", "direction": "toStored", "value": VOICE_SECONDS})["storedValue"]
    mesh_gain = invoke({"command": "convertSynthesisParameter", "typeId": "trilinearMesh",
        "parameterId": "gain", "direction": "toStored", "value": 2.0})["storedValue"]
    settings = [parameters("voice", {"voiceLength": voice_length, "pitch": 0, "oversampling": "2x",
            "pitchIndependentSpectralControl": True, "controlInterval": "64", "envelopeModulation": "live"}),
        parameters("output", {"gain": .5}),
        parameters(ids["controls"], {"yellowSource": "voiceTime", "redSource": "keyScale",
            "blueSource": "midiCC", "blueController": 2}),
        parameters(ids["volume"], {"purpose": "volume", "logarithmic": False, "declick": True})]
    for side in ("L", "R"):
        settings.extend((parameters(ids["magnitude" + side], {"signalType": "spectralMagnitude",
            "polarity": "unipolar", "range": 1 / 3, "gain": mesh_gain}),
            parameters(ids["phase" + side], {"signalType": "spectralPhase", "phaseMode": "absolute",
            "polarity": "bipolar", "range": 0, "gain": .5})))
    # Incremental graph construction uses the normal domain commands. Atomic
    # model replacement below requires a complete, connected candidate graph.
    for setting in settings:
        for parameter in setting["parameters"]:
            invoke({"command": "setNodeParameter", "nodeId": setting["nodeId"],
                "parameterId": parameter["id"], "value": parameter["value"]})

    edges = [("globalInput", "time", "output", "time"),
        ("stereoSum", "out", "volumeMultiply", "left"), ("volume", "env", "volumeMultiply", "right"),
        ("volumeMultiply", "out", "voiceOutput", "time")]
    for side, port in (("L", "left"), ("R", "right")):
        ids["cablePan" + side] = "anchorCablePan" + side
        edges.extend((("ifft" + side, "time", "cablePan" + side, "in"),
            ("cablePan" + side, "out", "stereoSum", port),
            ("magnitude" + side, "out", "ifft" + side, "mag"), ("phase" + side, "out", "ifft" + side, "phase")))
        for kind in ("magnitude", "phase"):
            edges.append(("voice", "context", kind + side, "context"))
    edges.extend((("controls", "modulation", "voice", "modulation"), ("controls", "blue", "volume", "blue")))
    # Adding one cable cannot reduce the aggregate incomplete-voice-path issue
    # while other branches remain disconnected (feature queue M07). Import one
    # declared topology built from canonical node exports, then author every
    # model through the revision-checked semantic command below.
    configured = json.loads(export("configured-nodes.cyclegraph"))
    for key in ("preview", "outputSpyX", "outputSpyY"):
        configured["presetPresentation"].pop(key, None)
    # SpectralLayer is the durable internal representation of a cable's pan
    # handle. It is hidden by the canvas and is not a standalone palette node.
    for side, value in (("L", 0), ("R", 1)):
        configured["nodes"].append({"id": ids["cablePan" + side], "kind": "spectralLayer",
            "definitionVersion": 1, "position": {"x": 1000, "y": 380 if side == "L" else 750},
            "parameters": {"pan": value}})
    configured["edges"] = [{"sourceNodeId": ids.get(source, source), "sourcePortId": source_port,
        "destNodeId": ids.get(destination, destination), "destPortId": destination_port,
        "connectionKind": "configurationAttachment" if destination_port == "modulation" else "signal",
        "attachmentType": "modulationTriple" if destination_port == "modulation" else "none"}
        for source, source_port, destination, destination_port in edges]
    topology = output / "declared-topology.cyclegraph"
    write(topology, configured)
    invoke({"command": "openGraph", "path": str(topology), "includeSnapshot": False})
    shapes = []
    for channel, side in enumerate(("L", "R")):
        for kind in ("magnitude", "phase"):
            controls = inspect(ids[kind + side], True)
            write(output / (kind + side + "-coordinates.json"), controls["spectralAuthoring"])
            shapes.append(harmonic_model(controls, anchors, channel, kind == "phase"))
    shapes.append(volume_model(inspect(ids["volume"]), anchors))
    before = export("before-shapes.cyclegraph")
    edit(shapes)
    after = export("after-shapes.cyclegraph")
    for action, expected in (("undo", before), ("redo", after)):
        invoke({"command": action})
        if export(action + ".cyclegraph") != expected:
            raise RuntimeError("Authored models failed exact " + action)

    for node, x, y in (("voice", 450, 60), ("voiceOutput", 1800, 500), ("globalInput", 1490, 860), ("output", 1800, 860)):
        invoke({"command": "moveNode", "nodeId": node, "x": x, "y": y})
    for logical, text in (("volume", "Measured raw channel-energy RMS. Blue selects the two note envelopes; source key-off was not observed."),
            ("magnitudeL", "Measured left-channel harmonic envelopes. Yellow is note time; blue is the layer bridge."),
            ("magnitudeR", "Measured right-channel harmonic envelopes; preserves the second channel's recording balance."),
            ("phaseL", "Measured settled harmonic phases relative to the fundamental; static across note age."),
            ("phaseR", "Measured settled right-channel relative phases. Independent of amplitude fitting.")):
        invoke({"command": "setExplanation", "scope": "node", "nodeId": ids[logical], "text": text})
    presets = {}
    for name, source, value in (("tenor-anchor-layer-2", "constant", 0), ("tenor-anchor-layer-3", "constant", 1),
                                 ("tenor-anchor-bridge", "midiCC", .5)):
        edit([parameters(ids["controls"], {"blueSource": source, "blueConstant": value})])
        if source == "constant":
            anchor = anchors[int(value)]
            explanation = (f"Fixed VCSL tenor D3 layer {anchor['sourceLayer']} reference model. "
                "This is a source layer label. The bridge coordinate is fixed; CC2 is inactive.\n"
                "Play the embedded note to hear the measured full-note contour.")
            demonstration = {"durationSeconds": anchor["durationSeconds"], "notes": [{"pitch": 62,
                "velocity": 127, "startSeconds": 0,
                "durationSeconds": anchor["timings"]["selectedTailStartSeconds"]}], "controls": []}
        else:
            explanation = ("Tenor reference bridge: CC2 moves from VCSL D3 layer 2 to layer 3. "
                "These are source layer labels.\nPlay the embedded sequence for endpoints, "
                "a held-note bridge and nearby pitches.")
            demonstration = sequence()
        invoke({"command": "setPresetSequence", "sequence": demonstration})
        invoke({"command": "setExplanation", "scope": "preset", "text": explanation + "\n"
            "Independent left/right harmonic shapes preserve recording balance; amplitude and early timbre evolve over time. "
            "The measured anchor is MIDI 62 (293.66 Hz).\n"
            "This is a local harmonic model: noise, detailed pitch fluctuations and broader register behaviour remain to be refined."})
        path = output / (name + ".cyclegraph")
        invoke({"command": "saveGraph", "path": str(path)})
        saved = export(name + "-inspection.cyclegraph")
        invoke({"command": "openGraph", "path": str(path), "includeSnapshot": False})
        if export(name + "-reload.cyclegraph") != saved:
            raise RuntimeError("Preset changed on reload")
        if any(node["kind"] in ("stereoSplit", "stereoJoin") for node in json.loads(saved)["nodes"]):
            raise RuntimeError("Retired channel-management nodes are forbidden")
        presets[name] = path
    return presets


def bridge_suite(anchors, output):
    def case(identifier, duration, release, value, note=62, controls=()):
        return {"id": identifier, "durationMs": duration * 1000,
            "events": [{"type": "controller", "controller": 2, "value": value, "timeMs": 0},
                {"type": "noteOn", "note": note, "velocity": 1, "timeMs": 0},
                *[{"type": "controller", "controller": 2, "value": value, "timeMs": time * 1000}
                    for time, value in controls],
                {"type": "noteOff", "note": note, "timeMs": release * 1000}]}
    cases = [case("endpoint-" + str(anchor["sourceLayer"]), anchor["durationSeconds"],
        anchor["timings"]["selectedTailStartSeconds"], value) for anchor, value in zip(anchors, (0, 127))]
    cases.extend(case("bridge-" + str(value), 3.2, 2.3, value) for value in (32, 64, 96))
    sweep = [(1 + index / 32 * 2, round(index / 32 * 127)) for index in range(33)]
    sweep += [(3 + index / 32 * 2, round((1 - index / 32) * 127)) for index in range(1, 33)]
    cases.append(case("sweep", 7, 6, 0, controls=sweep))
    cases.extend(case("nearby-" + str(note), 3, 2, 64, note) for note in (60, 64))
    pattern = output / "demonstration.cyclepattern"
    write(pattern, {"version": 1, "sequence": sequence()})
    cases.append({"id": "demonstration", "pattern": pattern.name, "tailMs": 1000})
    suite = output / "bridge-suite.json"
    write(suite, {"schema": "cycle-performance-suite.v1", "id": "saxophone-anchor-bridge", "repetitions": 2,
        "render": {"sampleRate": 48000, "blockSize": 256, "channels": 2, "randomSeed": 1729,
            "outputGain": 1, "voiceDurationSeconds": VOICE_SECONDS},
        "budget": {"maxRenders": 18, "maxAudioSeconds": 120, "maxWallSeconds": 360}, "cases": cases})
    return load_suite(suite)


def listening_manifest(output, anchors, renders, bridge):
    def clip(path):
        return {"path": str(path.relative_to(output)), "sha256": file_digest(path)}
    comparisons = []
    for anchor in anchors:
        layer = str(anchor["sourceLayer"])
        source = output / ("source-layer-" + layer + ".wav")
        shutil.copyfile(anchor["sourcePath"], source)
        rendered = next(item for item in renders if item["id"] == "anchor-layer-" + layer)
        comparisons.append({"referenceId": "vcsl:" + anchor["id"], "renderId": rendered["id"],
            "source": clip(source), "synth": clip(Path(rendered["path"])),
            "note": "Full-note comparison at original recording level, with no local gain or time alignment. "
                "48 measured harmonics per channel; static settled phase, evolving spectral shape and measured amplitude. "
                "Noise and detailed pitch fluctuations remain unmodelled. Source key-off is an authored timing hypothesis."})
    labels = {"endpoint-2": "Layer 2", "bridge-32": "25%", "bridge-64": "50%", "bridge-96": "75%",
        "endpoint-3": "Layer 3", "sweep": "Sweep", "nearby-60": "C4", "nearby-64": "E4", "demonstration": "Phrase"}
    auditions = [{"id": item["id"], "label": labels[item["id"]], "renderId": item["id"] + "-01",
        **clip(output / "bridge-renders" / item["renders"][0]["path"])} for item in bridge["cases"]]
    destination = output / "comparisons.json"
    write(destination, {"schema": "saxophone-anchor-comparisons.v1", "comparisons": comparisons,
        "bridge": {"title": "Tenor layer bridge · CC2", "note":
            "One shared harmonic graph interpolates both recorded layers. Intermediate settings and nearby notes "
            "are synthesis/playability probes, without intermediate acoustic references. Raw dynamic contrast is retained.",
            "clips": auditions}})
    return destination


def run(analysis_path, output, socket):
    output.mkdir(parents=True, exist_ok=False)
    analysis = json.loads(analysis_path.read_text())
    anchors = analysis["anchors"]
    for anchor in anchors:
        for path_key, hash_key in (("sourcePath", "sourceSha256"), ("pitchAnalysisPath", "pitchAnalysisSha256")):
            if file_digest(Path(anchor[path_key])) != anchor[hash_key]:
                raise ValueError("Anchor input identity changed")
    engine = process_identity(int(os.environ["CYCLE_APP_PID"]))
    result = {"schema": "cycle-saxophone-anchor-fit.v1", "ok": False,
        "engine": {"pid": int(os.environ["CYCLE_APP_PID"]), "processIdentity": engine},
        "engineSha256": file_digest(Path(engine["executable"])), "analysisSha256": file_digest(analysis_path),
        "coordinatorArtifacts": os.environ["CYCLE_AUTOMATION_ARTIFACTS"],
        "authoringSha256": {name: file_digest(ROOT / "scripts" / name) for name in (
            "create_cycle_v2_saxophone_anchors.py", "cycle_saxophone_anchor_model.py", "cycle_saxophone_anchor_analysis.py")},
        "renders": [], "acousticTasteVerified": False}
    def invoke(payload):
        response = command(socket, payload, timeout=120, require_ok=False)
        with (output / "commands.jsonl").open("a") as log:
            log.write(json.dumps({"command": payload, "response": response}, allow_nan=False) + "\n")
        if not response.get("ok") or not response.get("result", {}).get("ok"):
            raise RuntimeError("Automation rejected: " + str(response))
        return response["result"].get("data", {})
    try:
        presets = author(output, anchors, invoke)
        result["presets"] = {name: {"path": str(path), "sha256": file_digest(path)} for name, path in presets.items()}
        for index, anchor in enumerate(anchors):
            preset = presets["tenor-anchor-layer-" + str(anchor["sourceLayer"])]
            for name, path in (("anchor", preset), ("baseline", ROOT / "cycle-v2/content/presets/expressive-saxophone.cyclegraph")):
                invoke({"command": "openGraph", "path": str(path), "includeSnapshot": False})
                wav = output / (name + "-layer-" + str(anchor["sourceLayer"]) + ".wav")
                events = [{"type": "controller", "timeMs": 0, "controller": controller, "value": value}
                    for controller, value in ((1, 0), (2, index * 127), (3, 0), (4, 0), (5, 0), (11, 104), (76, 64), (77, 0))]
                capture = invoke({"command": "captureAudio", "sampleRate": 48000, "blockSize": 256,
                    "channels": 2, "randomSeed": 1729, "outputGain": 1, "voiceDurationSeconds": VOICE_SECONDS,
                    "note": 62, "velocity": 1, "noteDurationMs": anchor["timings"]["selectedTailStartSeconds"] * 1000,
                    "durationMs": anchor["durationSeconds"] * 1000, "events": events, "path": str(wav)})
                measured = plot_render_comparison(anchor, wav, output / (wav.stem + ".png"))
                result["renders"].append({"id": wav.stem, "path": str(wav), "sha256": file_digest(wav),
                    "referenceId": "vcsl:" + anchor["id"], "capture": capture, "comparison": measured})
                print(wav.stem, "RMS error dB", round(measured["renderRmsErrorDb"], 3),
                    "settled harmonic error dB", round(measured["intervals"]["settledBody"]["globalGainMatchedHarmonicMeanAbsoluteDbError"], 3), flush=True)
        plan = bridge_suite(anchors, output)
        bridge = render_suite(plan, presets["tenor-anchor-bridge"], output / "bridge-renders",
            lambda payload, **kwargs: command(socket, payload, **kwargs), result["engine"])
        if not bridge["ok"]:
            raise RuntimeError("Bridge suite failed: " + bridge.get("error", "unknown"))
        result["bridgeSuite"] = "bridge-renders/results.json"
        for index, anchor in enumerate(anchors):
            case = bridge["cases"][index]
            expected = wav_identity(output / ("anchor-layer-" + str(anchor["sourceLayer"]) + ".wav"), plan["cases"][index]["performance"])
            if expected["pcmSha256"] != case["renders"][0]["pcmSha256"]:
                raise RuntimeError("Shared bridge endpoint differs from independent anchor " + anchor["id"])
        result["endpointPcmEquality"] = True
        result["listeningManifest"] = str(listening_manifest(output, anchors, result["renders"], bridge))
        result["ok"] = True
    except Exception as error:
        result["error"] = str(error)
        raise
    finally:
        write(output / "results.json", result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analysis", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--in-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.in_session:
        coordinator = [sys.executable, str(ROOT / "scripts/cycle_automation.py"), "exec", "--product", "cycle-v2"]
        if args.no_build:
            coordinator.append("--no-build")
        return subprocess.run(coordinator + ["--", sys.executable, str(Path(__file__).resolve()),
            "--analysis", str(args.analysis.resolve()), "--output-dir", str(args.output_dir.resolve()), "--in-session"],
            cwd=ROOT, check=False).returncode
    run(args.analysis.resolve(), args.output_dir.resolve(), os.environ["CYCLE_AGENT_SESSION"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
