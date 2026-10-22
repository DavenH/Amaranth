"""JSON-lines clients for an existing, exact-instance automation socket."""

import json
import socket
import sys
import uuid


class Session:
    """One sequential request/response connection; failed commands are never replayed."""

    def __init__(self, socket_path, timeout=10, max_response_bytes=64 * 1024 * 1024):
        self.socket_path = str(socket_path)
        self.timeout = timeout
        self.max_response_bytes = max_response_bytes
        self.connection = None
        self.buffer = bytearray()
        self.request_prefix = uuid.uuid4().hex
        self.request_count = 0

    def connect(self):
        if self.connection is None:
            connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                connection.settimeout(self.timeout)
                connection.connect(self.socket_path)
            except OSError:
                connection.close()
                raise
            self.connection = connection
        return self

    def close(self):
        if self.connection is not None:
            self.connection.close()
            self.connection = None
        self.buffer.clear()

    def reconnect(self):
        self.close()
        return self.connect()

    def __enter__(self):
        return self.connect()

    def __exit__(self, *_):
        self.close()

    def _response(self):
        while True:
            newline = self.buffer.find(b"\n")
            if newline >= 0:
                if newline > self.max_response_bytes:
                    raise RuntimeError("Automation response exceeds the session limit")
                line = bytes(self.buffer[:newline])
                del self.buffer[:newline + 1]
                return json.loads(line)
            if len(self.buffer) > self.max_response_bytes:
                raise RuntimeError("Automation response exceeds the session limit")
            chunk = self.connection.recv(65536)
            if not chunk:
                raise RuntimeError("Automation session disconnected before a complete response")
            self.buffer.extend(chunk)

    def command(self, payload, timeout=None, require_ok=True, request_id=None):
        self.connect()
        self.request_count += 1
        identifier = request_id if request_id is not None else f"{self.request_prefix}:{self.request_count}"
        request = {"id": identifier, "command": payload}
        try:
            self.connection.settimeout(self.timeout if timeout is None else timeout)
            self.connection.sendall((json.dumps(request, separators=(",", ":")) + "\n").encode())
            result = self._response()
            if not isinstance(result, dict) or result.get("id") != identifier:
                raise RuntimeError(f"Automation response ID does not match request {identifier}: {result}")
        except (OSError, ValueError, RuntimeError):
            self.close()
            raise
        if require_ok and (not result.get("ok") or not result.get("result", {}).get("ok")):
            raise RuntimeError(f"Automation command failed: {result}")
        return result


def command(socket_path, payload, timeout=10, require_ok=True):
    with Session(socket_path, timeout) as session:
        return session.command(payload, require_ok=require_ok)


def repl(socket_path, timeout=30, input_stream=None, output_stream=None):
    """Read a JSON command (or command name) per line; :reconnect and :exit are local."""
    source = input_stream or sys.stdin
    output = output_stream or sys.stdout
    interactive = source.isatty()
    with Session(socket_path, timeout) as session:
        if interactive:
            print("Cycle V2 session ready. JSON commands or command names; :reconnect, :exit.",
                  file=output, flush=True)
        while True:
            if interactive:
                print("cycle> ", end="", file=output, flush=True)
            line = source.readline()
            if not line:
                break
            line = line.strip()
            if not line:
                continue
            if line == ":exit":
                break
            try:
                if line == ":reconnect":
                    session.reconnect()
                    print("Reconnected; no commands replayed.", file=output, flush=True)
                    continue
                payload = json.loads(line) if line.startswith("{") else {"command": line}
                result = session.command(payload, require_ok=False)
                print(json.dumps(result, separators=(",", ":")), file=output, flush=True)
            except (OSError, ValueError, RuntimeError) as error:
                print(json.dumps({"clientError": str(error)}), file=output, flush=True)
