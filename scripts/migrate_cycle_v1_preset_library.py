#!/usr/bin/env python3

"""Convert a canonical Cycle 1 preset export into a Cycle 2 library."""

import argparse
import json
from pathlib import Path
import re
import tempfile

import port_cycle_v1_preset


def kebab_case(name):
    name = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1-\2", name)
    name = re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", name)
    name = re.sub(r"([a-zA-Z])([0-9])", r"\1-\2", name)
    name = re.sub(r"([0-9])([a-zA-Z])", r"\1-\2", name)
    return re.sub(r"[^a-zA-Z0-9]+", "-", name).strip("-").lower()


def migrate_library(source_directory, destination_directory):
    sources = sorted(source_directory.glob("*.json"))
    names = {}
    for source in sources:
        names.setdefault(kebab_case(source.stem), []).append(source)

    destination_directory.mkdir(parents=True, exist_ok=True)
    results = []
    for source in sources:
        target_name = kebab_case(source.stem)
        destination = destination_directory / f"{target_name}.cyclegraph"
        if len(names[target_name]) > 1:
            results.append({
                "source": str(source),
                "destination": str(destination),
                "status": "collision",
                "issues": [
                    "normalized name is shared by: "
                    + ", ".join(path.name for path in names[target_name])
                ],
            })
            continue
        if destination.exists():
            results.append({
                "source": str(source),
                "destination": str(destination),
                "status": "existing",
                "issues": [],
            })
            continue

        with source.open(encoding="utf-8") as source_file:
            canonical = json.load(source_file)
        issues = port_cycle_v1_preset.validate_conversion(canonical)
        if issues:
            results.append({
                "source": str(source),
                "destination": str(destination),
                "status": "blocked",
                "issues": issues,
            })
            continue

        try:
            converted = port_cycle_v1_preset.convert(canonical)
            with tempfile.TemporaryDirectory(
                    prefix="cycle-v1-library-",
                    dir=destination_directory) as temporary_directory:
                temporary = Path(temporary_directory) / destination.name
                port_cycle_v1_preset.write_canonical_graph(converted, temporary)
                temporary.replace(destination)
            status = "converted"
            issues = []
        except Exception as error:
            status = "failed"
            issues = [str(error)]
        results.append({
            "source": str(source),
            "destination": str(destination),
            "status": status,
            "issues": issues,
        })

    counts = {
        status: sum(result["status"] == status for result in results)
        for status in ("converted", "existing", "blocked", "collision", "failed")
    }
    return {
        "sourceDirectory": str(source_directory.resolve()),
        "destinationDirectory": str(destination_directory.resolve()),
        "sourceCount": len(sources),
        "counts": counts,
        "results": results,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_directory", type=Path)
    parser.add_argument("destination_directory", type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()

    report = migrate_library(args.source_directory, args.destination_directory)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=4) + "\n", encoding="utf-8")
    print(json.dumps(report["counts"], sort_keys=True))
    return 1 if report["counts"]["collision"] or report["counts"]["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
