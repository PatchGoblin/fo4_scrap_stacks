"""Reads F4SE Address Library databases (old-gen `version-*.bin`, format V0).

The V0 layout is a u64 count followed by that many (u64 id, u64 offset) pairs, where
offset is relative to the executable's image base. This is the same file CommonLibF4
maps at runtime, so an ID resolved here is the address the plugin will hook.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

DEFAULT_BIN = Path(__file__).resolve().parents[1] / "re" / "addrlib" / "version-1-10-163-0.bin"


@dataclass(frozen=True)
class AddressLibrary:
    by_id: dict[int, int]
    by_offset: dict[int, int]

    @classmethod
    def load(cls, path: Path | str = DEFAULT_BIN) -> "AddressLibrary":
        data = Path(path).read_bytes()
        if len(data) < 8:
            raise ValueError(f"{path}: truncated header")
        (count,) = struct.unpack_from("<Q", data, 0)
        expected = 8 + count * 16
        if len(data) < expected:
            raise ValueError(f"{path}: truncated, expected {expected} bytes, got {len(data)}")

        by_id: dict[int, int] = {}
        by_offset: dict[int, int] = {}
        for id_, off in struct.iter_unpack("<QQ", data[8:expected]):
            by_id[id_] = off
            by_offset.setdefault(off, id_)
        return cls(by_id, by_offset)

    def offset(self, id_: int) -> int:
        try:
            return self.by_id[id_]
        except KeyError:
            raise KeyError(f"ID {id_} is not in the address library") from None

    def id_at(self, offset: int) -> int | None:
        return self.by_offset.get(offset)
