// client_test.exe - exercises the outloud_host pipe protocol end to end.
// Usage: client_test.exe [--lang=enu] [--variant=1] [--rate=50] [--sr=2]
//                        [--text=...] [--out=out.wav] [--indexes]
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "../src/engine_client.h"
#include "../src/voice_data.hpp"
#include "../src/utils.hpp"

using namespace Outloud;

static std::vector<char> g_pcm;

static bool on_audio(const char* pcm, uint32_t bytes, void*)
{
    g_pcm.insert(g_pcm.end(), pcm, pcm + bytes);
    return true;
}

static void on_index(int32_t index, void*)
{
    printf("  index %d at byte %zu\n", index, g_pcm.size());
}

int wmain(int argc, wchar_t** argv)
{
    std::string lang = "enu";
    int variant = 1, rate = 50, sr = 1;
    std::wstring text = L"Hello, this is a client test of the Outloud pipe protocol.";
    std::wstring out = L"client_test.wav";
    bool withIndexes = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a.rfind(L"--lang=", 0) == 0) lang = utils::wstring_to_string(a.substr(7));
        else if (a.rfind(L"--variant=", 0) == 0) variant = _wtoi(a.c_str() + 10);
        else if (a.rfind(L"--rate=", 0) == 0) rate = _wtoi(a.c_str() + 7);
        else if (a.rfind(L"--sr=", 0) == 0) sr = _wtoi(a.c_str() + 5);
        else if (a.rfind(L"--text=", 0) == 0) text = a.substr(7);
        else if (a.rfind(L"--out=", 0) == 0) out = a.substr(6);
        else if (a == L"--indexes") withIndexes = true;
    }

    const int li = voices::language_index_from_code(lang.c_str());
    if (li < 0) {
        printf("unknown language %s\n", lang.c_str());
        return 1;
    }

    EngineClient client;
    printf("arch: %d-bit client\n", (int)(sizeof(void*) * 8));
    if (!client.ping()) {
        printf("FAIL: cannot reach or launch outloud_host\n");
        return 1;
    }
    printf("host: reachable\n");

    OutloudSpeakRequest req = {};
    req.dialect = voices::languages[li].dialect;
    req.variant = variant;
    req.sampleRate = sr;
    req.abbrDict = 1;
    req.rate = rate;
    req.pitch = req.inflection = req.headSize = req.roughness = req.breathiness = -1;
    req.volume = 92;

    std::vector<SpeakSegment> segments;
    if (text.find(L'|') != std::wstring::npos) {
        // Manual segmentation mode: '|' splits text into separate segments,
        // with an index marker between them - reproduces how the SAPI DLL
        // interleaves word markers into the text stream.
        std::wstring part;
        int idx = 1;
        for (size_t i = 0; i <= text.size(); ++i) {
            if (i < text.size() && text[i] != L'|') {
                part += text[i];
                continue;
            }
            if (!part.empty()) {
                SpeakSegment seg;
                seg.text = utils::wstring_to_codepage(part, voices::languages[li].codepage);
                segments.push_back(seg);
                part.clear();
            }
            if (i < text.size()) {
                SpeakSegment mark;
                mark.isIndex = true;
                mark.index = idx++;
                segments.push_back(mark);
            }
        }
    } else if (withIndexes) {
        // Split into words with an index before each, mimicking the SAPI DLL.
        std::wstring word;
        int idx = 1;
        for (size_t i = 0; i <= text.size(); ++i) {
            if (i < text.size() && !iswspace(text[i])) {
                word += text[i];
            } else if (!word.empty()) {
                SpeakSegment mark;
                mark.isIndex = true;
                mark.index = idx++;
                segments.push_back(mark);
                SpeakSegment seg;
                seg.text = utils::wstring_to_codepage(word + L" ", voices::languages[li].codepage);
                segments.push_back(seg);
                word.clear();
            }
        }
    } else {
        SpeakSegment seg;
        seg.text = utils::wstring_to_codepage(text, voices::languages[li].codepage);
        segments.push_back(seg);
    }

    bool aborted = false;
    if (!client.speak(req, segments, on_audio, on_index, nullptr, aborted)) {
        printf("FAIL: speak transport error\n");
        return 1;
    }
    if (g_pcm.empty()) {
        printf("FAIL: no audio\n");
        return 1;
    }

    short peak = 0;
    const short* s = reinterpret_cast<const short*>(g_pcm.data());
    for (size_t i = 0; i < g_pcm.size() / 2; ++i) {
        short a = s[i] < 0 ? -s[i] : s[i];
        if (a > peak) peak = a;
    }

    const DWORD hz = sr == 0 ? 8000 : 11025; // engine truly renders only 8/11 kHz
    FILE* f = _wfopen(out.c_str(), L"wb");
    if (f) {
        DWORD dataSize = (DWORD)g_pcm.size(), chunk = 36 + dataSize, fmtSize = 16, byteRate = hz * 2;
        WORD fmt = 1, ch = 1, align = 2, bits = 16;
        fwrite("RIFF", 1, 4, f); fwrite(&chunk, 4, 1, f); fwrite("WAVE", 1, 4, f);
        fwrite("fmt ", 1, 4, f); fwrite(&fmtSize, 4, 1, f); fwrite(&fmt, 2, 1, f);
        fwrite(&ch, 2, 1, f); fwrite(&hz, 4, 1, f); fwrite(&byteRate, 4, 1, f);
        fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&dataSize, 4, 1, f);
        fwrite(g_pcm.data(), 1, dataSize, f);
        fclose(f);
    }

    printf("OK: %zu bytes @ %lu Hz (%.2f s, peak %d) -> %S\n",
        g_pcm.size(), hz, (double)g_pcm.size() / (hz * 2), (int)peak, out.c_str());
    return peak > 200 ? 0 : 2;
}
