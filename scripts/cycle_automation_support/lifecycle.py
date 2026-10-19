import os
import plistlib
import subprocess
import sys
import time
from pathlib import Path

from .identity import (fingerprint, process_identity, remove_owned_socket,
                       socket_fingerprint, socket_path, valid_instance)
from .registry import instance_key, lease, recover, registry
from .session import command


class LaunchFailure(RuntimeError):
    def __init__(self, message, record):
        super().__init__(message)
        self.record = record


def build(profile):
    args = ["cmake", "--build", "--preset", "standalone-debug"]
    if profile.target:
        args.extend(["--target", profile.target])
    args.extend(["--parallel", "10"])
    subprocess.run(args, check=True, stdout=sys.stderr)


def app_metadata(profile):
    if not profile.executable.is_file():
        raise RuntimeError(f"Build the standalone app first: {profile.executable}")
    with (profile.app / "Contents" / "Info.plist").open("rb") as handle:
        info = plistlib.load(handle)
    if info.get("CFBundleIdentifier") != profile.bundle_id or info.get("CFBundleExecutable") != profile.executable_name:
        raise RuntimeError(f"App bundle metadata does not match {profile.name}: {profile.app}")
    return fingerprint(profile.executable)


def compiled_helper(source_name):
    source = Path(__file__).resolve().parents[1] / source_name
    executable = Path(__file__).resolve().parents[2] / ".context" / source.stem
    executable.parent.mkdir(exist_ok=True)
    if not executable.exists() or source.stat().st_mtime_ns > executable.stat().st_mtime_ns:
        subprocess.run(["swiftc", "-parse-as-library", str(source), "-o", str(executable)], check=True)
    return executable


def launch_helper():
    return compiled_helper("launch_cycle_app.swift")


def ping(record):
    if not valid_instance(record):
        return False
    try:
        command(record["socket"], {"command": "ping"}, timeout=2)
        return True
    except (OSError, RuntimeError, ValueError):
        return False


def stop_instance(record):
    if not valid_instance(record):
        return
    try:
        command(record["socket"], {"command": "quit"}, timeout=3)
    except (OSError, RuntimeError):
        pass
    deadline = time.monotonic() + 5
    while valid_instance(record) and time.monotonic() < deadline:
        time.sleep(0.1)
    if valid_instance(record):
        os.kill(record["pid"], 15)
        deadline = time.monotonic() + 3
        while valid_instance(record) and time.monotonic() < deadline:
            time.sleep(0.1)
    if valid_instance(record):
        raise RuntimeError(f"Owned PID {record['pid']} did not exit")


def publish_lease_pid(key, owner, pid):
    with registry() as data:
        for resource in (f"instance:{key}", "lifecycle"):
            active = data["leases"].get(resource)
            if active and active["run_id"] == owner["run_id"]:
                active["app_pid"] = pid


def ensure(profile, owner, short_id, clean=False, wait=0, task=None, scoped=False):
    key = instance_key(short_id, profile.name)
    if scoped:
        key += f"/{owner['run_id']}"
    current_fingerprint = app_metadata(profile)
    with lease(f"instance:{key}", owner, "start", key, wait):
        with registry() as data:
            recover(data)
            existing = data["instances"].get(key)
        if existing and (existing["workspace_id"] != owner["workspace_id"]
                         or existing["workspace_path"] != owner["workspace_path"]):
            raise RuntimeError(
                f"Workspace identity collision at {key}: owned by "
                f"{existing['workspace_id']} ({existing['workspace_path']})")
        if existing and existing["fingerprint"] == current_fingerprint and ping(existing) and not clean:
            publish_lease_pid(key, owner, existing["pid"])
            return task(existing) if task else existing
        with lease("lifecycle", owner, "restart" if existing else "launch", key, wait):
            if existing and valid_instance(existing):
                stop_instance(existing)
            with registry() as data:
                if data["instances"].get(key) == existing:
                    data["instances"].pop(key, None)
            socket = socket_path(short_id, profile.name + (f"-{owner['run_id']}" if scoped else ""))
            if socket.exists():
                if existing is None or valid_instance(existing):
                    raise RuntimeError(f"Socket collision at {socket}; inspect coordinator status")
                socket.unlink()
            result = subprocess.run([str(launch_helper()), str(profile.app), str(socket)],
                                    check=True, capture_output=True, text=True)
            pid = int(result.stdout.strip())
            identity = process_identity(pid)
            if identity is None or identity["executable"] != str(profile.executable.resolve()):
                raise RuntimeError(f"Launched PID {pid} has unexpected executable: {identity}; expected {profile.executable}")
            publish_lease_pid(key, owner, pid)
            record = {
                "schema": 1, "workspace_id": owner["workspace_id"],
                "workspace_path": owner["workspace_path"], "product": profile.name,
                "app": str(profile.app), "executable": str(profile.executable.resolve()),
                "fingerprint": current_fingerprint, "pid": pid,
                "process_identity": identity, "bundle_id": profile.bundle_id,
                "socket": str(socket), "launched": time.time(),
                "coordinator_pid": os.getpid(), "run_id": owner["run_id"],
                "coordinator_identity": process_identity(os.getpid()),
            }
            deadline = time.monotonic() + 20
            ready = False
            while time.monotonic() < deadline:
                if not valid_instance(record):
                    raise LaunchFailure(f"Launched PID {pid} exited before socket readiness", record)
                if socket.is_socket() and ping(record):
                    record["pinged"] = time.time()
                    record["socket_identity"] = socket_fingerprint(socket)
                    with registry() as data:
                        data["instances"][key] = record
                    ready = True
                    break
                time.sleep(0.1)
            if not ready:
                raise LaunchFailure(f"PID {pid} did not answer ping at {socket} within 20s", record)
        if not scoped:
            return task(record) if task else record
        try:
            return task(record) if task else record
        finally:
            with lease("lifecycle", owner, "stop-scoped", key, wait):
                stop_instance(record)
                with registry() as data:
                    if data["instances"].get(key) == record:
                        del data["instances"][key]
                remove_owned_socket(record)


def owned_record(profile, short_id):
    key = instance_key(short_id, profile.name)
    with registry() as data:
        recover(data)
        return data["instances"].get(key)


def stop(profile, owner, short_id, wait=0):
    key = instance_key(short_id, profile.name)
    with lease(f"instance:{key}", owner, "stop", key, wait):
        with lease("lifecycle", owner, "stop", key, wait):
            record = owned_record(profile, short_id)
            if record:
                publish_lease_pid(key, owner, record["pid"])
                if (record["workspace_id"] != owner["workspace_id"]
                        or record["workspace_path"] != owner["workspace_path"]):
                    raise RuntimeError(
                        f"Workspace identity collision at {key}: owned by "
                        f"{record['workspace_id']} ({record['workspace_path']})")
                stop_instance(record)
                with registry() as data:
                    if data["instances"].get(key) == record:
                        del data["instances"][key]
                remove_owned_socket(record)
            return record
