#!/usr/bin/env python3
"""Build a local listening page from the acquired saxophone reference metadata."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import wave

from cycle_reference_study import file_digest

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "scripts/templates/saxophone_reference_audit.html"


def read(path):
    return json.loads(path.read_text())


def source_path(directory, relative):
    path = (directory / relative).resolve()
    if not path.is_relative_to(directory.resolve()):
        raise ValueError("Reference path escapes its collection: " + relative)
    return path


def record(identifier, collection, path, **fields):
    return {"id": identifier, "source": collection, "originalFilename": path.name,
            "instrument": "Saxophone", "title": path.stem, "technique": "", "pitch": "",
            "dynamic": "", "kind": "Recording", "sourceUrl": "", "note": "",
            "available": True, "reserved": False, "_path": path, **fields}


def reference_records(corpus, include_reserved=False):
    records = []
    vcsl = corpus / "vcsl"
    for item in read(vcsl / "download-manifest.json")["audioFiles"]:
        labels = item["sourceLabels"]
        technique = {"Non-Vibrato": "Non-vibrato"}.get(
                labels["articulationFolder"], labels["articulationFolder"])
        layer = labels.get("velocityLayer")
        dynamic = "Layer " + str(layer) if layer is not None else ""
        records.append(record("vcsl:" + item["id"], "VCSL",
            source_path(vcsl, item["localPath"]),
            instrument="Tenor saxophone", title=" · ".join(filter(None,
                (labels["noteToken"], technique, dynamic))), technique=technique,
            pitch=labels["noteToken"], dynamic=dynamic, kind="Single note",
            durationSeconds=item["wave"]["durationSeconds"], sha256=item["sha256"],
            sourceUrl=item["url"], note="CC0. Source pitch and layer labels; sounding octave and dynamic mapping are unverified. All twelve takes are developmental references."))
    iowa = corpus / "iowa"
    for item in read(iowa / "collection.json")["audioFiles"]:
        labels = item["publishedLabels"]
        technique = {"NoVib": "Non-vibrato", "Vib": "Vibrato"}[labels["technique"]]
        reserved = item["partition"] == "reserved"
        records.append(record("iowa:" + item["id"], "Iowa",
            source_path(iowa, item["path"]),
            instrument="Alto saxophone", title=" · ".join((labels["pitchRange"],
                labels["dynamic"], technique)), technique=technique, pitch=labels["pitchRange"],
            dynamic=labels["dynamic"], kind="Chromatic scale",
            durationSeconds=item["durationSeconds"], sha256=item["sha256"],
            sourceUrl="https://theremin.music.uiowa.edu/MISaltosaxophone.html",
            note="Michael Giles, 1997. Whole chromatic scale, not a single note. Publisher pitch-range and dynamic labels; original recording level is retained.",
            reserved=reserved, available=not reserved or include_reserved,
            _sampleRate=item["sampleRate"], _channels=item["channels"], _frames=item["frames"]))
    solo = corpus / "youtube"
    acquisition = read(solo / "acquisition.json")
    records.append(record("solo:" + acquisition["videoId"], "Solo",
        source_path(solo, acquisition["mediaPath"]), instrument="Tenor saxophone",
        title=acquisition["title"], technique="Solo", kind="Continuous performance",
        durationSeconds=float(acquisition["actualProbe"]["format"]["duration"]),
        sha256=acquisition["sha256"], sourceUrl=acquisition["sourceUrl"],
        note="Ivan Renta / congahead. Continuous solo with no annotated note cuts. Original local WebM/Opus stream."))
    growl = ROOT / "cycle-v2/content/references/sax-growl.wav"
    with wave.open(str(growl), "rb") as audio:
        duration = audio.getnframes() / audio.getframerate()
    records.append(record("bundled:sax-growl", "Bundled", growl,
        title="Sax growl", technique="Growl (inferred)", kind="Texture",
        durationSeconds=duration, sha256=file_digest(growl),
        note="Bundled reference. Growl is an inferred label; performer and recording provenance are unspecified."))
    return records


def prepare_media(records, output):
    media = output / "media"
    media.mkdir(parents=True, exist_ok=True)
    manifest_path = output / "media-manifest.json"
    previous = read(manifest_path) if manifest_path.exists() else {}
    manifest = {}
    for item in records:
        item["url"] = None
        source = item["_path"]
        converted = source.suffix.lower() in (".aiff", ".aif")
        suffix = ".wav" if converted else source.suffix
        name = re.sub(r"[^A-Za-z0-9_.-]", "-", item["id"]) + suffix
        destination = media / name
        if not item["available"]:
            if destination.exists() or destination.is_symlink():
                destination.unlink()
            continue
        if file_digest(source) != item["sha256"]:
            raise ValueError("Original source changed: " + str(source))
        cached = previous.get(item["id"], {})
        reusable = (destination.is_file() and cached.get("sourceSha256") == item["sha256"]
                    and cached.get("mediaSha256") == file_digest(destination))
        if converted and not reusable:
            ffmpeg = shutil.which("ffmpeg")
            if ffmpeg is None:
                raise RuntimeError("ffmpeg is required for browser-compatible Iowa playback")
            temporary = destination.with_suffix(".new.wav")
            subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                "-i", str(source), "-map", "0:a:0", "-c:a", "pcm_s16le", "-map_metadata", "-1",
                str(temporary)], check=True)
            with wave.open(str(temporary), "rb") as audio:
                if (audio.getframerate(), audio.getnchannels(), audio.getsampwidth(), audio.getnframes()) != (
                        item["_sampleRate"], item["_channels"], 2, item["_frames"]):
                    raise ValueError("Playback conversion changed the source format or duration")
            temporary.replace(destination)
        elif not converted and not reusable:
            if destination.exists() or destination.is_symlink():
                destination.unlink()
            destination.symlink_to(os.path.relpath(source, media))
        item["url"] = "media/" + name
        manifest[item["id"]] = {"sourcePath": str(source), "sourceSha256": item["sha256"],
            "mediaSha256": file_digest(destination),
            "conversion": "PCM16 WAV, original sample rate/channels/levels" if converted else "original"}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


def comparison_records(path, references):
    """Read optional render pairs; playback copies share the reference media pipeline."""
    if path is None:
        return {}, []
    path = path.resolve()
    manifest = read(path)
    if manifest.get("schema") != "saxophone-anchor-comparisons.v1":
        raise ValueError("Unsupported saxophone comparison manifest")
    known = {item["id"] for item in references}
    paired = set()
    media = []

    def clip(item, identifier):
        local = source_path(path.parent, item["path"])
        with wave.open(str(local), "rb") as audio:
            seconds = audio.getnframes() / audio.getframerate()
        result = {"id": identifier, "available": True, "_path": local,
            "originalFilename": local.name, "sha256": item["sha256"],
            "durationSeconds": seconds}
        media.append(result)
        return result

    comparisons = []
    for item in manifest.get("comparisons", []):
        reference = item["referenceId"]
        if reference not in known or reference in paired:
            raise ValueError("Unknown or duplicated comparison reference: " + reference)
        paired.add(reference)
        render = item["renderId"]
        if not render:
            raise ValueError("Comparison render ID is required")
        pair = {"referenceId": reference, "renderId": render, "note": item.get("note", "")}
        for role in ("source", "synth", "rawSynth"):
            if role in item:
                pair[role] = clip(item[role], "comparison:" + render + ":" + role)
        if "source" not in pair or "synth" not in pair:
            raise ValueError("Each comparison needs source and synth WAV files")
        if abs(pair["source"]["durationSeconds"] - pair["synth"]["durationSeconds"]) > .02:
            raise ValueError("Comparison source and synth durations differ")
        comparisons.append(pair)
    bridge = manifest.get("bridge", {})
    auditions = []
    identifiers = set()
    for item in bridge.get("clips", []):
        identifier = item["id"]
        if identifier in identifiers:
            raise ValueError("Duplicated bridge clip ID: " + identifier)
        identifiers.add(identifier)
        audition = clip(item, "bridge:" + identifier)
        audition.update(label=item["label"], renderId=item["renderId"], note=item.get("note", ""))
        auditions.append(audition)
    return {"comparisons": comparisons, "bridge": {"title": bridge.get("title", "Expressive bridge"),
        "note": bridge.get("note", ""), "clips": auditions}}, media


def public_fields(value):
    if isinstance(value, dict):
        return {key: public_fields(item) for key, item in value.items() if not key.startswith("_")}
    if isinstance(value, list):
        return [public_fields(item) for item in value]
    return value


def build(corpus, output, include_reserved=False, comparisons=None):
    output.mkdir(parents=True, exist_ok=True)
    records = reference_records(corpus.resolve(), include_reserved)
    comparison_catalog, comparison_media = comparison_records(comparisons, records)
    prepare_media(records + comparison_media, output)
    catalog = {"schema": "saxophone-reference-audit.v1", "title": "Saxophone references",
        "reservedSourcesIncluded": include_reserved,
        "tracks": public_fields(records), **public_fields(comparison_catalog)}
    serialized = json.dumps(catalog, ensure_ascii=False, allow_nan=False).replace("<", "\\u003c")
    page = TEMPLATE.read_text().replace("__SAXOPHONE_CATALOG_JSON__", serialized)
    (output / "index.html").write_text(page)
    (output / "catalog.json").write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n")
    return catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus-root", type=Path, default=ROOT / ".context/sax-reference-downloads")
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".context/saxophone-reference-audit")
    parser.add_argument("--include-reserved", action="store_true",
                        help="Include the two previously reserved Iowa takes for listening")
    parser.add_argument("--comparisons", type=Path,
                        help="Optional saxophone-anchor-comparisons.v1 manifest with source/synth WAV pairs")
    args = parser.parse_args()
    catalog = build(args.corpus_root, args.output_dir.resolve(), args.include_reserved, args.comparisons)
    print(str((args.output_dir / "index.html").resolve()))
    print(str(sum(item["available"] for item in catalog["tracks"])) + " playable recordings")


if __name__ == "__main__":
    main()
