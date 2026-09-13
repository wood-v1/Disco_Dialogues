# Runtime integration boundary

Only the integration introduced for DiscoDialogues was revised. Existing
OynonTools source files and public APIs were left unchanged, including unrelated
uncommitted changes. Both repositories contain parts of this refactor.

## Responsibilities

| OynonTools (`src/runtime/`) | DiscoDialogues (`src/`) |
| --- | --- |
| PE/byte guards, HD ABI addresses and virtual slots, installation and rollback | Which adapters to install, configuration, activation and DLL lifetime |
| Native-instruction scope and bounded native-name access | Recognition of the standard dialogue camera sequence |
| Camera state access and scoped direction replacement | Left-side framing mathematics, up vector, allowed FOV/fraction ranges and layout dimensions |
| UI Execute interception, integer argument access, scoped viewport and original-call forwarding | `DiscoDialoguesPrint`, its 11-argument contract, forwarding nine arguments to `PrintInWidth` |
| Script-event and speech-call interception; raw actor state and controller/script access | Reply event 11/count 2, nested reply scope, actor/lifecycle checks and stop/play suppression |
| Sound collection traversal, native flag matching, stream/buffer identification, pause count and per-entry resume filtering | Exit flags/mask 0/2, voice flag 0x10000, skipping unpaused voice playback |
| No mod log or presentation policy | Speech log path, limit, messages and version label |

The library has no knowledge of DiscoDialogues commands, dialogue patterns,
screen fractions, reply-event selection or rules for preserving speech. HD ABI
constants remain in the library because they describe the engine, not the mod.
The mod uses public call views and accessors; it contains no engine offsets,
virtual dispatch or protected memory writes.

## Public API

The newly introduced `OynonRuntimeApi.h` replaces the provisional
`OynonDialogApi.h` from the first extraction. The older `OynonToolsApi.h` is
unchanged by this revision.

- `OynonInstallCameraTransitHook`: synchronous interception, with accessors for
  the active instruction, native names, direction and FOV. The client chooses
  any replacement direction or forwards unchanged.
- `OynonInstallUIExecuteHook`: synchronous interception of any UI native command.
  The client chooses the native name, argument prefix and optional viewport.
- `OynonInstallScriptAudioHooks`: copies independently optional event, speech and
  sound-resume callbacks. Absent callbacks forward to the original functions.
- `OynonProceed*`: invoke the original function, preserving its arguments/result
  and restoring temporary native state. Explicit `noexcept(false)` preserves
  native exception unwinding through the C-linkage API under MSVC `/EHsc`.
- `OynonReadActorSpeech`, `OynonReadActorScript`, `OynonReadSoundPauseCount`: raw
  observations. Controller dispatch is separate so the client controls its
  lifecycle checks before calling the controller.

Callbacks receive `userData` and borrowed call views. Opaque identities may be
compared inside the synchronous call scope but must not be dereferenced or kept
after it. `nativeCall` is private transport, to be passed back unchanged. A client
calls the matching Proceed function at most once or supplies its own result.
No C++ containers or ownership of allocations crosses this API.

Installation is serialized by the caller, outside DllMain. There is one owner
per adapter, and successful installation cannot be repeated. Callback code,
userData and the library must live until process exit, including after failed
installation because rollback is best effort. DiscoDialogues still pins its DLL.
Safe live removal and simultaneous multi-client ownership remain future work;
restoring a slot alone cannot drain active callbacks. The adapters target the
same verified Classic HD x86 build as before.

## Build and verification

The mod links the CMake `OynonTools` target with public include paths. Packaging
and the Inventory compatibility build use the library produced by that build.
Behavioural tests for framing, classification, speech, camera and feed belong to
the mod. ABI/slot tests belong to OynonTools. Integration fixtures explicitly
compile the adapter implementation with the actual mod callbacks; production
mod targets never include library internals.

The tests cover original-call forwarding, nested scopes, native exceptions,
temporary-state restoration, argument checks and alternative client callbacks
that apply unrelated policies. All 39 original byte guards are retained. The
full game-data ABI script additionally needs the external PathologicScript
parser; cached native fixtures can be checked and run without that parser.
