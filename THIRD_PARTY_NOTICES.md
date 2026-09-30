# Licensing and third-party notices

ScrapStacks is free software under the **GNU General Public License v3.0** (`LICENSE`), with the modding and linking exceptions in `EXCEPTIONS`.
The shipped `ScrapStacks.dll` statically links GPL-3.0 code, so anyone distributing the DLL must also make the corresponding source available. Linking this repository at the release's tag does that.

## Linked into ScrapStacks.dll

| Component | License | Notes |
|---|---|---|
| [CommonLibF4, Dear-Modding-FO4 fork](https://github.com/Dear-Modding-FO4/commonlibf4) | MIT, © 2019 ryan-rsm-mckenzie | Game type and function definitions |
| [commonlib-shared, Dear-Modding-FO4 fork](https://github.com/Dear-Modding-FO4/commonlib-shared) | GPL-3.0 with modding and linking exceptions | Relocation and hooking core used by CommonLibF4 |

The MIT license text for CommonLibF4 is in `plugin/extern/CommonLibF4/LICENSE`, and the GPL-3.0 text and exceptions for commonlib-shared are in `plugin/extern/CommonLibF4/lib/commonlib-shared/`. Both come with the source.

## Required at runtime, not included

- [Fallout 4 Script Extender (F4SE)](https://f4se.silverlock.org/)
- [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327)

## Used only for development, not distributed

- [doctest](https://github.com/doctest/doctest) (MIT): unit tests.
- [xmake](https://xmake.io) (Apache-2.0): build.
- [capstone](https://www.capstone-engine.org) (BSD) and [pefile](https://github.com/erocarrera/pefile) (MIT): the Python reverse-engineering tools in `tools/`.
- [Steamless](https://github.com/atom0s/Steamless), [JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler) and [DepotDownloader](https://github.com/SteamRE/DepotDownloader): fetching and reading game builds for offline analysis.

## Game content

The repository and releases contain **no Bethesda files**: no executables, assets, scripts or decompiled code.
Unpacked executables and other analysis output stay in the gitignored `re/` folder on the developer's machine.
FallUI's menu code was read only to stay compatible with it, and none of it is included.

## Thanks

- ianpatt and the F4SE team, meh321 (Address Library), and the CommonLibF4 maintainers.
- FlenarnDev/Engine-Level and DCCStudios/F4-BetterWeaponScrapping, whose open hooks on the workbench scrap path were useful references. No code was copied from either.
- The FallUI authors: ScrapStacks works alongside FallUI's workbench list rather than replacing it.
