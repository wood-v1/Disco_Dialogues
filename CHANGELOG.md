# Changelog

## 1.0.0

- Preserve ongoing NPC speech while selecting dialogue replies immediately.
- Guard dialogue exit against restarting an already playing NPC voice, including buffered and streamed voices.
- Play the reply selection sound through the native Lua `PlaySound` path. Ship one audio file: `data/Sounds/disco-dialogs-action.ogg`.
- Remove the custom C++ audio playback implementation and obsolete sound settings.
- Support keys `1`–`5` for selecting the corresponding dialogue reply.
- Expand speech, native ABI, Lua behavior, package, and Launcher compatibility checks.

Validation: release build, six C++ tests, Lua compilation and behavior checks, native ABI checks, and Launcher installation/removal passed. In-game confirmation of the latest dialogue-exit speech fix remains pending.
