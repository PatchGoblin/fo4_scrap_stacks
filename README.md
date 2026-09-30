# Scrap Stacks

Scrap a whole stack of weapons or armor at the workbench in one go, and keep your place in the list while you scrap.

- **Shift + Scrap** scraps every copy in the selected row. The confirm dialog shows how many, e.g. `Combat Armor Left Leg (x12)`, and lists the combined components.
- **Plain Scrap** still takes one item, as in vanilla.
- **The cursor stays put.** After any scrap the selection stays on the same spot in the list instead of jumping near the top, and the list doesn't scroll. This works with FallUI's column sorting and the vanilla menu alike.

## Yields are exactly vanilla

Scrap Stacks doesn't compute components itself. It runs the game's own scrap calculation **once per copy**, including your Scrapper perk and each component's scrap scalar, and adds up the results. Scrapping 12 copies gives exactly what scrapping them one at a time would.

## What it won't scrap

When it can't take the whole row, Shift + Scrap takes one copy and says why in the dialog:
- **Equipped copies** are never scrapped.
- **A favorited copy** in the row: `(x1, stack has a favorite)`.
- **Copies with different mods:** the game sometimes shows copies with different attachments as one row. Scrap Stacks takes all the copies matching the one shown, `(x12 of 17, rest have other mods)`, and the next press takes the next group.
- **Unscrappable items** (legendaries, quest items and the like) are still blocked by the game's own check.

## Requirements

- Fallout 4 on Steam, any version F4SE supports: 1.10.163 (old-gen), 1.10.984 (next-gen) or 1.11.x (Anniversary Edition).
- [F4SE](https://f4se.silverlock.org/) for your game version.
- [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327).

No ESP, and nothing is written to your saves. Uninstalling is just removing the mod.

## Compatibility

- **FallUI – Workbench:** supported, including column sorting and category filters.
- **Vanilla workbench menu:** supported.
- **Other mods that hook the same workbench scrap code:** when Scrap Stacks starts, it checks the game code it hooks. If another mod already changed it, Scrap Stacks switches itself off and says why in its log rather than risk a conflict.

## Settings

`Data\F4SE\Plugins\ScrapStacks.ini`:

| Setting | Default | |
|---|---|---|
| `StackModifierKey` | `0x10` (Shift) | Windows virtual-key code to hold while pressing Scrap. `0x11` is Ctrl, `0x12` is Alt. |
| `Diagnostics` | `0` | `1` writes details of every scrap to the log. Turn it on when reporting a problem. |

The log is `Documents\My Games\Fallout4\F4SE\ScrapStacks.log`. If the mod ever does nothing, the log's first lines say which startup check failed.

## How it works

Scrapping at the workbench happens entirely inside the game's compiled code, out of reach of scripts and menu mods. That's why earlier bulk-scrap mods had to be separate scrapping containers or machines. Scrap Stacks is an F4SE plugin that hooks three spots in that code, located at startup rather than hard-coded:
- the game's yield calculation,
- the confirm dialog's label,
- the step between the game's own removal and its list refresh.

Before hooking anything, it checks every assumption it makes against the running game build: class layouts, call targets (cross-checked with Address Library) and memory offsets. If anything doesn't match, it installs nothing. The same check runs offline against every Steam build F4SE supports, so a new game patch can be verified the day it ships.

## Building from source

Source: this repository, licensed GPL-3.0 with a modding exception (see `LICENSE`, `EXCEPTIONS` and `THIRD_PARTY_NOTICES.md`).

```powershell
git clone --recurse-submodules <repo url>
cd plugin
xmake f -m releasedbg --deploy_dir="<MO2>/mods/Scrap Stacks"   # deploy_dir optional
xmake build ScrapStacks
xmake build tests; xmake run tests
```

Needs MSVC (Visual Studio Build Tools with the C++ workload) and [xmake](https://xmake.io).

The offline version tests need the game executables and Address Library files in `re/`, which is not committed: see `tools/fetch_exes.ps1` and `tools/builds.py`. `tools/package.ps1` builds, runs every test and produces the release zip.

## License

GPL-3.0 with a modding exception: see [LICENSE](LICENSE) and [EXCEPTIONS](EXCEPTIONS). Third-party components and their licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
The license covers ScrapStacks' own code only. Fallout 4 and its assets belong to Bethesda Softworks and are not covered or redistributed here.
