#!/usr/bin/env python3

"""Export Cycle 1 factory presets as canonical JSON through a live app session."""

import argparse
import json
from pathlib import Path

from cycle_agent_smoke_session import send_command


def is_canonical_preset(path):
    try:
        with path.open(encoding="utf-8") as source:
            value = json.load(source)
    except (OSError, json.JSONDecodeError):
        return False

    return isinstance(value, dict) and isinstance(value.get("preset"), dict)


def export_preset(socket_path, source, destination, request_prefix, timeout):
    export_response = send_command(
        socket_path,
        {
            "command": "exportPresetFile",
            "sourcePath": str(source.resolve()),
            "path": str(destination.resolve()),
        },
        f"{request_prefix}:export",
        timeout,
    )
    return {
        "source": str(source),
        "destination": str(destination),
        "ok": bool(export_response.get("ok")),
        "phase": "complete" if export_response.get("ok") else "export",
        "response": export_response,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Export all Cycle 1 factory presets through one agent session.")
    parser.add_argument("socket_path", help="Cycle 1 --agent-session socket")
    parser.add_argument("source_directory", type=Path)
    parser.add_argument("destination_directory", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--skip", action="append", default=[], help="Preset stem to skip")
    args = parser.parse_args()

    sources = sorted(args.source_directory.glob("*.cyc"), key=lambda path: path.name.lower())
    args.destination_directory.mkdir(parents=True, exist_ok=True)
    results = []

    skipped = set(args.skip)

    for index, source in enumerate(sources):
        destination = args.destination_directory / f"{source.stem}.json"
        if is_canonical_preset(destination):
            results.append({
                "source": str(source),
                "destination": str(destination),
                "ok": True,
                "phase": "existing",
            })
            continue
        if source.stem in skipped:
            results.append({
                "source": str(source),
                "destination": str(destination),
                "ok": False,
                "phase": "skipped",
            })
            continue
        try:
            result = export_preset(
                args.socket_path,
                source,
                destination,
                f"preset-library:{index}:{source.stem}",
                args.timeout,
            )
        except Exception as exc:
            result = {
                "source": str(source),
                "destination": str(destination),
                "ok": False,
                "phase": "transport",
                "message": str(exc),
            }
        results.append(result)

    report = {
        "sourceDirectory": str(args.source_directory),
        "destinationDirectory": str(args.destination_directory),
        "sourceCount": len(sources),
        "exportedCount": sum(bool(result["ok"]) for result in results),
        "newExportCount": sum(result["phase"] == "complete" for result in results),
        "existingCount": sum(result["phase"] == "existing" for result in results),
        "failedCount": sum(not result["ok"] for result in results),
        "results": results,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if report["failedCount"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
