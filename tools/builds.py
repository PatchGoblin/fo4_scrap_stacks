"""The Fallout4.exe builds available for offline analysis.

Each entry pairs a Steamless-unpacked exe in re/ with its Address Library database
and the CommonLib runtime index (0 old-gen, 1 next-gen, 2 AE).
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Address Library version-*.bin files for offline analysis (copy them here; gitignored).
ADDRLIB_DIR = Path(os.environ.get("SCRAPSTACKS_ADDRLIB_DIR", ROOT / "re" / "addrlib"))


@dataclass(frozen=True)
class Build:
    version: str
    runtime: int

    @property
    def exe(self) -> Path:
        return ROOT / "re" / f"Fallout4_{self.version.replace('.', '_')}.exe.unpacked.exe"

    @property
    def addrlib(self) -> Path:
        return ADDRLIB_DIR / f"version-{self.version.replace('.', '-')}-0.bin"


# Runtime indices: 0 old-gen, 1 next-gen, 2 AE (CommonLib's VariantID order).
BUILDS = {
    b.version: b
    for b in (
        Build("1.10.163", 0),
        Build("1.10.984", 1),
        Build("1.11.137", 2),
        Build("1.11.159", 2),
        Build("1.11.169", 2),
        Build("1.11.191", 2),
        Build("1.11.221", 2),
        Build("1.11.240", 2),
    )
}


def available() -> list[Build]:
    return [b for b in BUILDS.values() if b.exe.exists() and b.addrlib.exists()]
