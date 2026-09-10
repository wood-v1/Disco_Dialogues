#pragma once
#include <string>
namespace disco_dialogues {
void InitializeDialogAudio(const std::wstring& iniPath);
void PlayDialogChoiceSound() noexcept;
}
