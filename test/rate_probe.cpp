// rate_probe.cpp - asks the engine itself what sample rate each eciSampleRate
// mode really produces, by letting it write WAV files and reading the headers.
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

typedef void* ECIHand;
enum { eciSynthMode = 0, eciInputType = 1, eciSampleRate = 5 };

typedef ECIHand(__stdcall* PFN_eciNewEx)(int);
typedef ECIHand(__stdcall* PFN_eciDelete)(ECIHand);
typedef int(__stdcall* PFN_eciSetParam)(ECIHand, int, int);
typedef int(__stdcall* PFN_eciGetParam)(ECIHand, int);
typedef int(__stdcall* PFN_eciAddText)(ECIHand, const void*);
typedef int(__stdcall* PFN_eciSynthesize)(ECIHand);
typedef int(__stdcall* PFN_eciSynchronize)(ECIHand);
typedef int(__stdcall* PFN_eciSetOutputFilename)(ECIHand, const void*);

static PFN_eciNewEx p_new;
static PFN_eciDelete p_del;
static PFN_eciSetParam p_set;
static PFN_eciGetParam p_get;
static PFN_eciAddText p_add;
static PFN_eciSynthesize p_syn;
static PFN_eciSynchronize p_sync;
static PFN_eciSetOutputFilename p_file;

static void hideRegistryWithView(const std::string& engineDir)
{
    HKEY hVirtLM = NULL, hVirtCU = NULL;
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\ProbeLM");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\ProbeCU");
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\ProbeLM", 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hVirtLM, NULL);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudTest\\ProbeCU", 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hVirtCU, NULL);

    std::string ini = engineDir + "\\eci.ini";
    FILE* f = fopen(ini.c_str(), "rb");
    if (!f) { printf("no eci.ini at %s\n", ini.c_str()); exit(1); }
    const char* rootPath = "Software\\IBM\\ViaVoice Outloud 5.0";
    HKEY hSection = NULL;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len && (line[len-1]=='\r'||line[len-1]=='\n')) line[--len]=0;
        if (!len || line[0]==';') continue;
        if (line[0]=='[') {
            char* end = strchr(line, ']'); if (!end) continue; *end=0;
            std::string sec = line+1;
            std::string sub = std::string(rootPath) + "\\";
            if (sec.find('\\')==std::string::npos) sub += "ECIINI\\" + sec; else sub += sec;
            if (hSection) RegCloseKey(hSection);
            RegCreateKeyExA(hVirtLM, sub.c_str(), 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hSection, NULL);
        } else if (hSection) {
            char* eq = strchr(line, '='); if (!eq) continue; *eq=0;
            std::string value = eq+1;
            if (_strnicmp(line, "Path", 4)==0 && value.size() && value[1] != ':')
                value = engineDir + "\\" + value;
            RegSetValueExA(hSection, line, 0, REG_SZ, (const BYTE*)value.c_str(), (DWORD)value.size()+1);
        }
    }
    if (hSection) RegCloseKey(hSection);
    fclose(f);
    RegOverridePredefKey(HKEY_LOCAL_MACHINE, hVirtLM);
    RegOverridePredefKey(HKEY_CURRENT_USER, hVirtCU);
}

int main(int argc, char** argv)
{
    std::string engineDir = argc > 1 ? argv[1] : "C:\\Users\\joshk\\OneDrive\\dev\\sapivoice\\output\\engine";
    hideRegistryWithView(engineDir);

    LoadLibraryExA((engineDir + "\\etidev.dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    HMODULE h = LoadLibraryExA((engineDir + "\\ibmeci.dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) { printf("cannot load ibmeci.dll\n"); return 1; }
    p_new = (PFN_eciNewEx)GetProcAddress(h, "eciNewEx");
    p_del = (PFN_eciDelete)GetProcAddress(h, "eciDelete");
    p_set = (PFN_eciSetParam)GetProcAddress(h, "eciSetParam");
    p_get = (PFN_eciGetParam)GetProcAddress(h, "eciGetParam");
    p_add = (PFN_eciAddText)GetProcAddress(h, "eciAddText");
    p_syn = (PFN_eciSynthesize)GetProcAddress(h, "eciSynthesize");
    p_sync = (PFN_eciSynchronize)GetProcAddress(h, "eciSynchronize");
    p_file = (PFN_eciSetOutputFilename)GetProcAddress(h, "eciSetOutputFilename");

    int dialect = argc > 2 ? strtol(argv[2], nullptr, 16) : 0x00010000;
    for (int mode = 0; mode <= 2; ++mode) {
        ECIHand e = p_new(dialect);
        if (!e) { printf("mode %d: eciNewEx failed\n", mode); continue; }
        p_set(e, eciSynthMode, 1);
        p_set(e, eciInputType, 1);
        int prev = p_set(e, eciSampleRate, mode);
        int readback = p_get(e, eciSampleRate);
        char path[MAX_PATH];
        sprintf_s(path, "%s\\..\\probe_mode%d.wav", engineDir.c_str(), mode);
        p_file(e, path);
        p_add(e, "This is a sample rate probe sentence.");
        p_syn(e);
        p_sync(e);
        p_del(e);

        // read the WAV header the ENGINE wrote
        FILE* f = fopen(path, "rb");
        DWORD hz = 0, dataSize = 0;
        if (f) {
            fseek(f, 24, SEEK_SET); fread(&hz, 4, 1, f);
            fseek(f, 40, SEEK_SET); fread(&dataSize, 4, 1, f);
            fclose(f);
        }
        printf("mode %d: setParam prev=%d readback=%d -> engine WAV header says %lu Hz, %lu bytes (%.2f s)\n",
            mode, prev, readback, hz, dataSize, hz ? (double)dataSize / (hz * 2) : 0.0);
    }
    return 0;
}
