#include "choice_wav.h"
#include <cstdlib>
#include <limits>
#include <fstream>
#include <iterator>
using namespace disco_dialogues;
static void Check(bool condition) { if (!condition) std::abort(); }
static void Put32(std::vector<unsigned char>& b, size_t at, unsigned n) {
    for (unsigned i = 0; i < 4; ++i) b[at + i] = static_cast<unsigned char>(n >> (i * 8));
}
int main(int argc, char** argv) {
    if (argc > 1) {
        std::ifstream audio(argv[1], std::ios::binary);
        Check(audio.good());
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(audio)), {});
        ChoiceWav bundled;
        Check(DecodeChoiceWav(bytes, bundled, 0.35f));
        Check(bundled.rate == 48000 && bundled.channels == 2 && !bundled.pcm.empty());
    }
    std::vector<unsigned char> wav = {
        'R','I','F','F', 40,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0,1,0, 0x40,0x1f,0,0,
        0x80,0x3e,0,0, 2,0,16,0, 'd','a','t','a', 4,0,0,0,
        0,0x80, 0xff,0x7f};
    ChoiceWav out;
    Check(DecodeChoiceWav(wav, out, 0.5f));
    Check(out.rate == 8000 && out.channels == 1 && out.pcm.size() == 4);
    Check(static_cast<unsigned char>(out.pcm[1]) == 0xc0);
    Check(static_cast<unsigned char>(out.pcm[3]) == 0x3f);
    for (size_t length = 0; length < wav.size(); ++length)
        Check(!DecodeChoiceWav({wav.begin(), wav.begin() + length}, out, 1));
    for (size_t at : {size_t(4), size_t(16), size_t(40)}) {
        auto bad = wav; Put32(bad, at, 0xffffffff);
        Check(!DecodeChoiceWav(bad, out, 1));
    }
    for (size_t at : {size_t(20), size_t(22), size_t(28), size_t(32), size_t(34)}) {
        auto bad = wav; bad[at] = 0;
        Check(!DecodeChoiceWav(bad, out, 1));
    }
    Check(!DecodeChoiceWav(wav, out, std::numeric_limits<float>::quiet_NaN()));
    Check(!DecodeChoiceWav(wav, out, -1));
    auto padded = wav;
    padded.insert(padded.begin() + 12, {'J','U','N','K',1,0,0,0,42,0});
    Put32(padded, 4, static_cast<unsigned>(padded.size() - 8));
    Check(DecodeChoiceWav(padded, out, 0));
    for (char value : out.pcm) Check(value == 0);
    return 0;
}
