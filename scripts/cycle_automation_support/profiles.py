from dataclasses import dataclass
from pathlib import Path
from typing import Optional


ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class Product:
    name: str
    bundle: Path
    executable_name: str
    bundle_id: str
    fixture_prefix: str
    target: Optional[str]

    @property
    def app(self):
        return (ROOT / self.bundle).resolve()

    @property
    def executable(self):
        return self.app / "Contents" / "MacOS" / self.executable_name


PRODUCTS = {
    "cycle1": Product("cycle1", Path("build/standalone-debug/cycle/Cycle.app"),
                      "Cycle", "com.amaranth.cycle", "cycle-agent-", "Cycle"),
    "cycle-v2": Product("cycle-v2", Path("build/standalone-debug/cycle-v2/CycleV2.app"),
                        "CycleV2", "com.amaranthaudio.cycle-v2", "cycle-v2-agent-", "CycleV2"),
}
