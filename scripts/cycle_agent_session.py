#!/usr/bin/env python3
import argparse
import json
import socket
import sys


def load_command(args):
    if args.command:
        return json.loads(args.command)

    if args.file:
        with open(args.file, "r", encoding="utf-8") as handle:
            return json.load(handle)

    return json.load(sys.stdin)


def main():
    parser = argparse.ArgumentParser(description="Send one command to a running Cycle automation session.")
    parser.add_argument("socket_path", help="Unix-domain socket path passed to Cycle --agent-session")
    parser.add_argument("-c", "--command", help="JSON command object")
    parser.add_argument("-f", "--file", help="JSON command file")
    parser.add_argument("--id", default="1", help="Request id to include in the session envelope")
    args = parser.parse_args()

    command = load_command(args)
    request = {
        "id": args.id,
        "command": command,
    }

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(args.socket_path)
        sock.sendall((json.dumps(request, separators=(",", ":")) + "\n").encode("utf-8"))

        chunks = []
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break

            chunks.append(chunk)

            if b"\n" in chunk:
                break

    response = b"".join(chunks).split(b"\n", 1)[0]
    print(json.dumps(json.loads(response.decode("utf-8")), indent=2))


if __name__ == "__main__":
    main()
