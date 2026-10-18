import fcntl
import json
import os
import time
from contextlib import contextmanager

from .identity import identity_matches, process_identity, remove_owned_socket, state_root, valid_instance


SCHEMA = 1


class Conflict(RuntimeError):
    pass


def log_event(action, resource, owner):
    path = state_root() / "events.jsonl"
    with path.open("a") as handle:
        os.chmod(path, 0o600)
        handle.write(json.dumps({"time": time.time(), "action": action,
                                 "resource": resource, "owner": owner}) + "\n")


@contextmanager
def registry():
    root = state_root()
    lock = root / "registry.lock"
    with lock.open("a+") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        path = root / "registry.json"
        if path.exists():
            try:
                data = json.loads(path.read_text())
                if data.get("schema") != SCHEMA or not isinstance(data.get("instances"), dict) or not isinstance(data.get("leases"), dict):
                    raise ValueError("unsupported registry schema")
            except (OSError, ValueError, TypeError) as error:
                raise RuntimeError(f"Invalid registry preserved at {path}: {error}") from error
        else:
            data = {"schema": SCHEMA, "instances": {}, "leases": {}}
        yield data
        temporary = root / f"registry.{os.getpid()}.tmp"
        try:
            with temporary.open("w") as output:
                os.chmod(temporary, 0o600)
                json.dump(data, output, indent=2, sort_keys=True)
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)
        fcntl.flock(handle, fcntl.LOCK_UN)


def instance_key(short_id, product):
    return f"{short_id}/{product}"


def recover(data):
    for key, record in list(data["instances"].items()):
        coordinator_alive = identity_matches(
            record.get("coordinator_pid", 0), record.get("coordinator_identity", {}))
        if not valid_instance(record) and not coordinator_alive:
            del data["instances"][key]
            for name, lease in list(data["leases"].items()):
                if lease.get("instance") == key and not identity_matches(lease["owner_pid"], lease["owner_identity"]):
                    del data["leases"][name]
            if record.get("socket"):
                remove_owned_socket(record)
    for name, lease in list(data["leases"].items()):
        if lease.get("instance") in data["instances"]:
            continue
        if not identity_matches(lease["owner_pid"], lease["owner_identity"]):
            del data["leases"][name]


def acquire(data, resource, owner, operation, instance=None):
    existing = data["leases"].get(resource)
    if existing:
        raise Conflict(
            f"Resource {resource} is held by workspace {existing['workspace_id']} "
            f"({existing['workspace_path']}), PID {existing.get('app_pid')}, "
            f"run {existing['run_id']}, operation {existing['operation']} "
            f"since {existing['acquired']}. Run scripts/cycle_automation.py status.")
    data["leases"][resource] = {
        **owner, "operation": operation, "instance": instance,
        "acquired": time.time(), "owner_pid": os.getpid(),
        "owner_identity": process_identity(os.getpid()),
    }
    log_event("acquire", resource, data["leases"][resource])


@contextmanager
def lease(resource, owner, operation, instance=None, wait=0):
    deadline = time.monotonic() + wait
    while True:
        with registry() as data:
            recover(data)
            try:
                acquire(data, resource, owner, operation, instance)
                break
            except Conflict:
                if time.monotonic() >= deadline:
                    raise
        time.sleep(min(0.1, max(0, deadline - time.monotonic())))
    try:
        yield
    finally:
        with registry() as data:
            current = data["leases"].get(resource)
            if current and current["run_id"] == owner["run_id"]:
                log_event("release", resource, current)
                del data["leases"][resource]
