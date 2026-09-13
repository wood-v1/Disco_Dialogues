# Disco Dialogues 1.0

A dialogue UI mod for **Pathologic Classic HD**, inspired by Disco Elysium.

Keys `1`?`5` select the corresponding reply.

Conversations appear in a scrolling panel on the right, with replies below and an interactive character portrait. Supports 1920x1080, 1600x900, and 1366x768.

NPC speech continues while you select replies and advance through dialogue immediately. If that reply requests a replacement speech while the preserved line is still playing, the replacement is skipped. Unloading the NPC and other non-reply events retain their normal speech cleanup.

The mod prevents the world audio resume on dialogue exit from restarting an unpaused voice. It handles both buffered and streamed HD sounds. Paused sounds still resume normally.

## Installation

With the game closed, install the mod ZIP through Utopian Launcher. Select `DiscoDialogues.dll` and its shared dependency, `OynonTools.dll`.

Settings are in `bin/Final/mods/DiscoDialogues.ini`.

The sole choice sound is `data/Sounds/disco-dialogs-action.ogg`. Lua calls the engine's `PlaySound("disco-dialogs-action")`, using the sound resource declared in the dialogue XML. There is no separate C++ audio player or bundled decoder. The old `DialogChoiceSoundPath` and `DialogChoiceSoundVolume` INI options are no longer used. Playback lifetime follows the native UI sound system.

When upgrading from 0.2.8 or earlier, remove the old Disco Dialogues package through the launcher before installing this ZIP so its obsolete sound files are removed.

`[Audio] PreserveNpcSpeech=1` enables this behavior by default; set it to `0` and restart the game to restore vanilla speech interruption. Existing saves and dialogue choices are unchanged.

## Building

Requires CMake, MSVC with x86 tools, Python 3, and the OynonTools, UtopianInventory, pathologic_lua_compiler, and pathologic_re projects. Python dependencies include pefile and those required by the compiler tools.

Place the dependency repositories beside this project and build OynonTools first. Then run:

```powershell
./build-release.ps1 -GameRoot '<game folder>'
```

The mod ZIP is written to `release/`.

## Compatibility

Targets the Steam version of Pathologic Classic HD. The original 2005 release is not supported. In-game validation is still pending.
