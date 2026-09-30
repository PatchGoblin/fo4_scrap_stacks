"""Finds vtables by MSVC RTTI class name, independent of Address Library.

MSVC x64 layout:
  TypeDescriptor   { void* vftable; void* spare; char name[] }   name like ".?AVExamineMenu@@"
  CompleteObjectLocator { u32 signature=1, u32 offset, u32 cdOffset,
                          u32 typeDescriptor (RVA), u32 classDescriptor (RVA), u32 self (RVA) }
  vtable[-1] is a pointer to the class's CompleteObjectLocator.

`offset` is where that vtable's subobject sits in the class: 0 for the primary
vtable, non-zero for secondary ones in multiple inheritance.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass


@dataclass(frozen=True)
class VTable:
    rva: int
    offset: int  # subobject offset within the class (0 = primary)
    type_name: str


def _sections(image):
    for s in image.pe.sections:
        yield s.Name.rstrip(b"\0").decode(), s.VirtualAddress, s.get_data()


def find_type_descriptors(image, name_prefix: bytes) -> dict[int, str]:
    """RVA of each TypeDescriptor whose mangled name starts with name_prefix."""
    found = {}
    for _, va, data in _sections(image):
        start = 0
        while (hit := data.find(name_prefix, start)) != -1:
            end = data.find(b"\0", hit)
            found[va + hit - 0x10] = data[hit:end].decode(errors="replace")
            start = hit + 1
    return found


def find_vtables(image, name_prefix: bytes) -> list[VTable]:
    tds = find_type_descriptors(image, name_prefix)
    if not tds:
        return []
    base = image.base
    cols: dict[int, tuple[int, str]] = {}  # COL rva -> (offset, type name)
    for name, va, data in _sections(image):
        if name != ".rdata":
            continue
        for pos in range(0, len(data) - 24, 4):
            sig, off, _cd, td, _cls, self_rva = struct.unpack_from("<IIIIII", data, pos)
            if sig == 1 and td in tds and self_rva == va + pos:
                cols[va + pos] = (off, tds[td])
    vtables = []
    for name, va, data in _sections(image):
        if name != ".rdata":
            continue
        for col_rva, (off, type_name) in cols.items():
            needle = struct.pack("<Q", base + col_rva)
            start = 0
            while (hit := data.find(needle, start)) != -1:
                if hit % 8 == 0:
                    vtables.append(VTable(va + hit + 8, off, type_name))
                start = hit + 1
    return sorted(vtables, key=lambda v: (v.offset, v.rva))
