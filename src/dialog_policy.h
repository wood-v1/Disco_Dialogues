#pragma once

namespace disco_dialogues {
bool InstallDialogCamera();
bool InstallDialogFeed();
bool InstallDialogSpeech();

void InitializeDialogSpeechTrace(const wchar_t* path);

float ResolveDialogLayoutFraction();

bool ShouldPreserveDialogSpeech();
}
