#include "dialog_audio.h"
#include "choice_wav.h"
#include <windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <mutex>

namespace disco_dialogues {
namespace {
std::mutex audioMutex;
HWAVEOUT output = nullptr;
WAVEHDR header{};
ChoiceWav cached;
bool warned = false;
bool initialized = false;
void Warn(const wchar_t* reason) {
    if (warned) return;
    warned = true;
    ::OutputDebugStringW(L"DiscoDialogues: choice sound unavailable: ");
    ::OutputDebugStringW(reason);
    ::OutputDebugStringW(L". Selection remains enabled; check [Audio] in DiscoDialogues.ini.\n");
}
}
void InitializeDialogAudio(const std::wstring& iniPath) {
    std::lock_guard<std::mutex> lock(audioMutex);
    if (initialized) return;
    initialized = true;
    try {
        wchar_t configured[1024]{}, volumeText[64]{};
        ::GetPrivateProfileStringW(L"Audio", L"DialogChoiceSoundPath", L"sounds\\DialogOptionClick.wav",
            configured, 1024, iniPath.c_str());
        ::GetPrivateProfileStringW(L"Audio", L"DialogChoiceSoundVolume", L"0.35",
            volumeText, 64, iniPath.c_str());
        if (!*configured) return; // Empty path explicitly disables sound.
        wchar_t* end = nullptr;
        float volume = std::wcstof(volumeText, &end);
        if (end == volumeText || *end || !std::isfinite(volume)) volume = 0.35f;
        volume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
        if (volume == 0) return;
        const std::filesystem::path relative(configured);
        if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
            Warn(L"use a path relative to the mod directory"); return;
        }
        const auto path = std::filesystem::path(iniPath).parent_path() / relative;
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) { Warn(path.c_str()); return; }
        const auto size = stream.tellg();
        if (size < 12 || size > 8 * 1024 * 1024) { Warn(L"invalid WAV size"); return; }
        std::vector<unsigned char> bytes(static_cast<size_t>(size));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())) ||
            !DecodeChoiceWav(bytes, cached, volume)) {
            Warn(L"expected PCM 16-bit mono/stereo WAV, 8–192 kHz, at most 10 seconds"); return;
        }
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = static_cast<WORD>(cached.channels);
        format.nSamplesPerSec = cached.rate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * 2;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        if (::waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            output = nullptr; Warn(L"audio device could not be opened"); return;
        }
        header.lpData = cached.pcm.data();
        header.dwBufferLength = static_cast<DWORD>(cached.pcm.size());
        if (::waveOutPrepareHeader(output, &header, sizeof(header)) != MMSYSERR_NOERROR) {
            ::waveOutClose(output); output = nullptr; Warn(L"audio buffer preparation failed");
        }
        // DLL is pinned for process lifetime; the same prepared PCM buffer and
        // device survive dialogue teardown, including the final Leave choice.
    } catch (...) { Warn(L"audio initialization failed"); }
}
void PlayDialogChoiceSound() noexcept {
    try {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (!output) return;
        // Restart this UI cue on rapid valid choices, without overlapping copies.
        if (::waveOutReset(output) != MMSYSERR_NOERROR ||
            ::waveOutWrite(output, &header, sizeof(header)) != MMSYSERR_NOERROR)
            Warn(L"audio playback failed");
    } catch (...) { /* An optional cue must never interrupt SelectAnswer. */ }
}
}
