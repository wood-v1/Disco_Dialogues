# Disco Dialogues

A dialogue UI mod for **Pathologic Classic HD**, inspired by Disco Elysium.

Conversations appear in a scrolling panel on the right, with replies below and an interactive character portrait. Supports 1920x1080, 1600x900, and 1366x768.

## Installation

With the game closed, install the mod ZIP through Utopian Launcher. Select `DiscoDialogues.dll` and its shared dependency, `OynonTools.dll`.

Settings are in `bin/Final/mods/DiscoDialogues.ini`.

## Building

Requires CMake, MSVC with x86 tools, Python 3, and the OynonTools, UtopianInventory, pathologic_lua_compiler, and pathologic_re projects. Python dependencies include pefile and those required by the compiler tools.

Place the dependency repositories beside this project and build OynonTools first. Then run:

```powershell
./build-release.ps1 -GameRoot '<game folder>'
```

The mod ZIP is written to `release/`.

## Compatibility

Targets the Steam version of Pathologic Classic HD. The original 2005 release is not supported. In-game validation is still pending.
