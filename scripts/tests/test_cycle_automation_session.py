import io
import json
import socket
import sys
import tempfile
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_automation_support.session import Session, command, repl


class Server:
    def __init__(self, handler):
        self.directory = tempfile.TemporaryDirectory(prefix="cycle-session-")
        self.path = Path(self.directory.name) / "test.sock"
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.listener.bind(str(self.path))
        self.listener.listen(4)
        self.listener.settimeout(3)
        self.errors = []
        self.handler = handler
        self.thread = threading.Thread(target=self.run)

    def run(self):
        try:
            self.handler(self.listener)
        except BaseException as error:
            self.errors.append(error)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.thread.join(4)
        self.listener.close()
        self.directory.cleanup()
        if self.thread.is_alive():
            raise AssertionError("Test server did not stop")
        if self.errors:
            raise self.errors[0]


def response(request, **extra):
    return {"id": request["id"], "ok": True, "result": {"ok": True, **extra}}


def write_line(connection, payload):
    connection.sendall((json.dumps(payload, ensure_ascii=False) + "\n").encode())


class SessionTest(unittest.TestCase):
    def test_same_socket_retains_partial_utf8_and_coalesced_responses(self):
        requests = []

        def serve(listener):
            with listener.accept()[0] as connection:
                reader = connection.makefile("rb")
                requests.append(json.loads(reader.readline()))
                first = response(requests[0], text="café" + "x" * 180000)
                second = response({"id": "test:2"}, text="second")
                encoded = ((json.dumps(first, ensure_ascii=False) + "\n")
                           + json.dumps(second) + "\n").encode()
                split = encoded.index(b"\xc3") + 1
                connection.sendall(encoded[:split])
                connection.sendall(encoded[split:])
                requests.append(json.loads(reader.readline()))
                reader.close()

        with Server(serve) as server, Session(server.path) as session:
            session.request_prefix = "test"
            first_fd = session.connection.fileno()
            self.assertEqual(session.command({"command": "first"})["result"]["text"], "café" + "x" * 180000)
            self.assertEqual(session.command({"command": "second"})["result"]["text"], "second")
            self.assertEqual(first_fd, session.connection.fileno())
        self.assertEqual([request["id"] for request in requests], ["test:1", "test:2"])

    def test_disconnect_is_not_replayed_and_explicit_reconnect_is_clean(self):
        requests = []

        def serve(listener):
            for index in range(2):
                with listener.accept()[0] as connection:
                    with connection.makefile("rb") as reader:
                        request = json.loads(reader.readline())
                    requests.append(request)
                    if index:
                        write_line(connection, response(request))

        with Server(serve) as server, Session(server.path) as session:
            with self.assertRaisesRegex(RuntimeError, "disconnected"):
                session.command({"command": "mutation"})
            self.assertIsNone(session.connection)
            session.reconnect()
            self.assertTrue(session.command({"command": "ping"})["ok"])
        self.assertEqual([request["command"]["command"] for request in requests], ["mutation", "ping"])
        self.assertNotEqual(requests[0]["id"], requests[1]["id"])

    def test_mismatched_response_id_and_oversized_line_close_session(self):
        for mode in ("id", "limit"):
            with self.subTest(mode=mode):
                def serve(listener):
                    with listener.accept()[0] as connection:
                        with connection.makefile("rb") as reader:
                            request = json.loads(reader.readline())
                        write_line(connection, response({"id": "wrong"} if mode == "id" else request,
                                                        text="x" * 500))
                with Server(serve) as server, Session(server.path, max_response_bytes=200 if mode == "limit" else 2000) as session:
                    with self.assertRaisesRegex(RuntimeError, "limit" if mode == "limit" else "ID"):
                        session.command({"command": "ping"})
                    self.assertIsNone(session.connection)

    def test_one_shot_and_repl_share_protocol_and_command_errors_do_not_disconnect(self):
        observed = []

        def serve(listener):
            for index in range(2):
                with listener.accept()[0] as connection:
                    with connection.makefile("rb") as reader:
                        for line in reader:
                            request = json.loads(line)
                            observed.append(request)
                            result = response(request)
                            if request["command"]["command"] == "invalid":
                                result["ok"] = False
                                result["result"]["ok"] = False
                            write_line(connection, result)

        with Server(serve) as server:
            self.assertTrue(command(server.path, {"command": "ping"})["ok"])
            output = io.StringIO()
            repl(server.path, input_stream=io.StringIO('invalid\n{"command":"ping"}\n:exit\n'),
                 output_stream=output)
            rows = [json.loads(line) for line in output.getvalue().splitlines()]
            self.assertFalse(rows[0]["ok"])
            self.assertTrue(rows[1]["ok"])
        self.assertEqual(len({row["id"] for row in observed}), 3)


if __name__ == "__main__":
    unittest.main()
