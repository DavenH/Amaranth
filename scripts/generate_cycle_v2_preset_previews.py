#!/usr/bin/env python3
"""Generate embedded Cycle V2 preset previews through a running agent session."""

import argparse
import json
import pathlib
import socket
import subprocess
import sys
import time


def send_command(socket_path, command, request_id):
    request = {"id": request_id, "command": command}
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.connect(socket_path)
        connection.sendall(
            (json.dumps(request, separators=(",", ":")) + "\n").encode("utf-8")
        )
        chunks = []
        while True:
            chunk = connection.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
            if b"\n" in chunk:
                break
    payload = b"".join(chunks).split(b"\n", 1)[0]
    return json.loads(payload.decode("utf-8"))


def response_result(response):
    return response.get("result", response)


def response_message(response, fallback):
    result = response_result(response)
    return result.get("message", response.get("message", fallback))


def modified_paths(repository):
    result = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=no"],
        cwd=repository,
        check=True,
        capture_output=True,
        text=True,
    )
    return {
        line[3:].strip()
        for line in result.stdout.splitlines()
        if len(line) > 3
    }


def has_preview_source(preset):
    try:
        document = json.loads(preset.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    nodes = document.get("nodes", [])
    infrastructure = {"output", "globalInput", "voiceOutput"}
    return (
        any(node.get("kind") == "output" for node in nodes)
        and any(node.get("kind") not in infrastructure for node in nodes)
    )


def top_level_value_span(text, member_name):
    depth = 0
    index = 0
    while index < len(text):
        character = text[index]
        if character == '"':
            value, end = json.decoder.scanstring(text, index + 1)
            if depth == 1 and value == member_name:
                cursor = end
                while cursor < len(text) and text[cursor].isspace():
                    cursor += 1
                if cursor >= len(text) or text[cursor] != ":":
                    raise ValueError(f"Malformed root member: {member_name}")
                cursor += 1
                while cursor < len(text) and text[cursor].isspace():
                    cursor += 1
                _, value_length = json.JSONDecoder().raw_decode(text[cursor:])
                return cursor, cursor + value_length
            index = end
            continue
        if character in "[{":
            depth += 1
        elif character in "]}":
            depth -= 1
        index += 1
    return None


def formatted_value(value, base_indent):
    lines = json.dumps(value, indent=4, ensure_ascii=False).splitlines()
    return lines[0] + "".join(
        "\n" + " " * base_indent + line for line in lines[1:]
    )


def embed_preview(preset, preview):
    text = preset.read_text(encoding="utf-8")
    document = json.loads(text)
    presentation = document.get("presetPresentation", {})
    presentation["version"] = 1
    presentation["preview"] = preview
    encoded = formatted_value(presentation, 4)
    span = top_level_value_span(text, "presetPresentation")
    if span:
        updated = text[: span[0]] + encoded + text[span[1] :]
    else:
        closing = text.rfind("}")
        if closing < 0:
            raise ValueError("Preset root object is missing its closing brace")
        prefix = text[:closing].rstrip()
        separator = "" if prefix.endswith("{") else ","
        updated = (
            prefix
            + separator
            + "\n    \"presetPresentation\": "
            + encoded
            + "\n"
            + text[closing:]
        )
    temporary = preset.with_suffix(preset.suffix + ".preview-tmp")
    temporary.write_text(updated, encoding="utf-8")
    temporary.replace(preset)


def main():
    parser = argparse.ArgumentParser(
        description="Embed default-output spy JPEGs in Cycle V2 preset JSON."
    )
    parser.add_argument("socket_path", help="CycleV2 --agent-session Unix socket")
    parser.add_argument(
        "--directory",
        default="cycle-v2/content/presets",
        help="Preset directory, relative to the repository by default",
    )
    parser.add_argument(
        "--preset",
        action="append",
        default=[],
        help="Generate only this preset path; may be supplied more than once",
    )
    parser.add_argument("--view", choices=("spectrum", "time"), default="spectrum")
    parser.add_argument("--write", action="store_true", help="Write previews; default is dry-run")
    parser.add_argument(
        "--include-dirty",
        action="store_true",
        help="Allow writing presets already modified in git",
    )
    parser.add_argument("--settle-ms", type=int, default=500)
    args = parser.parse_args()

    repository = pathlib.Path(__file__).resolve().parents[1]
    directory = pathlib.Path(args.directory)
    if not directory.is_absolute():
        directory = repository / directory
    if args.preset:
        presets = []
        for preset_name in args.preset:
            preset = pathlib.Path(preset_name)
            if not preset.is_absolute():
                preset = repository / preset
            presets.append(preset)
        presets.sort()
    else:
        presets = sorted(directory.glob("*.cyclegraph"))
    dirty = modified_paths(repository)

    candidates = []
    skipped = []
    for preset in presets:
        try:
            relative = preset.relative_to(repository).as_posix()
        except ValueError:
            relative = ""
        if not has_preview_source(preset):
            skipped.append((preset, "no renderable output source"))
        elif relative and relative in dirty and not args.include_dirty:
            skipped.append((preset, "modified in git"))
        else:
            candidates.append(preset)

    print(f"{len(candidates)} presets eligible; {len(skipped)} skipped")
    for preset, reason in skipped:
        print(f"SKIP {preset.name}: {reason}")
    if not args.write:
        print("Dry run only. Pass --write to generate previews.")
        return 0

    failures = []
    for index, preset in enumerate(candidates, start=1):
        response = send_command(
            args.socket_path,
            {"command": "openGraph", "path": str(preset)},
            f"open:{index}",
        )
        if not response.get("ok"):
            failures.append((preset, response_message(response, "open failed")))
            continue
        send_command(
            args.socket_path,
            {"command": "waitForIdle", "delayMs": args.settle_ms},
            f"wait:{index}",
        )

        response = {}
        for attempt in range(4):
            response = send_command(
                args.socket_path,
                {
                    "command": "generatePresetPreview",
                    "path": str(preset),
                    "view": args.view,
                    "embed": False,
                },
                f"preview:{index}:{attempt}",
            )
            if response.get("ok"):
                break
            time.sleep(0.25)
        if response.get("ok"):
            data = response_result(response).get("data", {})
            preview = {
                "mediaType": data["mediaType"],
                "width": data["width"],
                "height": data["height"],
                "view": data["view"],
                "data": data["data"],
            }
            embed_preview(preset, preview)
            print(f"[{index}/{len(candidates)}] {preset.name}")
        else:
            failures.append((preset, response_message(response, "preview failed")))

    for preset, message in failures:
        print(f"FAIL {preset.name}: {message}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
