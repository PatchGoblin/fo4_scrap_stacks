"""Every CommonLib Address Library ID the plugin can reach exists on every build.

CommonLib's IDDB::offset() returns the *next* ID's offset for an ID that isn't in
the database, which would make the plugin call the wrong function without any
error. This checks, per build, that each ID in the CommonLib namespaces the plugin
uses resolves exactly, for the runtime that build maps to.
"""

import re

import pytest

from tools.addrlib import AddressLibrary
from tools.builds import available
from tools.names import COMMONLIB_RE

# CommonLib ID namespaces behind the functions the plugin calls (directly or via
# inline CommonLib code): menu, inventory, forms, Scaleform, locks, memory, extra data.
NAMESPACES = {
    "ExamineMenu",
    "WorkbenchMenuBase",
    "BGSInventoryInterface",
    "BGSInventoryItem",
    "BGSInventoryList",
    "PlayerCharacter",
    "TESForm",
    "TESFullName",
    "GFx::Value",
    "BSSpinLock::BSReadWriteLock",
    "BSReadWriteLock",
    "MemoryManager",
    "ExtraDataList",
    "BaseExtraList",
    "BGSObjectInstanceExtra",
}

_ENTRY = re.compile(r"REL::(?:VariantID|ID)\s+(\w+)\s*\{([\d,\s]+)\}")
_NS = re.compile(r"^\s*namespace\s+([\w:]+)\s*$")


def ids_by_namespace():
    """(namespace, name, [og, ng, ae]) for every entry in IDs.h under NAMESPACES."""
    entries = []
    stack, pending, depth = [], None, 0
    for line in (COMMONLIB_RE / "IDs.h").read_text(encoding="utf-8").splitlines():
        if (m := _NS.match(line)):
            pending = m.group(1)
        if (m := _ENTRY.search(line)):
            # Drop the RE::ID / Scaleform::ID roots: ExamineMenu, GFx::Value, ...
            scope = "::".join(n for n, _ in stack if n not in ("RE::ID", "Scaleform::ID"))
            if scope in NAMESPACES:
                ids = [int(v) for v in m.group(2).replace(" ", "").split(",") if v]
                ids += [ids[-1]] * (3 - len(ids))
                entries.append((scope, m.group(1), ids))
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
    return entries


ENTRIES = ids_by_namespace()


def test_the_namespace_list_matches_real_commonlib_entries():
    found = {scope for scope, _, _ in ENTRIES}
    assert {"ExamineMenu", "GFx::Value", "BGSInventoryInterface", "MemoryManager"} <= found


@pytest.mark.parametrize("build", available(), ids=lambda b: b.version)
def test_every_reachable_commonlib_id_exists(build):
    lib = AddressLibrary.load(build.addrlib)
    # ID 0 is CommonLib's marker for "not on this runtime"; the plugin uses none of those.
    missing = [
        f"{scope}::{name} ({ids[build.runtime]})"
        for scope, name, ids in ENTRIES
        if ids[build.runtime] != 0 and ids[build.runtime] not in lib.by_id
    ]
    assert not missing, f"{len(missing)} ID(s) missing on {build.version}: {missing}"
