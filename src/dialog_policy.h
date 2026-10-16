#pragma once

#include "camera_framing.h"

namespace disco_dialogues {
bool InstallDialogCamera();
bool InstallDialogFeed();
bool InstallDialogSpeech();

void InitializeDialogSpeechTrace(const wchar_t* path);

float ResolveDialogLayoutFraction();
CameraFramingSettings ResolveDialogCameraSettings();

bool ShouldPreserveDialogSpeech();
}
