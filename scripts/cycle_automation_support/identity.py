import ctypes
import hashlib
import os
import subprocess
from pathlib import Path

from .profiles import ROOT


def workspace():
    path = ROOT.resolve()
    if os.environ.get("CONDUCTOR_IS_LOCAL") == "1" and os.environ.get("CONDUCTOR_WORKSPACE_ID"):
        identity = os.environ["CONDUCTOR_WORKSPACE_ID"]
    else:
        identity = str(path)
    return identity, str(path), hashlib.sha256(identity.encode()).hexdigest()[:12]


def state_root():
    root = Path(f"/private/tmp/amaranth-cycle-{os.getuid()}")
    root.mkdir(mode=0o700, parents=True, exist_ok=True)
    if root.stat().st_uid != os.getuid() or root.stat().st_mode & 0o077:
        raise RuntimeError(f"Coordinator state is not user-private: {root}")
    return root


def socket_path(short_id, product):
    path = state_root() / "sockets" / short_id / f"{product}.sock"
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    if len(os.fsencode(path)) >= 104:
        raise RuntimeError(f"Unix socket path is too long: {path}")
    return path


def fingerprint(path):
    stat = path.stat()
    return [stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns]


def socket_fingerprint(path):
    try:
        stat = Path(path).stat()
    except FileNotFoundError:
        return None
    return [stat.st_dev, stat.st_ino]


def remove_owned_socket(record):
    path = Path(record["socket"])
    if socket_fingerprint(path) == record.get("socket_identity"):
        path.unlink(missing_ok=True)


def process_identity(pid):
    if pid <= 0:
        return None
    buffer = ctypes.create_string_buffer(4096)
    libproc = ctypes.CDLL("/usr/lib/libproc.dylib")
    if libproc.proc_pidpath(pid, buffer, len(buffer)) <= 0:
        return None

    class BsdInfo(ctypes.Structure):
        _fields_ = [
            ("flags", ctypes.c_uint32), ("status", ctypes.c_uint32),
            ("xstatus", ctypes.c_uint32), ("pid", ctypes.c_uint32),
            ("ppid", ctypes.c_uint32), ("uid", ctypes.c_uint32),
            ("gid", ctypes.c_uint32), ("ruid", ctypes.c_uint32),
            ("rgid", ctypes.c_uint32), ("svuid", ctypes.c_uint32),
            ("svgid", ctypes.c_uint32), ("reserved", ctypes.c_uint32),
            ("comm", ctypes.c_char * 16), ("name", ctypes.c_char * 32),
            ("nfiles", ctypes.c_uint32), ("pgid", ctypes.c_uint32),
            ("pjobc", ctypes.c_uint32), ("tdev", ctypes.c_uint32),
            ("tpgid", ctypes.c_uint32), ("nice", ctypes.c_int32),
            ("start_seconds", ctypes.c_uint64),
            ("start_microseconds", ctypes.c_uint64),
        ]

    info = BsdInfo()
    size = libproc.proc_pidinfo(pid, 3, 0, ctypes.byref(info), ctypes.sizeof(info))
    if size != ctypes.sizeof(info) or info.pid != pid:
        return None
    return {"executable": str(Path(os.fsdecode(buffer.value)).resolve()),
            "started": [info.start_seconds, info.start_microseconds]}


def valid_instance(record):
    return identity_matches(record["pid"], record["process_identity"])


def identity_matches(pid, expected):
    current = process_identity(pid)
    if current is None or current["executable"] != expected.get("executable"):
        return False
    if current["started"] == expected.get("started"):
        return True
    if isinstance(expected.get("started"), str):
        old_start = subprocess.run(
            ["ps", "-p", str(pid), "-o", "lstart="],
            capture_output=True, text=True, check=False).stdout.strip()
        return old_start == expected["started"]
    return False
