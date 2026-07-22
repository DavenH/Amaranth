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
        subprocess.run([
            "osascript", "-e", 'tell application "CycleV2" to activate'
        ], check=True)
        subprocess.run(["open", "-a", "cycle-v2"], check=True)
        time.sleep(0.1)

    def stop(self):
        try:
            self.command({"command": "quit"})
        except (ConnectionError, FileNotFoundError):
            pass
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            result = subprocess.run(
                ["pgrep", "-x", "CycleV2"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if result.returncode != 0:
                return
            time.sleep(0.02)
        raise AssertionError("CycleV2 did not exit after native edit smoke")

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

    @staticmethod
    def focus_app():
        subprocess.run(["open", "-a", "cycle-v2"], check=True)
        time.sleep(0.03)

    def click(self, *commands):
        self.focus_app()
        subprocess.run([CLICK, "-w", "20", *commands], check=True)
        time.sleep(SETTLE_SECONDS)

    def primary_click(self, point):
        self.focus_app()
        subprocess.run([
            CLICK,
            "-w",
            "20",
            f"m:{point[0] + 2},{point[1]}",
            "w:10",
            f"m:{point[0]},{point[1]}",
            "w:20",
            f"dd:{point[0]},{point[1]}",
        ], check=True)
        time.sleep(0.04)
        subprocess.run([CLICK, "-w", "20", f"du:{point[0]},{point[1]}"], check=True)
        time.sleep(SETTLE_SECONDS)

    def drag(self, source, destination, force_curve_reshape=False):
        self.focus_app()
        moves = []
        for step in range(1, 7):
            x = round(source[0] + (destination[0] - source[0]) * step / 6)
            y = round(source[1] + (destination[1] - source[1]) * step / 6)
            moves.append(f"dm:{x},{y}")
            moves.append("w:4")
        down_commands = [
            CLICK,
            "-w",
            "20",
            f"m:{source[0] + 2},{source[1]}",
            "w:10",
            f"m:{source[0]},{source[1]}",
            "w:20",
        ]
        if force_curve_reshape:
            down_commands.extend(["kd:cmd", "w:10"])
        down_commands.append(f"dd:{source[0]},{source[1]}")
        subprocess.run(down_commands, check=True)
        time.sleep(0.04)
        drag_commands = [
            CLICK,
            "-w",
            "20",
            *moves,
            f"du:{destination[0]},{destination[1]}",
        ]
        if force_curve_reshape:
            drag_commands.extend(["w:10", "ku:cmd"])
        subprocess.run(drag_commands, check=True)
        time.sleep(SETTLE_SECONDS)

    def drag_and_hold(self, source, destination):
        self.focus_app()
        moves = []
        for step in range(1, 7):
            x = round(source[0] + (destination[0] - source[0]) * step / 6)
            y = round(source[1] + (destination[1] - source[1]) * step / 6)
            moves.extend((f"dm:{x},{y}", "w:4"))
        subprocess.run([
            CLICK,
            "-w",
            "20",
            f"m:{source[0] + 2},{source[1]}",
            "w:10",
            f"m:{source[0]},{source[1]}",
            "w:20",
            f"dd:{source[0]},{source[1]}",
        ], check=True)
        time.sleep(0.04)
        subprocess.run([
            CLICK,
            "-w",
            "20",
            *moves,
        ], check=True)
        time.sleep(SETTLE_SECONDS)

    def release_drag(self, destination):
        subprocess.run([
            CLICK,
            "-w",
            "20",
            f"du:{destination[0]},{destination[1]}",
        ], check=True)
        time.sleep(SETTLE_SECONDS)

    def capture(self, name, bounds):
        directory = os.environ.get("CYCLE_V2_NATIVE_CAPTURE_DIR")
        if not directory:
            return
        time.sleep(0.25)
        os.makedirs(directory, exist_ok=True)
        region = "{x},{y},{width},{height}".format(**{
            key: round(bounds[key]) for key in ("x", "y", "width", "height")
        })
        subprocess.run([
            "screencapture", "-C", "-x", f"-R{region}", os.path.join(directory, f"{name}.png")
        ], check=True)

    def open_editor(self, node_id, trimesh=False):
        command = "openMeshPopup" if trimesh else "openNodeEditor"
        self.command({"command": command, "nodeId": node_id})
        time.sleep(0.2)
        return self.inspect(node_id)

    def inspect(self, node_id):
        return self.command({"command": "inspectNodeControls", "nodeId": node_id})

    def inspect_until(self, node_id, predicate):
        deadline = time.monotonic() + 0.1
        state = self.inspect(node_id)
        while not predicate(state) and time.monotonic() < deadline:
            time.sleep(0.005)
            state = self.inspect(node_id)
        return state

    def target(self, target_id):
        targets = self.command({"command": "inspectPointerTargets"})["targets"]
        return next(target for target in targets if target["id"] == target_id)["screenBounds"]

    def graph_state(self):
        return self.command({"command": "snapshotState"})

    def graph_state_until(self, predicate):
        deadline = time.monotonic() + 0.1
        state = self.graph_state()
        while not predicate(state) and time.monotonic() < deadline:
            time.sleep(0.005)
            state = self.graph_state()
        return state

    @staticmethod
    def causal_sequence(state):
        return max((int(event["sequence"]) for event in state["causalUpdates"]), default=0)

    @staticmethod
    def causal_events_since(state, sequence):
        return [
            event for event in state["causalUpdates"]
            if int(event["sequence"]) > sequence
        ]

    @staticmethod
    def assert_causal_exactly_once(events, context):
        assert not any(event["phase"] == "InvariantViolation" for event in events), (
            context,
            events,
        )
        completed = {}
        for event in events:
            if event["phase"] != "Completed":
                continue
            key = (event["editId"], event["nodeId"], event["product"])
            completed[key] = completed.get(key, 0) + 1
        duplicates = {key: count for key, count in completed.items() if count > 1}
        assert not duplicates, (context, duplicates)

    def key_chord(self, key, shift=False):
        self.focus_app()
        modifiers = "{command down, shift down}" if shift else "{command down}"
        subprocess.run([
            "osascript",
            "-e",
            f'tell application "System Events" to keystroke "{key}" using {modifiers}',
        ], check=True)
        time.sleep(SETTLE_SECONDS)

    def press_native_delete(self):
        self.focus_app()
        subprocess.run([
            "osascript", "-e", 'tell application "System Events" to key code 51'
        ], check=True)
        time.sleep(SETTLE_SECONDS)

    def create_from_palette(self, section_index, entry_index):
        canvas = self.target("canvas")
        group = (
            round(canvas["x"] + 54),
            round(canvas["y"] + 74 + 7 + section_index * 78 + 30),
        )
        entry = (
            round(canvas["x"] + 100 + 72),
            round(canvas["y"] + 74 + 7 + section_index * 78 + entry_index * 52 + 22),
        )
        self.primary_click(group)
        self.primary_click(entry)
        state = self.graph_state()
        assert state["selectedNodeId"], (section_index, entry_index, state)
        return state["selectedNodeId"]

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

    @staticmethod
    def model_revision(state):
        return int(NativeEditSmoke.parameters(state)["curve.modelRevision"])

    @staticmethod
    def selected_vertex_parameters(state):
        return {
            parameter["id"]: float(parameter["value"])
            for parameter in state["effect2D"]["panelState"]["selectedVertexParameters"]
        }

    @staticmethod
    def assert_trimesh_slice(state, context):
        trimesh = state["trimesh"]
        assert trimesh["sliceSampleCount"] > 1, (context, trimesh)
        assert trimesh["sliceAbsoluteSum"] > 1.0e-4, (context, trimesh)
        assert trimesh["sliceMaximum"] - trimesh["sliceMinimum"] > 1.0e-4, (context, trimesh)
        assert trimesh["panelSampleCount"] > 1, (context, trimesh)
        assert trimesh["panelInterceptCount"] > 1, (context, trimesh)
        assert trimesh["panelAbsoluteSum"] > 1.0e-4, (context, trimesh)
        assert trimesh["panelMaximum"] - trimesh["panelMinimum"] > 1.0e-4, (context, trimesh)

    def audio_samples(self, frame_count=128):
        return self.command({"command": "captureAudio", "frames": frame_count})["samples"]

    @staticmethod
    def assert_audio_changed(before, after, context):
        assert len(before) == len(after)
        maximum_delta = max(abs(float(lhs) - float(rhs)) for lhs, rhs in zip(before, after))
        assert maximum_delta > 1.0e-5, (context, maximum_delta)

    @staticmethod
    def has_edge(state, source_node, source_port, dest_node, dest_port):
        return any(
            edge["sourceNodeId"] == source_node
            and edge["sourcePortId"] == source_port
            and edge["destNodeId"] == dest_node
            and edge["destPortId"] == dest_port
            for edge in state["edges"]
        )

    @staticmethod
    def node_bounds(state, node_id):
        return next(node["bounds"] for node in state["nodes"] if node["id"] == node_id)

    @staticmethod
    def horizontal_gap(left, right):
        return float(right["x"]) - float(left["x"] + left["width"])

    def graph_authoring_sequence(self):
        initial = self.graph_state()
        assert initial["compileSucceeded"]

        fft_edge_index = next(
            index for index, edge in enumerate(initial["edges"])
            if edge["sourceNodeId"] == "waveMesh"
            and edge["destNodeId"] == "fft"
        )
        cable_before = self.target(f"edge:{fft_edge_index}")
        fft_node = self.target("node:fft")
        fft_source = self.point(fft_node, 0.5, 0.5)
        fft_destination = (fft_source[0] + 120, fft_source[1] + 80)
        self.drag_and_hold(fft_source, fft_destination)
        cable_during_drag = self.target(f"edge:{fft_edge_index}")
        assert cable_during_drag != cable_before, (cable_before, cable_during_drag)
        self.capture("authoring-cable-drag", self.target("canvas"))
        self.release_drag(fft_destination)
        self.key_chord("z")

        created_envelope = self.create_from_palette(4, 0)
        assert created_envelope == "env2"
        created = self.graph_state()
        assert created["nodeCount"] == initial["nodeCount"] + 1

        envelope_output = self.target(f"output:{created_envelope}.env")
        scratch_input = self.target("input:waveMesh.scratch")
        self.drag(
            self.point(envelope_output, 0.5, 0.5),
            self.point(scratch_input, 0.5, 0.5),
        )
        connected = self.graph_state()
        assert connected["compileSucceeded"]
        assert self.has_edge(connected, created_envelope, "env", "waveMesh", "scratch")
        assert not self.has_edge(connected, "scratchEnv", "env", "waveMesh", "scratch")

        self.key_chord("z")
        undone_connection = self.graph_state_until(
            lambda state: self.has_edge(state, "scratchEnv", "env", "waveMesh", "scratch")
        )
        assert self.has_edge(undone_connection, "scratchEnv", "env", "waveMesh", "scratch")
        assert not self.has_edge(undone_connection, created_envelope, "env", "waveMesh", "scratch")

        self.key_chord("z", shift=True)
        redone_connection = self.graph_state_until(
            lambda state: self.has_edge(state, created_envelope, "env", "waveMesh", "scratch")
        )
        assert self.has_edge(redone_connection, created_envelope, "env", "waveMesh", "scratch")

        saved_path = os.path.join(tempfile.gettempdir(), "cycle-v2-native-authoring.cyclegraph")
        self.command({"command": "saveGraph", "path": saved_path})
        self.command({"command": "openGraph", "path": saved_path})
        reloaded = self.graph_state()
        assert reloaded["compileSucceeded"]
        assert reloaded["nodeCount"] == created["nodeCount"]
        assert self.has_edge(reloaded, created_envelope, "env", "waveMesh", "scratch")

        created_node = self.target(f"node:{created_envelope}")
        self.primary_click(self.point(created_node, 0.5, 0.5))
        self.press_native_delete()
        deleted = self.graph_state()
        assert deleted["nodeCount"] == initial["nodeCount"]
        assert not any(node["id"] == created_envelope for node in deleted["nodes"])

        self.key_chord("z")
        restored = self.graph_state_until(
            lambda state: any(node["id"] == created_envelope for node in state["nodes"])
        )
        assert restored["nodeCount"] == initial["nodeCount"] + 1
        assert self.has_edge(restored, created_envelope, "env", "waveMesh", "scratch")

        self.key_chord("z", shift=True)
        deleted_again = self.graph_state_until(
            lambda state: not any(node["id"] == created_envelope for node in state["nodes"])
        )
        assert deleted_again["nodeCount"] == initial["nodeCount"]

        self.command({
            "command": "openGraph",
            "path": os.path.join(REPO, "cycle-v2", "resources", "default.cyclegraph"),
        })

        insertion_initial = self.graph_state()
        created_delay = self.create_from_palette(5, 3)
        assert created_delay == "delay"
        delay_node = self.target(f"node:{created_delay}")
        wave_output = self.target("output:waveMesh.out")
        fft_input = self.target("input:fft.time")
        cable_midpoint = (
            round((self.point(wave_output, 0.5, 0.5)[0] + self.point(fft_input, 0.5, 0.5)[0]) * 0.5),
            round((self.point(wave_output, 0.5, 0.5)[1] + self.point(fft_input, 0.5, 0.5)[1]) * 0.5),
        )
        self.drag(self.point(delay_node, 0.5, 0.5), cable_midpoint)
        inserted = self.graph_state()
        assert inserted["compileSucceeded"]
        assert inserted["nodeCount"] == insertion_initial["nodeCount"] + 1
        assert inserted["edgeCount"] == insertion_initial["edgeCount"] + 1
        assert not self.has_edge(inserted, "waveMesh", "out", "fft", "time")
        assert self.has_edge(inserted, "waveMesh", "out", created_delay, "time")
        assert self.has_edge(inserted, created_delay, "time", "fft", "time")
        wave_bounds = self.node_bounds(inserted, "waveMesh")
        delay_bounds = self.node_bounds(inserted, created_delay)
        fft_bounds = self.node_bounds(inserted, "fft")
        assert self.horizontal_gap(wave_bounds, delay_bounds) >= 55.9
        assert self.horizontal_gap(delay_bounds, fft_bounds) >= 55.9

        self.key_chord("z")
        insertion_undone = self.graph_state_until(
            lambda state: self.has_edge(state, "waveMesh", "out", "fft", "time")
        )
        assert self.has_edge(insertion_undone, "waveMesh", "out", "fft", "time")
        assert not self.has_edge(insertion_undone, "waveMesh", "out", created_delay, "time")

        self.key_chord("z", shift=True)
        insertion_redone = self.graph_state_until(
            lambda state: self.has_edge(state, "waveMesh", "out", created_delay, "time")
        )
        assert self.has_edge(insertion_redone, "waveMesh", "out", created_delay, "time")
        assert self.has_edge(insertion_redone, created_delay, "time", "fft", "time")
        redone_wave_bounds = self.node_bounds(insertion_redone, "waveMesh")
        redone_delay_bounds = self.node_bounds(insertion_redone, created_delay)
        redone_fft_bounds = self.node_bounds(insertion_redone, "fft")
        assert self.horizontal_gap(redone_wave_bounds, redone_delay_bounds) >= 55.9
        assert self.horizontal_gap(redone_delay_bounds, redone_fft_bounds) >= 55.9

        self.command({
            "command": "openGraph",
            "path": os.path.join(REPO, "cycle-v2", "resources", "default.cyclegraph"),
        })
        reset = self.graph_state()
        assert reset["nodeCount"] == initial["nodeCount"]
        assert reset["edgeCount"] == initial["edgeCount"]
        assert reset["compileSucceeded"]

    def effect2d_sequence(self):
        initial_audio = self.audio_samples()
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

        panel = self.target("expanded:waveshaper.panel2D")
        source = panel_position(added_vertex["x"], added_vertex["y"])
        destination = panel_position(min(0.85, added_vertex["x"] + 0.16), min(0.85, added_vertex["y"] + 0.16))
        self.click(f"m:{source[0]},{source[1]}")
        self.drag(source, destination)
        moved_state = self.inspect_until(
            "waveshaper",
            lambda state: abs(next(
                vertex for vertex in self.flat_snapshot(state)["vertices"]
                if vertex["id"] == added_vertex["id"]
            )["x"] - added_vertex["x"]) > 0.02,
        )
        moved = self.flat_snapshot(moved_state)
        moved_vertex = next(vertex for vertex in moved["vertices"] if vertex["id"] == added_vertex["id"])
        assert abs(moved_vertex["x"] - added_vertex["x"]) > 0.02, (
            "new Waveshaper vertex did not move",
            added_vertex,
            moved_vertex,
            source,
            destination,
            moved_state["effect2D"]["panelState"],
        )
        assert moved["selection"] == added_vertex["id"]

        reshaped = moved
        used_vertex_ids = set()
        for polarity in (1, -1):
            direction = "upward" if polarity > 0 else "downward"
            def polarity_candidates():
                current_inspection = self.inspect("waveshaper")
                current = self.flat_snapshot(current_inspection)
                vertices = {vertex["x"]: vertex for vertex in current["vertices"]}
                found = []
                internal_vertices = sorted(vertices.values(), key=lambda item: item["x"])[1:-1]
                for point in current_inspection["effect2D"]["panelState"]["waveformPoints"]:
                    vertex = min(internal_vertices, key=lambda item: abs(item["x"] - point["x"]))
                    vertical_delta = vertex["y"] - point["y"]
                    if (vertical_delta * polarity > 0.06
                            and vertex["curve"] < 0.9
                            and vertex["id"] not in used_vertex_ids):
                        found.append((abs(vertical_delta), point, vertex))
                return found

            candidates = polarity_candidates()
            if not candidates:
                current = self.flat_snapshot(self.inspect("waveshaper"))
                movable = [
                    vertex for vertex in sorted(current["vertices"], key=lambda item: item["x"])[1:-1]
                    if vertex["id"] not in used_vertex_ids and vertex["curve"] < 0.9
                ]
                if movable:
                    control_vertex = movable[0]
                    print(
                        f"[waveshaper] arrange {direction} curve-drag precondition",
                        flush=True,
                    )
                    source = panel_position(control_vertex["x"], control_vertex["y"])
                    destination = panel_position(control_vertex["x"], 0.85 if polarity > 0 else 0.2)
                    self.click(f"m:{source[0]},{source[1]}")
                    self.drag(source, destination)
                    candidates = polarity_candidates()
                else:
                    before_add = current
                    add_position = panel_position(
                        open_phase(before_add["vertices"]), 0.85 if polarity > 0 else 0.2
                    )
                    self.click(f"rc:{add_position[0]},{add_position[1]}")
                    after_add = self.flat_snapshot(self.inspect("waveshaper"))
                    assert any(vertex["id"] not in {
                        item["id"] for item in before_add["vertices"]
                    } for vertex in after_add["vertices"])
                    candidates = polarity_candidates()
                    assert candidates, ("missing curve polarity after insertion", polarity)
            assert candidates, ("missing curve polarity", polarity)
            _, curve_point, control_vertex = max(candidates, key=lambda item: item[0])
            used_vertex_ids.add(control_vertex["id"])

            curve_start = panel_position(curve_point["x"], curve_point["y"])
            if polarity == 1:
                blank = self.point(panel, 0.5, 0.1)
                self.click(
                    f"m:{blank[0] + 8},{blank[1]}",
                    "w:20",
                    f"m:{blank[0]},{blank[1]}",
                )
                blank_state = self.inspect("waveshaper")["effect2D"]["panelState"]
                assert not blank_state["curveHover"], "curve hover remained active away from the curve"
                assert not blank_state["curveResizeCursor"], "curve cursor remained active away from the curve"
                self.capture("effect2d-before-hover", panel)
            self.click(f"m:{curve_start[0]},{curve_start[1]}")
            hover_state = self.inspect("waveshaper")["effect2D"]["panelState"]
            assert hover_state["curveHover"], ("curve hover callback missing", polarity)
            assert hover_state["curveResizeCursor"], ("curve resize cursor missing", polarity)
            if polarity == 1:
                self.capture("effect2d-after-hover", panel)
            hovered_vertex = hover_state["currentVertex"]
            before_drag = self.flat_snapshot(self.inspect("waveshaper"))
            control_vertex = min(
                before_drag["vertices"],
                key=lambda item: abs(item["x"] - hovered_vertex["x"]) + abs(item["y"] - hovered_vertex["y"]),
            )
            assert (control_vertex["y"] - curve_point["y"]) * polarity > 0.06
            print(f"[waveshaper] drag curve {direction} toward control vertex", flush=True)
            self.capture(f"waveshaper-{direction}-before-curve-drag", panel)
            curve_end = panel_position(
                curve_point["x"] + (control_vertex["x"] - curve_point["x"]) * 1.2,
                curve_point["y"] + (control_vertex["y"] - curve_point["y"]) * 1.2,
            )
            self.drag(curve_start, curve_end, force_curve_reshape=True)
            reshaped_state = self.inspect_until(
                "waveshaper",
                lambda state: next(
                    vertex for vertex in self.flat_snapshot(state)["vertices"]
                    if vertex["id"] == control_vertex["id"]
                )["curve"] > control_vertex["curve"] + 0.01,
            )
            reshaped = self.flat_snapshot(reshaped_state)
            reshaped_vertex = next(
                vertex for vertex in reshaped["vertices"] if vertex["id"] == control_vertex["id"]
            )
            assert reshaped_vertex["curve"] > control_vertex["curve"] + 0.01, (
                polarity, control_vertex["curve"], reshaped_vertex["curve"]
            )
            self.capture(f"waveshaper-{direction}-after-curve-drag", panel)

        vertex_to_delete = reshaped["selection"]
        assert vertex_to_delete > 0
        selected_vertex = next(item for item in reshaped["vertices"] if item["id"] == vertex_to_delete)
        selected_point = panel_position(selected_vertex["x"], selected_vertex["y"])
        self.click(f"c:{selected_point[0]},{selected_point[1]}", "w:60", "kp:delete")
        deleted = self.flat_snapshot(self.inspect("waveshaper"))
        assert all(item["id"] != vertex_to_delete for item in deleted["vertices"])

        second_add = panel_position(open_phase(deleted["vertices"]), 0.68)
        self.click(f"rc:{second_add[0]},{second_add[1]}")
        final = self.flat_snapshot(self.inspect("waveshaper"))
        assert len(final["vertices"]) == len(deleted["vertices"]) + 1
        self.assert_audio_changed(initial_audio, self.audio_samples(), "Waveshaper downstream output")

    def envelope_sequence(self):
        self.command({
            "command": "openGraph",
            "path": os.path.join(REPO, "cycle-v2", "resources", "with-spies.cyclegraph"),
        })
        time.sleep(SETTLE_SECONDS)
        initial_audio = self.audio_samples(2048)
        initial_state = self.open_editor("env")
        initial_snapshot = self.parameters(initial_state)["curve.modelSnapshot"]
        panel = self.target("expanded:env.panel2D")
        panel_state = initial_state["effect2D"]["panelState"]
        zoom = panel_state["zoom"]
        intercept = panel_state["intercepts"][1]

        def panel_position(x, y):
            return self.point(
                panel,
                (x - zoom["x"]) / zoom["w"],
                1.0 - (y - zoom["y"]) / zoom["h"],
            )

        vertex_point = panel_position(intercept["x"], intercept["y"])
        self.click(f"m:{vertex_point[0]},{vertex_point[1]}")
        hovered = self.inspect("env")
        hovered_panel = hovered["effect2D"]["panelState"]
        assert hovered_panel["hasCurrentCube"], "Envelope hover did not resolve a cube"
        assert "currentVertex" in hovered_panel, "Envelope hover did not resolve a vertex"

        self.primary_click(vertex_point)
        selected = self.inspect("env")
        selected_parameters = self.selected_vertex_parameters(selected)
        expanded = self.target("expanded:env")
        amp_rail = next(
            rail for rail in selected["effect2D"]["vertexParameterRails"]
            if rail["id"] == "vertex.amp"
        )["bounds"]
        amp_value = float(selected_parameters["vertex.amp"])
        amp_target = amp_value - 0.08 if amp_value > 0.5 else amp_value + 0.08
        amp_point = (
            round(expanded["x"] + amp_rail["x"] + amp_rail["width"] * amp_target),
            round(expanded["y"] + amp_rail["y"] + amp_rail["height"] * 0.5),
        )
        self.primary_click(amp_point)
        selected = self.inspect_until(
            "env",
            lambda state: abs(
                self.selected_vertex_parameters(state)["vertex.amp"] - amp_target
            ) < 0.03,
        )
        selected_parameters = self.selected_vertex_parameters(selected)
        assert abs(selected_parameters["vertex.amp"] - amp_target) < 0.03, (
            amp_value,
            amp_target,
            selected_parameters["vertex.amp"],
            amp_rail,
        )
        initial_revision = self.model_revision(selected)
        initial_snapshot = self.parameters(selected)["curve.modelSnapshot"]

        blank = self.point(panel, 0.92, 0.12)
        self.click(f"m:{blank[0]},{blank[1]}")
        away = self.inspect("env")
        assert self.selected_vertex_parameters(away) == selected_parameters, (
            "Envelope parameter selection changed on hover",
            selected_parameters,
            self.selected_vertex_parameters(away),
        )

        panel = self.target("expanded:env.panel2D")
        vertex_point = panel_position(intercept["x"], intercept["y"])
        self.click(f"m:{vertex_point[0]},{vertex_point[1]}")
        destination = panel_position(
            min(1.35, intercept["x"] + 0.08),
            max(0.1, intercept["y"] - 0.12),
        )
        causal_start = self.causal_sequence(self.graph_state())
        self.drag_and_hold(vertex_point, destination)
        held_state = self.graph_state()
        held_events = self.causal_events_since(held_state, causal_start)
        assert held_state["probeRefreshMode"] == "On Release", held_state["probeRefreshMode"]
        assert any(
            event["product"] == "LocalSlice" and event["phase"] == "Completed"
            for event in held_events
        ), (held_events, self.inspect("env"))
        assert any(event["phase"] == "DeferredUntilCommit" for event in held_events), held_events
        assert not any(
            event["product"] in ("PreviewTraversal", "ProbePreview")
            and event["phase"] == "Completed"
            for event in held_events
        ), held_events
        self.release_drag(destination)
        moved = self.inspect_until(
            "env",
            lambda state: self.model_revision(state) > initial_revision,
        )
        moved_parameters = self.selected_vertex_parameters(moved)
        assert self.model_revision(moved) == initial_revision + 1, (
            initial_revision,
            self.model_revision(moved),
        )
        assert (
            abs(moved_parameters["vertex.phase"] - selected_parameters["vertex.phase"]) > 0.01
            or abs(moved_parameters["vertex.amp"] - selected_parameters["vertex.amp"]) > 0.01
        ), (selected_parameters, moved_parameters)
        assert self.parameters(moved)["curve.modelSnapshot"] != initial_snapshot
        committed_state = self.graph_state_until(lambda state: any(
            int(event["sequence"]) > causal_start
            and event["product"] == "ProbePreview"
            and event["phase"] == "Published"
            for event in state["causalUpdates"]
        ))
        self.assert_causal_exactly_once(
            self.causal_events_since(committed_state, causal_start),
            "Envelope On Release gesture",
        )

        refresh_toggle = self.target("probeRefreshMode")
        self.primary_click(self.point(refresh_toggle, 0.5, 0.5))
        toggled_state = self.graph_state()
        assert toggled_state["probeRefreshMode"] == "Live", (refresh_toggle, toggled_state)
        self.capture("probe-rail-live-tabs", self.target("canvas"))

        live_start = self.causal_sequence(self.graph_state())
        live_source = panel_position(
            moved_parameters["vertex.phase"],
            moved_parameters["vertex.amp"],
        )
        live_destination = panel_position(
            max(0.05, moved_parameters["vertex.phase"] - 0.04),
            min(0.9, moved_parameters["vertex.amp"] + 0.08),
        )
        self.drag_and_hold(live_source, live_destination)
        live_state = self.graph_state_until(lambda state: any(
            int(event["sequence"]) > live_start
            and event["product"] == "ProbePreview"
            and event["phase"] == "Published"
            for event in state["causalUpdates"]
        ))
        live_events = self.causal_events_since(live_state, live_start)
        self.assert_causal_exactly_once(live_events, "Envelope Live movement")
        completed_before_release = sum(
            event["phase"] == "Completed"
            and event["product"] in ("PreviewTraversal", "ProbePreview")
            for event in live_events
        )
        self.release_drag(live_destination)
        released_state = self.graph_state_until(lambda state: any(
            int(event["sequence"]) > self.causal_sequence(live_state)
            and event["phase"] == "AlreadyCurrent"
            for event in state["causalUpdates"]
        ))
        released_events = self.causal_events_since(released_state, live_start)
        completed_after_release = sum(
            event["phase"] == "Completed"
            and event["product"] in ("PreviewTraversal", "ProbePreview")
            for event in released_events
        )
        assert completed_after_release == completed_before_release, released_events
        self.assert_causal_exactly_once(released_events, "Envelope Live commit")
        self.assert_audio_changed(
            initial_audio,
            self.audio_samples(2048),
            "Envelope downstream output",
        )

    def trimesh_sequence(self):
        initial_audio = self.audio_samples()
        state = self.open_editor("waveMesh", trimesh=True)
        panel = self.target("expanded:waveMesh.panel2D")
        self.capture("trimesh-00-initial", panel)
        initial = self.parameters(state)
        initial_vertex_count = state["trimesh"]["vertexCount"]
        self.assert_trimesh_slice(state, "initial Trimesh slice")

        intercepts = sorted(state["trimesh"]["panelIntercepts"], key=lambda point: point["x"])
        left, right = max(
            zip(intercepts, intercepts[1:]),
            key=lambda pair: pair[1]["x"] - pair[0]["x"],
        )
        add_at = self.point(
            panel,
            (left["x"] + right["x"]) * 0.5,
            1.0 - (left["y"] + right["y"]) * 0.5,
        )
        self.click(f"rc:{add_at[0]},{add_at[1]}")
        added_state = self.inspect("waveMesh")
        added = self.parameters(added_state)
        added_vertex_count = added_state["trimesh"]["vertexCount"]
        assert added_vertex_count > initial_vertex_count
        self.assert_trimesh_slice(added_state, "Trimesh slice after cube addition")
        panel = self.target("expanded:waveMesh.panel2D")
        self.capture("trimesh-01-added", panel)

        initial_intercepts = state["trimesh"]["panelIntercepts"]
        added_intercepts = added_state["trimesh"]["panelIntercepts"]
        source_intercept = max(
            added_intercepts,
            key=lambda point: min(abs(point["x"] - before["x"]) for before in initial_intercepts),
        )
        selected_state = added_state
        self.capture("trimesh-02-selected-added-vertex", panel)
        selected = {
            parameter["id"]: parameter["value"]
            for parameter in selected_state["trimesh"]["selectedVertexParameters"]
        }
        source = self.point(panel, source_intercept["x"], 1.0 - source_intercept["y"])
        collision_target = min(
            initial_intercepts,
            key=lambda point: abs(point["x"] - source_intercept["x"]),
        )
        collision_point = self.point(panel, collision_target["x"], 1.0 - collision_target["y"])
        self.drag(source, collision_point)
        collision_state = self.inspect("waveMesh")
        self.assert_trimesh_slice(collision_state, "Trimesh slice after collision rejection")
        self.capture("trimesh-03-collision-rejected", panel)
        collision_intercepts = sorted(
            collision_state["trimesh"]["panelIntercepts"], key=lambda point: point["x"]
        )
        minimum_gap = min(
            right["x"] - left["x"]
            for left, right in zip(collision_intercepts, collision_intercepts[1:])
        )
        assert minimum_gap > 0.002, ("Trimesh collision created overlapping intercepts", minimum_gap)

        source_after_collision = min(
            collision_intercepts,
            key=lambda point: abs(point["x"] - source_intercept["x"]),
        )
        source = self.point(
            panel, source_after_collision["x"], 1.0 - source_after_collision["y"]
        )
        destination = self.point(
            panel,
            min(0.95, source_after_collision["x"] + 0.025),
            1.0 - min(0.9, source_after_collision["y"] + 0.08),
        )
        topology_before_drag = self.parameters(collision_state)["mesh.topology"]
        self.drag(source, destination)
        moved_state = self.inspect("waveMesh")
        self.assert_trimesh_slice(moved_state, "Trimesh slice after vertex drag")
        self.capture("trimesh-04-moved", panel)
        moved = {
            parameter["id"]: parameter["value"]
            for parameter in moved_state["trimesh"]["selectedVertexParameters"]
        }
        assert self.parameters(moved_state)["mesh.topology"] != topology_before_drag, (
            "Trimesh vertex did not move after collision rejection",
            moved,
            source,
            destination,
            collision_target,
            collision_state["trimesh"],
            moved_state["trimesh"],
        )
        topology_before_parameter_edit = self.parameters(moved_state)["mesh.topology"]
        self.capture("trimesh-05-before-parameter", self.target("canvas"))

        amp_slider = self.target("expanded:waveMesh.trimeshVertexParameter.vertex.amp")
        moved_amp = float(moved["vertex.amp"])
        amp_target = max(0.05, moved_amp - 0.08) if moved_amp > 0.5 else min(0.95, moved_amp + 0.08)
        self.primary_click(self.point(amp_slider, amp_target, 0.5))
        clicked_parameter_state = self.inspect("waveMesh")
        clicked_parameters = {
            parameter["id"]: parameter["value"]
            for parameter in clicked_parameter_state["trimesh"]["selectedVertexParameters"]
        }
        assert abs(float(clicked_parameters["vertex.amp"]) - amp_target) < 0.03, (
            amp_target,
            clicked_parameters["vertex.amp"],
            amp_slider,
        )
        moved_amp = float(clicked_parameters["vertex.amp"])
        amp_target = 0.85 if moved_amp < 0.5 else 0.15
        amp_source = self.point(amp_slider, moved_amp, 0.5)
        amp_point = self.point(amp_slider, amp_target, 0.5)
        self.drag(amp_source, amp_point)
        parameter_state = self.inspect("waveMesh")
        self.assert_trimesh_slice(parameter_state, "Trimesh slice after parameter edit")
        parameter_edited = {
            parameter["id"]: parameter["value"]
            for parameter in parameter_state["trimesh"]["selectedVertexParameters"]
        }
        assert abs(float(parameter_edited["vertex.amp"]) - float(moved["vertex.amp"])) > 0.01, (
            moved["vertex.amp"],
            parameter_edited["vertex.amp"],
            amp_target,
            amp_slider,
            amp_source,
            amp_point,
        )
        topology_after_parameter_edit = self.parameters(parameter_state)["mesh.topology"]
        assert topology_after_parameter_edit != topology_before_parameter_edit

        red_slider = self.target("expanded:waveMesh.trimeshMorphRail.red")
        red_point = self.point(red_slider, 0.72, 0.5)
        old_red = float(added["red"])
        self.primary_click(red_point)
        morphed = self.parameters(self.inspect("waveMesh"))
        assert abs(float(morphed["red"]) - old_red) > 0.01

        selected_phase = float(parameter_edited["vertex.phase"])
        selected_amp = float(parameter_edited["vertex.amp"])
        selected_point = self.point(panel, selected_phase, 1.0 - selected_amp)
        self.click(f"c:{selected_point[0]},{selected_point[1]}", "kp:delete")
        deleted_state = self.inspect("waveMesh")
        self.assert_trimesh_slice(deleted_state, "Trimesh slice after vertex deletion")
        deleted_count = deleted_state["trimesh"]["vertexCount"]
        assert deleted_count < added_vertex_count

        panel = self.target("expanded:waveMesh.panel2D")
        deleted_intercepts = sorted(
            deleted_state["trimesh"]["panelIntercepts"], key=lambda point: point["x"]
        )
        second_left, second_right = max(
            zip(deleted_intercepts, deleted_intercepts[1:]),
            key=lambda pair: pair[1]["x"] - pair[0]["x"],
        )
        second_add = self.point(
            panel,
            (second_left["x"] + second_right["x"]) * 0.5,
            1.0 - (second_left["y"] + second_right["y"]) * 0.5,
        )
        self.click(f"rc:{second_add[0]},{second_add[1]}")
        final_state = self.inspect("waveMesh")
        self.assert_trimesh_slice(final_state, "Trimesh slice after second cube addition")
        final_count = final_state["trimesh"]["vertexCount"]
        assert final_count > deleted_count
        final_parameters = self.parameters(final_state)
        assert "mesh.topology" in final_parameters
        assert not any(parameter_id.startswith("mesh.vertex.") for parameter_id in final_parameters)
        topology = json.loads(final_parameters["mesh.topology"])
        assert len(topology["vertices"]) == final_count
        assert len(topology["cubes"]) > 0
        assert all(len(cube["vertexIds"]) == 8 for cube in topology["cubes"])

        saved_path = os.path.join(tempfile.gettempdir(), "cycle-v2-native-trimesh.cyclegraph")
        self.command({"command": "saveGraph", "path": saved_path})
        self.command({"command": "openGraph", "path": saved_path})
        reloaded_state = self.open_editor("waveMesh", trimesh=True)
        reloaded_parameters = self.parameters(reloaded_state)
        assert reloaded_parameters["mesh.topology"] == final_parameters["mesh.topology"]
        assert reloaded_state["trimesh"]["vertexCount"] == final_count

        self.assert_audio_changed(initial_audio, self.audio_samples(), "Trimesh downstream output")

    def causal_trimesh_sequence(self):
        self.command({
            "command": "openGraph",
            "path": os.path.join(REPO, "cycle-v2", "resources", "with-spies.cyclegraph"),
        })
        time.sleep(SETTLE_SECONDS)
        state = self.open_editor("waveMesh", trimesh=True)
        parameters = self.parameters(state)

        if self.graph_state()["probeRefreshMode"] != "On Release":
            refresh_toggle = self.target("probeRefreshMode")
            self.primary_click(self.point(refresh_toggle, 0.5, 0.5))
        assert self.graph_state()["probeRefreshMode"] == "On Release"

        primary_axis = "red"
        if parameters.get("primaryAxis") != primary_axis:
            primary_axis_button = self.target(
                f"expanded:waveMesh.trimeshPrimaryAxis.{primary_axis}"
            )
            self.primary_click(self.point(primary_axis_button, 0.5, 0.5))
            parameters = self.parameters(self.inspect("waveMesh"))
            assert parameters["primaryAxis"] == primary_axis
        primary_slider = self.target(f"expanded:waveMesh.trimeshMorphRail.{primary_axis}")
        primary_value = float(parameters[primary_axis])
        primary_target = 0.25 if primary_value > 0.5 else 0.75
        primary_start = self.causal_sequence(self.graph_state())
        self.primary_click(self.point(primary_slider, primary_target, 0.5))
        primary_events = self.causal_events_since(self.graph_state(), primary_start)
        assert any(
            event["product"] == "LocalSlice" and event["phase"] == "Completed"
            for event in primary_events
        ), (primary_axis, primary_value, primary_target, primary_slider,
            self.parameters(self.inspect("waveMesh")), primary_events)
        assert any(
            event["product"] == "DurablePublication" and event["phase"] == "Completed"
            for event in primary_events
        ), primary_events
        assert not any(
            event["product"] in (
                "CompactPreview",
                "PreviewTraversal",
                "ProbePreview",
                "AudioConfiguration",
            )
            and event["phase"] == "Completed"
            for event in primary_events
        ), primary_events
        self.assert_causal_exactly_once(primary_events, "Trimesh primary-axis morph")

        nonprimary_axis = "red" if primary_axis != "red" else "blue"
        parameters = self.parameters(self.inspect("waveMesh"))
        nonprimary_slider = self.target(
            f"expanded:waveMesh.trimeshMorphRail.{nonprimary_axis}"
        )
        nonprimary_value = float(parameters[nonprimary_axis])
        nonprimary_target = 0.2 if nonprimary_value > 0.5 else 0.8
        nonprimary_start = self.causal_sequence(self.graph_state())
        self.primary_click(self.point(nonprimary_slider, nonprimary_target, 0.5))
        nonprimary_state = self.graph_state_until(lambda graph_state: any(
            int(event["sequence"]) > nonprimary_start
            and event["product"] == "PreviewTraversal"
            and event["phase"] == "Completed"
            for event in graph_state["causalUpdates"]
        ))
        self.assert_causal_exactly_once(
            self.causal_events_since(nonprimary_state, nonprimary_start),
            "Trimesh non-primary morph",
        )

        refresh_toggle = self.target("probeRefreshMode")
        self.primary_click(self.point(refresh_toggle, 0.5, 0.5))
        assert self.graph_state()["probeRefreshMode"] == "Live"

        editor_state = self.inspect("waveMesh")
        panel = self.target("expanded:waveMesh.panel2D")
        intercepts = sorted(
            editor_state["trimesh"]["panelIntercepts"],
            key=lambda intercept: intercept["x"],
        )
        source_intercept = intercepts[len(intercepts) // 2]
        source = self.point(panel, source_intercept["x"], 1.0 - source_intercept["y"])
        destination = self.point(
            panel,
            min(0.95, source_intercept["x"] + 0.03),
            1.0 - min(0.9, source_intercept["y"] + 0.05),
        )
        mesh_start = self.causal_sequence(self.graph_state())
        self.drag_and_hold(source, destination)
        mesh_state = self.graph_state_until(lambda graph_state: any(
            int(event["sequence"]) > mesh_start
            and event["product"] == "ProbePreview"
            and event["phase"] == "Published"
            for event in graph_state["causalUpdates"]
        ))
        self.assert_causal_exactly_once(
            self.causal_events_since(mesh_state, mesh_start),
            "Trimesh Live panel movement",
        )
        self.release_drag(destination)

    def run(self, sequences):
        self.start()
        try:
            self.command({
                "command": "openGraph",
                "path": os.path.join(REPO, "cycle-v2", "resources", "default.cyclegraph"),
            })
            time.sleep(SETTLE_SECONDS)
            available = {
                "authoring": self.graph_authoring_sequence,
                "waveshaper": self.effect2d_sequence,
                "envelope": self.envelope_sequence,
                "trimesh": self.trimesh_sequence,
                "causal-trimesh": self.causal_trimesh_sequence,
            }
            for sequence in sequences:
                available[sequence]()
        finally:
            self.stop()


if __name__ == "__main__":
    if sys.platform != "darwin":
        raise SystemExit("Native CycleV2 edit smoke requires macOS")
    requested = sys.argv[1:] or ["authoring", "waveshaper", "envelope", "trimesh"]
    unknown = set(requested) - {
        "authoring",
        "waveshaper",
        "envelope",
        "trimesh",
        "causal-trimesh",
    }
    if unknown:
        raise SystemExit(f"Unknown native edit smoke sequence: {', '.join(sorted(unknown))}")
    NativeEditSmoke().run(requested)
    print("CycleV2 native edit smoke passed")
