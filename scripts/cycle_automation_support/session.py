import json
import socket


def command(socket_path, payload, timeout=10, require_ok=True):
    request = {"id": "coordinator", "command": payload}
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(timeout)
        connection.connect(str(socket_path))
        connection.sendall((json.dumps(request, separators=(",", ":")) + "\n").encode())
        response = bytearray()
        while b"\n" not in response:
            chunk = connection.recv(65536)
            if not chunk:
                break
            response.extend(chunk)
    if not response:
        raise RuntimeError("Automation session returned no response")
    result = json.loads(response.split(b"\n", 1)[0])
    if require_ok and (not result.get("ok") or not result.get("result", {}).get("ok")):
        raise RuntimeError(f"Automation command failed: {result}")
    return result
