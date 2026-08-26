// outloud_test.cpp - registry-free smoke test for the ViaVoice Outloud (ECI) engine.
// Renders one WAV per language and one WAV per voice variant.
// Usage: outloud_test.exe [engineDir] [outDir] [--show-registry] [--mode=cwd|path|none]
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

// ---- ECI API (dynamic binding) ----
typedef void* ECIHand;
enum ECIParam {
    eciSynthMode = 0, eciInputType = 1, eciTextMode = 2, eciDictionary = 3,
    eciSampleRate = 5, eciWantPhonemeIndices = 7, eciRealWorldUnits = 8,
    eciLanguageDialect = 9, eciNumberMode = 10, eciWantWordIndex = 12
};
enum ECIVoiceParam {
    eciGender = 0, eciHeadSize, eciPitchBaseline, eciPitchFluctuation,
    eciRoughness, eciBreathiness, eciSpeed, eciVolume
};
enum ECIMessage {
    eciWaveformBuffer = 0, eciPhonemeBuffer, eciIndexReply, eciPhonemeIndexReply,
    eciWordIndexReply, eciStringIndexReply, eciAudioIndexReply, eciSynthesisBreak
};
enum ECICallbackReturn { eciDataNotProcessed = 0, eciDataProcessed, eciDataAbort };

typedef ECICallbackReturn(__stdcall* ECICallback)(ECIHand, ECIMessage, long, void*);

typedef ECIHand(__stdcall* PFN_eciNewEx)(int);
typedef ECIHand(__stdcall* PFN_eciDelete)(ECIHand);
typedef int(__stdcall* PFN_eciGetAvailableLanguages)(int*, int*);
typedef void(__stdcall* PFN_eciVersion)(char*);
typedef int(__stdcall* PFN_eciGetParam)(ECIHand, int);
typedef int(__stdcall* PFN_eciSetParam)(ECIHand, int, int);
typedef int(__stdcall* PFN_eciGetVoiceParam)(ECIHand, int, int);
typedef int(__stdcall* PFN_eciSetVoiceParam)(ECIHand, int, int, int);
typedef int(__stdcall* PFN_eciCopyVoice)(ECIHand, int, int);
typedef int(__stdcall* PFN_eciGetVoiceName)(ECIHand, int, void*);
typedef int(__stdcall* PFN_eciAddText)(ECIHand, const void*);
typedef int(__stdcall* PFN_eciSynthesize)(ECIHand);
typedef int(__stdcall* PFN_eciSynchronize)(ECIHand);
typedef int(__stdcall* PFN_eciSetOutputBuffer)(ECIHand, int, short*);
typedef void(__stdcall* PFN_eciRegisterCallback)(ECIHand, ECICallback, void*);
typedef int(__stdcall* PFN_eciProgStatus)(ECIHand);
typedef void(__stdcall* PFN_eciErrorMessage)(ECIHand, void*);

static PFN_eciNewEx p_eciNewEx;
static PFN_eciDelete p_eciDelete;
static PFN_eciGetAvailableLanguages p_eciGetAvailableLanguages;
static PFN_eciVersion p_eciVersion;
static PFN_eciGetParam p_eciGetParam;
static PFN_eciSetParam p_eciSetParam;
static PFN_eciGetVoiceParam p_eciGetVoiceParam;
static PFN_eciSetVoiceParam p_eciSetVoiceParam;
static PFN_eciCopyVoice p_eciCopyVoice;
static PFN_eciGetVoiceName p_eciGetVoiceName;
static PFN_eciAddText p_eciAddText;
static PFN_eciSynthesize p_eciSynthesize;
static PFN_eciSynchronize p_eciSynchronize;
static PFN_eciSetOutputBuffer p_eciSetOutputBuffer;
static PFN_eciRegisterCallback p_eciRegisterCallback;
static PFN_eciProgStatus p_eciProgStatus;
static PFN_eciErrorMessage p_eciErrorMessage;

struct LangInfo {
    int dialect;
    const char* code;     // 3-letter id used in file names
    const char* name;
    UINT codepage;        // codepage for text conversion
    const wchar_t* sample;
};

static const LangInfo g_langs[] = {
    { 0x00010000, "enu", "American English",        1252, L"Hello, this is the American English voice of Outloud." },
    { 0x00010001, "eng", "British English",         1252, L"Hello, this is the British English voice of Outloud." },
    { 0x00020000, "esp", "Castilian Spanish",       1252, L"Hola, esta es la voz en espa\x00F1ol castellano." },
    { 0x00020001, "esm", "Latin American Spanish",  1252, L"Hola, esta es la voz en espa\x00F1ol latinoamericano." },
    { 0x00030000, "fra", "French",                  1252, L"Bonjour, ceci est la voix fran\x00E7aise." },
    { 0x00030001, "frc", "Canadian French",         1252, L"Bonjour, ceci est la voix canadienne fran\x00E7aise." },
    { 0x00040000, "deu", "German",                  1252, L"Hallo, dies ist die deutsche Stimme." },
    { 0x00050000, "ita", "Italian",                 1252, L"Ciao, questa \x00E8 la voce italiana." },
    { 0x00060000, "chs", "Mandarin Chinese",         936, L"\x4F60\x597D\xFF0C\x8FD9\x662F\x4E2D\x6587\x666E\x901A\x8BDD\x8BED\x97F3\x3002" },
    { 0x00070000, "ptb", "Brazilian Portuguese",    1252, L"Ol\x00E1, esta \x00E9 a voz em portugu\x00EAs brasileiro." },
    { 0x00080000, "jpn", "Japanese",                 932, L"\x3053\x3093\x306B\x3061\x306F\x3002\x3053\x308C\x306F\x65E5\x672C\x8A9E\x306E\x97F3\x58F0\x3067\x3059\x3002" },
    { 0x00090000, "fin", "Finnish",                 1252, L"Hei, t\x00E4m\x00E4 on suomenkielinen \x00E4\x00E4ni." },
    { 0x000A0000, "kor", "Korean",                   949, L"\xC548\xB155\xD558\xC138\xC694. \xC774\xAC83\xC740 \xD55C\xAD6D\xC5B4 \xC74C\xC131\xC785\xB2C8\xB2E4." },
    { 0x000B0001, "ctt", "Hong Kong Cantonese",      950, L"\x4F60\x597D\xFF0C\x9019\x662F\x9999\x6E2F\x7CB5\x8A9E\x8A9E\x97F3\x3002" },
    { 0x000D0000, "nor", "Norwegian",               1252, L"Hei, dette er den norske stemmen." },
    { 0x000E0000, "swe", "Swedish",                 1252, L"Hej, detta \x00E4r den svenska r\x00F6sten." },
    { 0x000F0000, "dan", "Danish",                  1252, L"Hej, dette er den danske stemme." },
};
static const int g_langCount = sizeof(g_langs) / sizeof(g_langs[0]);

// ---- audio capture ----
static std::vector<short> g_samples;
static short g_buffer[4096];

static ECICallbackReturn __stdcall audioCallback(ECIHand, ECIMessage msg, long lparam, void*)
{
    if (msg == eciWaveformBuffer && lparam > 0) {
        g_samples.insert(g_samples.end(), g_buffer, g_buffer + lparam);
    }
    return eciDataProcessed;
}

static bool writeWav(const char* path, const std::vector<short>& samples, int sampleRate)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    DWORD dataSize = (DWORD)(samples.size() * 2);
    DWORD chunkSize = 36 + dataSize;
    DWORD byteRate = sampleRate * 2;
    WORD blockAlign = 2, channels = 1, bits = 16, fmt = 1;
    DWORD fmtSize = 16, sr = (DWORD)sampleRate;
    fwrite("RIFF", 1, 4, f); fwrite(&chunkSize, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtSize, 4, 1, f); fwrite(&fmt, 2, 1, f);
    fwrite(&channels, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataSize, 4, 1, f);
    if (dataSize) fwrite(samples.data(), 1, dataSize, f);
    fclose(f);
    return true;
}

static void printEciError(ECIHand h, const char* what)
{
    char msg[512] = { 0 };
    int status = p_eciProgStatus ? p_eciProgStatus(h) : -1;
    if (p_eciErrorMessage) p_eciErrorMessage(h, msg);
    printf("  ERROR in %s: status=0x%08X msg=\"%s\"\n", what, status, msg);
}

static std::string toCodepage(const wchar_t* text, UINT cp)
{
    int len = WideCharToMultiByte(cp, 0, text, -1, NULL, 0, NULL, NULL);
    std::string out(len > 0 ? len - 1 : 0, '\0');
    if (len > 1) WideCharToMultiByte(cp, 0, text, -1, &out[0], len, NULL, NULL);
    return out;
}

static int currentSampleRateHz(ECIHand h)
{
    switch (p_eciGetParam(h, eciSampleRate)) {
    case 0: return 8000;
    case 2: return 22050;
    default: return 11025;
    }
}

// Render `text` (already in engine codepage) on handle h into a WAV file.
static bool renderToWav(ECIHand h, const std::string& text, const char* wavPath)
{
    g_samples.clear();
    if (!p_eciAddText(h, text.c_str())) { printEciError(h, "eciAddText"); return false; }
    if (!p_eciSynthesize(h)) { printEciError(h, "eciSynthesize"); return false; }
    if (!p_eciSynchronize(h)) { printEciError(h, "eciSynchronize"); return false; }
    if (g_samples.empty()) { printf("  ERROR: no audio produced\n"); return false; }
    short peak = 0;
    for (short s : g_samples) { short a = s < 0 ? -s : s; if (a > peak) peak = a; }
    int hz = currentSampleRateHz(h);
    if (!writeWav(wavPath, g_samples, hz)) { printf("  ERROR: cannot write %s\n", wavPath); return false; }
    printf("  OK: %s  (%u samples @ %d Hz, %.2f s, peak %d)\n",
        wavPath, (unsigned)g_samples.size(), hz, (double)g_samples.size() / hz, (int)peak);
    return peak > 200; // silence guard
}

static void hideRegistry()
{
    HKEY hEmptyLM = NULL, hEmptyCU = NULL;
    // Create scratch keys before overriding HKCU (handles stay valid afterwards).
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\EmptyLM", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hEmptyLM, NULL);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\EmptyCU", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hEmptyCU, NULL);
    LONG r1 = RegOverridePredefKey(HKEY_LOCAL_MACHINE, hEmptyLM);
    LONG r2 = RegOverridePredefKey(HKEY_CURRENT_USER, hEmptyCU);
    printf("Registry hidden: HKLM override=%ld HKCU override=%ld\n", r1, r2);
}

// Build a volatile registry view from eci.ini and override HKLM/HKCU with it.
// Sections without a backslash map to "...\ViaVoice Outloud 5.0\ECIINI\<sec>";
// sections with backslashes map to "...\ViaVoice Outloud 5.0\<sec>".
static bool buildVirtualRegistry(const std::string& iniPath)
{
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\VirtLM");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\VirtCU");

    HKEY hVirtLM = NULL, hVirtCU = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\VirtLM", 0, NULL,
        REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hVirtLM, NULL) != ERROR_SUCCESS) return false;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\VirtCU", 0, NULL,
        REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hVirtCU, NULL) != ERROR_SUCCESS) return false;

    FILE* f = fopen(iniPath.c_str(), "rb");
    if (!f) { printf("Cannot open %s\n", iniPath.c_str()); return false; }

    const char* rootPath = "Software\\IBM\\ViaVoice Outloud 5.0";
    HKEY hSection = NULL;
    char line[4096];
    int keys = 0, values = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = 0;
        if (!len) continue;
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (!end) continue;
            *end = 0;
            std::string sec = line + 1;
            std::string sub = std::string(rootPath) + "\\";
            if (sec.find('\\') == std::string::npos) sub += "ECIINI\\" + sec;
            else sub += sec;
            if (hSection) { RegCloseKey(hSection); hSection = NULL; }
            if (RegCreateKeyExA(hVirtLM, sub.c_str(), 0, NULL, REG_OPTION_VOLATILE,
                KEY_ALL_ACCESS, NULL, &hSection, NULL) == ERROR_SUCCESS) ++keys;
            else printf("  virtreg: cannot create %s\n", sub.c_str());
        } else if (hSection) {
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char* val = eq + 1;
            if (RegSetValueExA(hSection, line, 0, REG_SZ,
                (const BYTE*)val, (DWORD)strlen(val) + 1) == ERROR_SUCCESS) ++values;
        }
    }
    if (hSection) RegCloseKey(hSection);
    fclose(f);
    printf("Virtual registry: %d keys, %d values from %s\n", keys, values, iniPath.c_str());

    LONG r1 = RegOverridePredefKey(HKEY_LOCAL_MACHINE, hVirtLM);
    LONG r2 = RegOverridePredefKey(HKEY_CURRENT_USER, hVirtCU);
    printf("Overrides applied: HKLM=%ld HKCU=%ld\n", r1, r2);
    return r1 == ERROR_SUCCESS && r2 == ERROR_SUCCESS;
}

int main(int argc, char** argv)
{
    std::string engineDir = "C:\\Users\\joshk\\OneDrive\\dev\\sapivoice\\bin";
    std::string outDir = "C:\\Users\\joshk\\OneDrive\\dev\\sapivoice\\test_output";
    bool hideReg = true;
    std::string mode = "cwd";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--show-registry") hideReg = false;
        else if (a.rfind("--mode=", 0) == 0) mode = a.substr(7);
        else if (a.rfind("--engine=", 0) == 0) engineDir = a.substr(9);
        else if (a.rfind("--out=", 0) == 0) outDir = a.substr(6);
    }

    CreateDirectoryA(outDir.c_str(), NULL);
    printf("Engine dir: %s\nOutput dir: %s\nMode: %s\n", engineDir.c_str(), outDir.c_str(), mode.c_str());

    if (mode == "virtreg") {
        if (!buildVirtualRegistry(engineDir + "\\eci.ini")) {
            printf("FATAL: virtual registry setup failed\n");
            return 1;
        }
    } else if (hideReg) hideRegistry();
    else printf("Registry visible (--show-registry)\n");

    if (mode == "cwd") {
        SetCurrentDirectoryA(engineDir.c_str());
        printf("Set current directory to engine dir\n");
    } else if (mode == "path") {
        char oldPath[32000];
        GetEnvironmentVariableA("PATH", oldPath, sizeof(oldPath));
        std::string newPath = engineDir + ";" + oldPath;
        SetEnvironmentVariableA("PATH", newPath.c_str());
        printf("Prepended engine dir to PATH\n");
    } else {
        printf("No discovery help (mode=none)\n");
    }

    std::string etidevPath = engineDir + "\\etidev.dll";
    HMODULE hEtidev = LoadLibraryExA(etidevPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    printf("etidev.dll: %s\n", hEtidev ? "loaded" : "FAILED");

    std::string eciPath = engineDir + "\\ibmeci.dll";
    HMODULE hEci = LoadLibraryExA(eciPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hEci) {
        printf("FATAL: cannot load %s (err %lu)\n", eciPath.c_str(), GetLastError());
        return 1;
    }
    printf("ibmeci.dll: loaded\n");

#define BIND(n) p_##n = (PFN_##n)GetProcAddress(hEci, #n); if (!p_##n) { printf("FATAL: missing export %s\n", #n); return 1; }
    BIND(eciNewEx) BIND(eciDelete) BIND(eciGetAvailableLanguages) BIND(eciVersion)
    BIND(eciGetParam) BIND(eciSetParam) BIND(eciGetVoiceParam) BIND(eciSetVoiceParam)
    BIND(eciCopyVoice) BIND(eciGetVoiceName) BIND(eciAddText) BIND(eciSynthesize)
    BIND(eciSynchronize) BIND(eciSetOutputBuffer) BIND(eciRegisterCallback)
    BIND(eciProgStatus) BIND(eciErrorMessage)
#undef BIND

    char version[64] = { 0 };
    p_eciVersion(version);
    printf("ECI version: %s\n", version);

    int nLangs = 0;
    p_eciGetAvailableLanguages(NULL, &nLangs);
    std::vector<int> langs(nLangs > 0 ? nLangs : 0);
    if (nLangs > 0) p_eciGetAvailableLanguages(langs.data(), &nLangs);
    printf("Available languages: %d\n", nLangs);
    for (int i = 0; i < nLangs; ++i) printf("  0x%08X\n", langs[i]);
    if (nLangs == 0) {
        printf("WARNING: engine reports no languages - trying eciNewEx anyway\n");
    }

    int fails = 0, rendered = 0;

    // --- one WAV per language, default voice ---
    printf("\n=== Language pass ===\n");
    for (int i = 0; i < g_langCount; ++i) {
        const LangInfo& L = g_langs[i];
        bool avail = (nLangs == 0);
        for (int j = 0; j < nLangs; ++j) if (langs[j] == L.dialect) avail = true;
        if (!avail) { printf("%s (%s): NOT AVAILABLE per engine\n", L.name, L.code); ++fails; continue; }

        printf("%s (%s, 0x%08X):\n", L.name, L.code, L.dialect);
        ECIHand h = p_eciNewEx(L.dialect);
        if (!h) { printf("  ERROR: eciNewEx failed\n"); ++fails; continue; }

        p_eciRegisterCallback(h, audioCallback, NULL);
        p_eciSetOutputBuffer(h, 4096, g_buffer);
        p_eciSetParam(h, eciSynthMode, 1);
        p_eciSetParam(h, eciInputType, 1);
        p_eciSetParam(h, eciSampleRate, 1); // engine renders true audio only at modes 0/1

        // report the 8 preset voice names for this language
        printf("  Voices:");
        for (int v = 1; v <= 8; ++v) {
            char name[64] = { 0 };
            if (p_eciGetVoiceName(h, v, name) && name[0]) printf(" %d=%s", v, name);
            else printf(" %d=?", v);
        }
        printf("\n");

        char wavPath[MAX_PATH];
        sprintf(wavPath, "%s\\lang_%02d_%s.wav", outDir.c_str(), i + 1, L.code);
        if (renderToWav(h, toCodepage(L.sample, L.codepage), wavPath)) ++rendered; else ++fails;
        p_eciDelete(h);
    }

    // --- one WAV per voice variant, American English ---
    printf("\n=== Variant pass (American English) ===\n");
    ECIHand h = p_eciNewEx(0x00010000);
    if (!h) { printf("ERROR: eciNewEx(enu) failed\n"); ++fails; }
    else {
        p_eciRegisterCallback(h, audioCallback, NULL);
        p_eciSetOutputBuffer(h, 4096, g_buffer);
        p_eciSetParam(h, eciSynthMode, 1);
        p_eciSetParam(h, eciInputType, 1);
        p_eciSetParam(h, eciSampleRate, 1); // engine renders true audio only at modes 0/1

        for (int v = 1; v <= 8; ++v) {
            char name[64] = { 0 };
            p_eciGetVoiceName(h, v, name);
            if (!name[0]) sprintf(name, "voice%d", v);
            printf("Variant %d (%s):\n", v, name);
            if (!p_eciCopyVoice(h, v, 0)) { printEciError(h, "eciCopyVoice"); ++fails; continue; }
            char text[256];
            sprintf(text, "Hello, my name is %s. I am voice number %d of Outloud.", name, v);
            char wavPath[MAX_PATH];
            sprintf(wavPath, "%s\\voice_%d_%s.wav", outDir.c_str(), v, name);
            if (renderToWav(h, text, wavPath)) ++rendered; else ++fails;
        }
        p_eciDelete(h);
    }

    printf("\n=== Summary: %d rendered, %d failures ===\n", rendered, fails);
    return fails ? 2 : 0;
}
