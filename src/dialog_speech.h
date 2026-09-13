#pragma once
namespace disco_dialogues {
bool InstallDialogSpeech();
void InitializeDialogSpeechTrace(const wchar_t* path);
// Runtime configuration and supported dialog layout, evaluated per reply.
bool ShouldPreserveDialogSpeech();
}
