# Changelog

## Unreleased

- Fix transparent dialogue backgrounds in non-Russian versions by using a bundled black swatch instead of language-specific stock atlas coordinates at all three supported resolutions.

## 1.0.0

Refactoring rebuild:

- Move verified runtime interception and native ABI access into OynonTools through `OynonRuntimeApi.h`.
- Keep dialogue classification, framing, speech-preservation rules, drawing commands and diagnostics in DiscoDialogues.
- Build and package the matching OynonTools DLL; use the same dependency for Inventory Overhaul compatibility artifacts.
- Document the shared runtime API, callback lifetime and single-owner limitations in OynonTools README.
- Add camera/UI adapter integration coverage, including alternative client callbacks and restoration after native exceptions.

Rebuild validation: Win32 Release, nine C++ tests, 23 Lua behavior check groups at three viewport sizes, unchanged bundled audio, native speech fixtures and six cached HD camera call sites passed. Existing Lua/BIN assets are unchanged. Full script re-extraction/recompilation was not repeated because the external PathologicScript parser is unavailable; in-game validation remains pending.

Original 1.0.0 changes:

- Preserve ongoing NPC speech while selecting dialogue replies immediately.
- Guard dialogue exit against restarting an already playing NPC voice, including buffered and streamed voices.
- Play the reply selection sound through the native Lua `PlaySound` path. Ship one audio file: `data/Sounds/disco-dialogs-action.ogg`.
- Remove the custom C++ audio playback implementation and obsolete sound settings.
- Support keys `1`–`5` for selecting the corresponding dialogue reply.
- Expand speech, native ABI, Lua behavior, package, and Launcher compatibility checks.

Validation: release build, six C++ tests, Lua compilation and behavior checks, native ABI checks, and Launcher installation/removal passed. In-game confirmation of the latest dialogue-exit speech fix remains pending.
