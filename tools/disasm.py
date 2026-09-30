"""Targeted disassembly of Fallout4.exe (1.10.163) with Address Library labels.

    python -m tools.disasm func 646841            # by Address Library ID
    python -m tools.disasm func 0xB20C30 --rva    # by RVA
    python -m tools.disasm vtable 1510330 --count 4
    python -m tools.disasm --build 1.11.191 func 2223077

Every RIP-relative operand and call target is annotated with its Address Library
ID and, when CommonLibF4 knows it, its name. Addresses print as RVAs.
"""

from __future__ import annotations

import argparse
import sys
from functools import cached_property
from pathlib import Path

import capstone
import pefile

from tools.addrlib import AddressLibrary
from tools.builds import BUILDS, Build
from tools.names import load_names



class Image:
    def __init__(self, build: Build = BUILDS["1.10.163"]):
        self.build = build
        self.pe = pefile.PE(str(build.exe), fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.lib = AddressLibrary.load(build.addrlib)
        self.names = load_names(runtime=build.runtime)
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.md.detail = True

    @cached_property
    def _sections(self):
        return [
            (s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData), s)
            for s in self.pe.sections
        ]

    def read(self, rva: int, size: int) -> bytes:
        return self.pe.get_data(rva, size)

    def qword(self, rva: int) -> int:
        return int.from_bytes(self.read(rva, 8), "little")

    def label(self, rva: int) -> str:
        id_ = self.lib.id_at(rva)
        if id_ is None:
            return ""
        name = self.names.get(id_)
        return f"id {id_}" + (f" {name}" if name else "")

    def disasm_function(self, rva: int, max_bytes: int = 0x2000) -> list[str]:
        code = self.read(rva, max_bytes)
        out: list[str] = []
        furthest = rva
        for insn in self.md.disasm(code, rva):
            note = self._annotate(insn)
            out.append(f"{insn.address:08X}  {insn.mnemonic:<7} {insn.op_str}" + (f"   ; {note}" if note else ""))
            target = self._branch_target(insn)
            if target is not None and rva <= target < rva + max_bytes:
                furthest = max(furthest, target)
            ends = insn.mnemonic == "ret" or (insn.mnemonic == "jmp" and target is not None and not (rva <= target < rva + max_bytes))
            if ends and insn.address >= furthest:
                nxt = insn.address + insn.size - rva
                if nxt >= len(code) or code[nxt] == 0xCC:
                    break
        return out

    def _branch_target(self, insn) -> int | None:
        if insn.group(capstone.CS_GRP_JUMP) or insn.group(capstone.CS_GRP_CALL):
            op = insn.operands[0] if insn.operands else None
            if op is not None and op.type == capstone.x86.X86_OP_IMM:
                return op.imm
        return None

    def _annotate(self, insn) -> str:
        notes = []
        target = self._branch_target(insn)
        if target is not None and (lbl := self.label(target)):
            notes.append(lbl)
        for op in insn.operands:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                ref = insn.address + insn.size + op.mem.disp
                lbl = self.label(ref)
                if not lbl and insn.mnemonic in ("call", "jmp"):
                    # Indirect through an import thunk or pointer slot.
                    lbl = f"[{ref:08X}]"
                notes.append(f"{ref:08X}" + (f" {lbl}" if lbl else ""))
        return "; ".join(notes)

    def vtable(self, rva: int, count: int) -> list[str]:
        out = []
        for slot in range(count):
            fn = self.qword(rva + slot * 8) - self.base
            lbl = self.label(fn)
            out.append(f"[{slot:#04x}] {fn:08X}" + (f"   ; {lbl}" if lbl else ""))
        return out


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("func")
    f.add_argument("target")
    f.add_argument("--rva", action="store_true", help="target is an RVA, not an ID")
    f.add_argument("--max", type=lambda s: int(s, 0), default=0x2000)
    v = sub.add_parser("vtable")
    v.add_argument("target")
    v.add_argument("--rva", action="store_true")
    v.add_argument("--count", type=int, default=16)
    parser.add_argument("--build", default="1.10.163", choices=sorted(BUILDS))
    args = parser.parse_args(argv)

    image = Image(BUILDS[args.build])
    rva = int(args.target, 0) if args.rva else image.lib.offset(int(args.target, 0))
    lines = image.disasm_function(rva, args.max) if args.cmd == "func" else image.vtable(rva, args.count)
    sys.stdout.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
