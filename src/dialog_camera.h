#pragma once
namespace disco_dialogues {
// In-process vtable hooks for the verified HD Game.exe/Engine.dll build only.
bool InstallDialogCamera();
// Resolve the enabled, present layout before CameraTransit, before UI creation.
float ResolveDialogLayoutFraction();
}
