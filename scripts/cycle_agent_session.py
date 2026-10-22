#!/usr/bin/env python3
import argparse
import json
import sys

from cycle_automation_support.session import Session


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
    with Session(args.socket_path) as session:
        response = session.command(command, request_id=args.id, require_ok=False)
    print(json.dumps(response, indent=2))


if __name__ == "__main__":
    main()
