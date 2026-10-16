# Disco Dialogues

Disco Dialogues gives **Pathologic Classic HD** a new conversation screen inspired by Disco Elysium. Dialogue and replies appear in a scrolling panel on the right, with the character visible on the left. You can click the character portrait to read more about them, and press `1` through `6` to choose a reply.

The mod also lets an NPC finish speaking while you choose and advance replies. If the next reply tries to start another voice line before the current one finishes, that new line may be skipped. Your saves and dialogue choices are unaffected.

## Install

1. Close the game.
2. Install the mod ZIP with Utopian Launcher. Select `DiscoDialogues.dll` and its required shared dependency, `OynonTools.dll`.
3. Start the game.

The new screen supports 1920×1080, 1600×900, 1366×768, 1024×768, and 800×600. At other resolutions, the game uses its original dialogue screen.

## Settings

Edit `bin/Final/mods/DiscoDialogues.ini` while the game is closed. Restart the game after changing camera or audio settings.

```ini
[General]
Enabled=1

[Camera]
FrameNPCOnLeft=1
LeftRotation=5
TopRotation=3

[Audio]
PreserveNpcSpeech=1
```

- `Enabled=0` turns off the new dialogue screen. Restart the game after changing it.
- `FrameNPCOnLeft=0` keeps the new screen but leaves the dialogue camera as it is in the game.
- `LeftRotation` moves the character horizontally. `50` keeps the previous view; smaller values move them left. `0` aims at the left edge of the free area, and negative values can push them partly or fully off screen. Allowed values: **−50 to 50**.
- `TopRotation` adjusts where the camera looks. Positive numbers look higher; negative numbers look lower. `0` keeps the previous view. Allowed values: **−5 to 5 degrees**.
- `PreserveNpcSpeech=0` restores the game's original behavior of interrupting NPC speech when you advance a reply.

The default camera values are `5` and `3`. Set `LeftRotation=50` and `TopRotation=0` for the original framing. Older INI files using `LeftAreaPosition` and `VerticalRotation` still work; the new names take priority if both are present. Values outside the allowed ranges use the nearest limit: for example, `−75` becomes `−50` and `25` degrees becomes `5`. These settings adjust the direction of the dialogue camera; the game still controls its position, distance, and field of view.

## Compatibility

The same package supports the verified Steam and GOG versions of **Pathologic Classic HD**. Other game builds and the original 2005 release are not supported. Automated checks cover both supported versions; an in-game visual check of the current camera changes is still pending.

## Build from source

You need CMake, MSVC with Win32 tools, Python 3, and the `OynonTools`, `UtopianInventory`, `pathologic_lua_compiler`, and `pathologic_re` repositories next to this one. Python also needs `pefile` and the compiler's dependencies.

```powershell
./build-release.ps1 -GameRoot '<game folder>'
```

To check both Steam and GOG game files, add `-CompatibilityGameRoots '<other game folder>'`. The finished ZIP and validation reports go in `release/`.

For implementation details, see [the runtime integration notes](HOOK_EXTRACTION.md).
