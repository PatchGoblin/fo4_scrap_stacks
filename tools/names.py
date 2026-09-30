"""Builds an Address Library ID -> symbol name map from CommonLibF4's ID headers.

Entries carry one ID per runtime ({old-gen, next-gen, AE}); `runtime` picks which.
A missing trailing ID repeats the last one, exactly as REL::VariantID does.

Vtable IDs in this CommonLib fork are old-gen only, so vtable names are only
loaded for runtime 0. On other runtimes, find vtables by RTTI (tools/rtti.py).
"""

from __future__ import annotations

import re
from pathlib import Path

COMMONLIB_RE = Path(__file__).resolve().parents[1] / "plugin/extern/CommonLibF4/include/RE"

_NAMESPACE = re.compile(r"^\s*namespace\s+([\w:]+)\s*$")
_ID_ENTRY = re.compile(r"REL::(?:VariantID|ID)\s+(\w+)\s*\{([\d,\s]+)\}")
_VTABLE_ENTRY = re.compile(r"std::array<REL::ID,\s*\d+>\s+(\w+)\s*\{(.*?)\};")
_REL_ID = re.compile(r"REL::ID\((\d+)\)")


def parse_ids_header(text: str, runtime: int = 0) -> dict[int, str]:
    names: dict[int, str] = {}
    # Brace depth at which each open namespace started, so nested namespaces
    # (RE::ID -> ExamineMenu) are tracked. The outer RE::ID one is dropped.
    stack: list[tuple[str, int]] = []
    pending: str | None = None
    depth = 0
    for line in text.splitlines():
        ns = _NAMESPACE.match(line)
        if ns:
            pending = ns.group(1)
        entry = _ID_ENTRY.search(line)
        if entry:
            scope = "::".join(n for n, _ in stack if not n.startswith("RE"))
            name = f"{scope}::{entry.group(1)}" if scope else entry.group(1)
            ids = [int(v) for v in entry.group(2).replace(" ", "").split(",") if v]
            names.setdefault(ids[min(runtime, len(ids) - 1)], name)
        for ch in line:
            if ch == "{":
                depth += 1
                if pending is not None:
                    stack.append((pending, depth))
                    pending = None
            elif ch == "}":
                if stack and stack[-1][1] == depth:
                    stack.pop()
                depth -= 1
    return names


def parse_vtable_header(text: str) -> dict[int, str]:
    names: dict[int, str] = {}
    for match in _VTABLE_ENTRY.finditer(text):
        for index, id_ in enumerate(_REL_ID.findall(match.group(2))):
            suffix = f"[{index}]" if index else ""
            names.setdefault(int(id_), f"vtbl {match.group(1)}{suffix}")
    return names


def load_names(root: Path = COMMONLIB_RE, runtime: int = 0) -> dict[int, str]:
    names = parse_ids_header((root / "IDs.h").read_text(encoding="utf-8"), runtime)
    if runtime == 0:
        for id_, name in parse_vtable_header((root / "IDs_VTABLE.h").read_text(encoding="utf-8")).items():
            names.setdefault(id_, name)
    return names
