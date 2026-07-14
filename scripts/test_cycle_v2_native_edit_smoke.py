#!/usr/bin/env python3

import json
import os
import socket
import subprocess
import sys
import tempfile
import time


REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(REPO, "build/standalone-debug/cycle-v2/CycleV2.app")
CLICK = "/opt/homebrew/bin/cliclick"
SETTLE_SECONDS = 0.08


class NativeEditSmoke:
    def __init__(self):
        temporary = tempfile.gettempdir()
        self.socket_path = os.path.join(temporary, "cycle-v2-native-edit-smoke.sock")
        self.log_path = os.path.join(temporary, "cycle-v2-native-edit-smoke.log")
        self.request_id = 0

    def start(self):
        environment = os.environ.copy()
        environment["CYCLE_APP_PATH"] = APP
        environment["CYCLE_PROCESS_NAME"] = "CycleV2"
        subprocess.run(
            [os.path.join(REPO, "scripts/run_cycle_agent_session.sh"), self.socket_path, self.log_path],
            cwd=REPO,
            env=environment,
            check=True,
            stdout=subprocess.DEVNULL,
        )

    def stop(self):
        try:
            self.command({"command": "quit"})
        except (ConnectionError, FileNotFoundError):
            pass

    def command(self, command):
        self.request_id += 1
        request = {"id": str(self.request_id), "command": command}
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.connect(self.socket_path)
            connection.sendall((json.dumps(request, separators=(",", ":")) + "\n").encode())
            response = b""
            while b"\n" not in response:
                response += connection.recv(65536)
        decoded = json.loads(response.split(b"\n", 1)[0])
        if not decoded.get("ok") or not decoded["result"].get("ok"):
            raise AssertionError(decoded)
        return decoded["result"].get("data", {})

    def click(self, *commands):
        subprocess.run([CLICK, "-w", "20", *commands], check=True)
        time.sleep(SETTLE_SECONDS)

    def drag(self, source, destination):
        moves = []
        for step in range(1, 5):
            x = round(source[0] + (destination[0] - source[0]) * step / 4)
            y = round(source[1] + (destination[1] - source[1]) * step / 4)
            moves.append(f"dm:{x},{y}")
        self.click(
            f"m:{source[0]},{source[1]}",
            "w:60",
            f"dd:{source[0]},{source[1]}",
            *moves,
            f"du:{destination[0]},{destination[1]}",
        )

    def open_editor(self, node_id, trimesh=False):
        command = "openMeshPopup" if trimesh else "openNodeEditor"
        self.command({"command": command, "nodeId": node_id})
        return self.inspect(node_id)

    def inspect(self, node_id):
        return self.command({"command": "inspectNodeControls", "nodeId": node_id})

    def target(self, target_id):
        targets = self.command({"command": "inspectPointerTargets"})["targets"]
        return next(target for target in targets if target["id"] == target_id)["screenBounds"]

    @staticmethod
    def point(bounds, x, y):
        return (
            round(bounds["x"] + bounds["width"] * x),
            round(bounds["y"] + bounds["height"] * y),
        )

    @staticmethod
    def parameters(state):
        return {parameter["id"]: parameter["value"] for parameter in state["parameters"]}

    @staticmethod
    def flat_snapshot(state):
        return json.loads(NativeEditSmoke.parameters(state)["curve.modelSnapshot"])

    def effect2d_sequence(self):
        state = self.open_editor("waveshaper")
        panel = self.target("expanded:waveshaper.panel2D")
        initial = self.flat_snapshot(state)

        def open_phase(vertices):
            phases = sorted(vertex["x"] for vertex in vertices)
            left, right = max(zip(phases, phases[1:]), key=lambda pair: pair[1] - pair[0])
            return (left + right) * 0.5

        def panel_position(x, y):
            zoom = self.inspect("waveshaper")["effect2D"]["panelState"]["zoom"]
            return self.point(
                panel,
                (x - zoom["x"]) / zoom["w"],
                1.0 - (y - zoom["y"]) / zoom["h"],
            )

        add_at = panel_position(open_phase(initial["vertices"]), 0.32)
        self.click(f"rc:{add_at[0]},{add_at[1]}")
        added = self.flat_snapshot(self.inspect("waveshaper"))
        assert len(added["vertices"]) == len(initial["vertices"]) + 1
        added_vertex = next(vertex for vertex in added["vertices"] if vertex["id"] not in {
            vertex["id"] for vertex in initial["vertices"]
        })

        source = panel_position(added_vertex["x"], added_vertex["y"])
        destination = panel_position(min(0.85, added_vertex["x"] + 0.08), min(0.85, added_vertex["y"] + 0.08))
        self.drag(source, destination)
        moved = self.flat_snapshot(self.inspect("waveshaper"))
        moved_vertex = next(vertex for vertex in moved["vertices"] if vertex["id"] == added_vertex["id"])
        assert abs(moved_vertex["x"] - added_vertex["x"]) > 0.02
        assert moved["selection"] == added_vertex["id"]

        neighbour = min(
            (vertex for vertex in moved["vertices"] if vertex["id"] != moved_vertex["id"]),
            key=lambda vertex: abs(vertex["x"] - moved_vertex["x"]),
        )
        curve_start = panel_position(
            (moved_vertex["x"] + neighbour["x"]) * 0.5,
            (moved_vertex["y"] + neighbour["y"]) * 0.5,
        )
        curve_end = panel_position(
            moved_vertex["x"],
            moved_vertex["y"],
        )
        old_curve = moved_vertex["curve"]
        self.drag(curve_start, curve_end)
        reshaped = self.flat_snapshot(self.inspect("waveshaper"))
        reshaped_vertex = next(vertex for vertex in reshaped["vertices"] if vertex["id"] == added_vertex["id"])
        assert abs(reshaped_vertex["curve"] - old_curve) > 0.01

        selected_point = panel_position(reshaped_vertex["x"], reshaped_vertex["y"])
        self.click(f"c:{selected_point[0]},{selected_point[1]}", "kp:delete")
        deleted = self.flat_snapshot(self.inspect("waveshaper"))
        assert all(vertex["id"] != added_vertex["id"] for vertex in deleted["vertices"])

        second_add = panel_position(open_phase(deleted["vertices"]), 0.68)
        self.click(f"rc:{second_add[0]},{second_add[1]}")
        final = self.flat_snapshot(self.inspect("waveshaper"))
        assert len(final["vertices"]) == len(initial["vertices"]) + 1

    def trimesh_sequence(self):
        state = self.open_editor("waveMesh", trimesh=True)
        panel = self.target("expanded:waveMesh.panel2D")
        initial = self.parameters(state)
        initial_vertex_count = state["trimesh"]["vertexCount"]

        add_at = self.point(panel, 0.50, 0.50)
        self.click(f"rc:{add_at[0]},{add_at[1]}")
        added_state = self.inspect("waveMesh")
        added = self.parameters(added_state)
        added_vertex_count = added_state["trimesh"]["vertexCount"]
        assert added_vertex_count > initial_vertex_count

        selected_index = added_state["trimesh"]["selectedVertexIndex"]
        selected = {
            parameter["id"]: parameter["value"]
            for parameter in added_state["trimesh"]["selectedVertexParameters"]
        }
        source_phase = float(selected["vertex.phase"])
        source_amp = float(selected["vertex.amp"])
        source = self.point(panel, source_phase, 1.0 - source_amp)
        collision_target = min(
            (marker for marker in state["trimesh"]["vertexMarkers"] if marker["index"] < initial_vertex_count),
            key=lambda marker: (marker["phase"] - source_phase) ** 2 + (marker["amp"] - source_amp) ** 2,
        )
        collision_point = self.point(panel, collision_target["phase"], 1.0 - collision_target["amp"])
        self.drag(source, collision_point)
        collision_state = self.inspect("waveMesh")
        collision_selected = {
            parameter["id"]: parameter["value"]
            for parameter in collision_state["trimesh"]["selectedVertexParameters"]
        }
        collision_distance = (
            (float(collision_selected["vertex.phase"]) - collision_target["phase"]) ** 2
            + (float(collision_selected["vertex.amp"]) - collision_target["amp"]) ** 2
        ) ** 0.5
        assert collision_distance > 0.002

        source_phase = float(collision_selected["vertex.phase"])
        source_amp = float(collision_selected["vertex.amp"])
        source = self.point(panel, source_phase, 1.0 - source_amp)
        destination = self.point(panel, min(0.85, source_phase + 0.12), max(0.15, 1.0 - source_amp - 0.10))
        self.drag(source, destination)
        moved_state = self.inspect("waveMesh")
        moved = {
            parameter["id"]: parameter["value"]
            for parameter in moved_state["trimesh"]["selectedVertexParameters"]
        }
        assert abs(float(moved["vertex.phase"]) - source_phase) > 0.01

        amp_slider = self.target("expanded:waveMesh.trimeshVertexParameter.vertex.amp")
        amp_point = self.point(amp_slider, 0.78, 0.5)
        self.click(f"c:{amp_point[0]},{amp_point[1]}")
        parameter_state = self.inspect("waveMesh")
        parameter_edited = {
            parameter["id"]: parameter["value"]
            for parameter in parameter_state["trimesh"]["selectedVertexParameters"]
        }
        assert abs(float(parameter_edited["vertex.amp"]) - float(moved["vertex.amp"])) > 0.01

        red_slider = self.target("expanded:waveMesh.trimeshMorphRail.red")
        red_point = self.point(red_slider, 0.72, 0.5)
        old_red = float(added["red"])
        self.click(f"c:{red_point[0]},{red_point[1]}")
        morphed = self.parameters(self.inspect("waveMesh"))
        assert abs(float(morphed["red"]) - old_red) > 0.01

        selected_phase = float(parameter_edited["vertex.phase"])
        selected_amp = float(parameter_edited["vertex.amp"])
        selected_point = self.point(panel, selected_phase, 1.0 - selected_amp)
        self.click(f"c:{selected_point[0]},{selected_point[1]}", "kp:delete")
        deleted_state = self.inspect("waveMesh")
        deleted_count = deleted_state["trimesh"]["vertexCount"]
        assert deleted_count < added_vertex_count

        second_add = self.point(panel, 0.38, 0.62)
        self.click(f"rc:{second_add[0]},{second_add[1]}")
        final_state = self.inspect("waveMesh")
        final_count = final_state["trimesh"]["vertexCount"]
        assert final_count > deleted_count

    def run(self):
        self.start()
        try:
            self.effect2d_sequence()
            self.trimesh_sequence()
        finally:
            self.stop()


if __name__ == "__main__":
    if sys.platform != "darwin":
        raise SystemExit("Native CycleV2 edit smoke requires macOS")
    NativeEditSmoke().run()
    print("CycleV2 native edit smoke passed")
