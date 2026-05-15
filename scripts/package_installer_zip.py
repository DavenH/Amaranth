#!/usr/bin/env python3

import argparse
import json
import os
import platform
import sys
import zipfile
from pathlib import Path


def platform_key():
    system = platform.system().lower()
    if system == "darwin":
        return "mac"
    if system == "linux":
        return "linux"
    if system == "windows":
        return "windows"
    return system


def artifact_source(artifact, key):
    sources = artifact.get("sources")
    if isinstance(sources, dict) and sources.get(key):
        return sources[key]
    return artifact.get("source")


def find_source(source, manifest_dir, artifact_roots):
    source_path = Path(source)
    candidates = []

    if source_path.is_absolute():
        candidates.append(source_path)
    else:
        candidates.append(manifest_dir / source_path)
        for root in artifact_roots:
            candidates.append(root / source_path)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    return None


def add_path(zip_file, source_path, archive_root):
    if source_path.is_dir():
        for path in sorted(source_path.rglob("*")):
            if path.is_file():
                zip_file.write(path, archive_root / path.relative_to(source_path))
        return

    zip_file.write(source_path, archive_root)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Package installer artifacts into the zip described by a product installer manifest."
    )
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument(
        "--artifact-root",
        action="append",
        default=[],
        type=Path,
        help="Directory to search for artifact sources. May be passed more than once.",
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--platform", default=platform_key(), choices=("mac", "linux", "windows"))
    parser.add_argument(
        "--allow-missing-optional",
        action="store_true",
        help="Skip missing artifacts with required=false instead of failing.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    manifest_path = args.manifest.resolve()
    manifest_dir = manifest_path.parent
    artifact_roots = [root.resolve() for root in args.artifact_root]

    with manifest_path.open("r", encoding="utf-8") as manifest_file:
        manifest = json.load(manifest_file)

    output_path = args.output
    if output_path is None:
        output_path = manifest_dir / manifest.get("zipName", f"{manifest.get('productName', 'Package')}.zip")

    output_path.parent.mkdir(parents=True, exist_ok=True)

    missing = []
    added = []
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zip_file:
        for artifact in manifest.get("artifacts", []):
            source = artifact_source(artifact, args.platform)
            if not source:
                missing.append(f"{artifact.get('id', '<unknown>')}: no source for {args.platform}")
                continue

            source_path = find_source(source, manifest_dir, artifact_roots)
            if source_path is None:
                if args.allow_missing_optional and not artifact.get("required", False):
                    continue
                missing.append(f"{artifact.get('id', source)}: {source}")
                continue

            archive_root = Path(source.replace("\\", "/"))
            add_path(zip_file, source_path, archive_root)
            added.append(f"{source} <- {source_path}")

    if missing:
        output_path.unlink(missing_ok=True)
        print("Missing installer artifacts:", file=sys.stderr)
        for item in missing:
            print(f"  {item}", file=sys.stderr)
        return 1

    print(f"Wrote {output_path}")
    for item in added:
        print(f"  {item}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
