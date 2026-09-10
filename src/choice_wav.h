#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace disco_dialogues {
struct ChoiceWav {
    unsigned channels = 0;
    unsigned rate = 0;
    std::vector<char> pcm;
};
inline unsigned Wav16(const unsigned char* p) { return p[0] | (unsigned(p[1]) << 8); }
inline unsigned Wav32(const unsigned char* p) { return Wav16(p) | (Wav16(p + 2) << 16); }
// Deliberately narrow format: uncompressed signed 16-bit PCM WAV, mono/stereo.
// Validate chunk lengths before reading; missing/corrupt audio is optional.
inline bool DecodeChoiceWav(const std::vector<unsigned char>& bytes, ChoiceWav& out, float volume) {
    out = {};
    if (bytes.size() < 12 || bytes.size() > 8 * 1024 * 1024 ||
        std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4)) return false;
    const size_t end = size_t(Wav32(bytes.data() + 4)) + 8;
    if (end < 12 || end > bytes.size()) return false;
    const unsigned char* format = nullptr;
    const unsigned char* pcm = nullptr;
    unsigned pcmSize = 0;
    for (size_t pos = 12; pos < end;) {
        if (end - pos < 8) return false;
        const auto chunk = bytes.data() + pos;
        const unsigned size = Wav32(chunk + 4);
        pos += 8;
        if (size > end - pos) return false;
        if (!std::memcmp(chunk, "fmt ", 4)) {
            if (format || size < 16) return false;
            format = bytes.data() + pos;
        }
        if (!std::memcmp(chunk, "data", 4)) {
            if (pcm) return false;
            pcm = bytes.data() + pos;
            pcmSize = size;
        }
        pos += size;
        if (size & 1) { if (pos == end) return false; ++pos; }
    }
    if (!format || !pcm || !pcmSize || Wav16(format) != 1 || Wav16(format + 14) != 16) return false;
    const unsigned channels = Wav16(format + 2), rate = Wav32(format + 4);
    if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000 ||
        Wav16(format + 12) != channels * 2 || Wav32(format + 8) != rate * channels * 2 ||
        pcmSize % (channels * 2) || pcmSize > rate * channels * 2 * 10) return false;
    if (!(volume >= 0.0f && volume <= 1.0f)) return false;
    out.channels = channels;
    out.rate = rate;
    out.pcm.resize(pcmSize);
    for (unsigned i = 0; i < pcmSize; i += 2) {
        const int sample = Wav16(pcm + i);
        const int signedSample = sample >= 32768 ? sample - 65536 : sample;
        const auto scaled = static_cast<std::uint16_t>(static_cast<int>(signedSample * volume));
        out.pcm[i] = static_cast<char>(scaled & 255);
        out.pcm[i + 1] = static_cast<char>(scaled >> 8);
    }
    return true;
}
}
