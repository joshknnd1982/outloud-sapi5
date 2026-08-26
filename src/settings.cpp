#include "settings.h"
#include "voice_data.hpp"
#include <shlobj.h>
#include <stdio.h>

namespace Outloud {

namespace {

std::wstring app_data_dir()
{
    wchar_t buf[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf))) {
        GetTempPathW(MAX_PATH, buf);
    }
    std::wstring dir(buf);
    if (!dir.empty() && dir.back() != L'\\') {
        dir += L'\\';
    }
    dir += L"OutloudSAPI";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

int read_int(const wchar_t* section, const wchar_t* key, int def, const std::wstring& file)
{
    return static_cast<int>(GetPrivateProfileIntW(section, key, def, file.c_str()));
}

void write_int(const wchar_t* section, const wchar_t* key, int value, const std::wstring& file)
{
    wchar_t buf[32];
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, file.c_str());
}

int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

Settings g_cached;
FILETIME g_cachedTime = {};
bool g_cacheValid = false;
CRITICAL_SECTION g_cacheLock;
INIT_ONCE g_lockInit = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK init_lock(PINIT_ONCE, PVOID, PVOID*)
{
    InitializeCriticalSection(&g_cacheLock);
    return TRUE;
}

}

std::wstring SettingsStore::path()
{
    return app_data_dir() + L"\\settings.ini";
}

std::wstring SettingsStore::log_dir()
{
    std::wstring dir = app_data_dir() + L"\\logs";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

bool SettingsStore::file_time(FILETIME& ft)
{
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(path().c_str(), GetFileExInfoStandard, &fad)) {
        return false;
    }
    ft = fad.ftLastWriteTime;
    return true;
}

Settings SettingsStore::load()
{
    const std::wstring file = path();
    Settings s;

    // Language is stored as its three-letter code for readability.
    wchar_t langBuf[16] = {};
    GetPrivateProfileStringW(L"speech", L"language", L"enu", langBuf, 16, file.c_str());
    char langCode[16] = {};
    WideCharToMultiByte(CP_ACP, 0, langBuf, -1, langCode, sizeof(langCode), nullptr, nullptr);
    const int li = voices::language_index_from_code(langCode);
    s.languageIndex = (li >= 0) ? li : 0;

    s.variant = clamp_int(read_int(L"speech", L"variant", 1, file), 1, voices::variant_count);
    s.rate = clamp_int(read_int(L"speech", L"rate", 50, file), 0, 250);
    s.pitch = clamp_int(read_int(L"speech", L"pitch", 65, file), 0, 100);
    s.inflection = clamp_int(read_int(L"speech", L"inflection", 30, file), 0, 100);
    s.headSize = clamp_int(read_int(L"speech", L"headSize", 50, file), 0, 100);
    s.roughness = clamp_int(read_int(L"speech", L"roughness", 0, file), 0, 100);
    s.breathiness = clamp_int(read_int(L"speech", L"breathiness", 0, file), 0, 100);
    s.volume = clamp_int(read_int(L"speech", L"volume", 92, file), 0, 100);

    s.backquoteVoiceTags = read_int(L"options", L"backquoteVoiceTags", 0, file) != 0;
    s.abbreviationExpansion = read_int(L"options", L"abbreviationExpansion", 1, file) != 0;
    s.phrasePrediction = read_int(L"options", L"phrasePrediction", 0, file) != 0;
    s.pauseMode = clamp_int(read_int(L"options", L"pauseMode", 2, file), 0, 2);
    s.sendParams = read_int(L"options", L"sendParams", 1, file) != 0;
    // Legacy value 2 ("22 kHz") clamps to 11 kHz - the engine cannot really
    // render 22 kHz; that mode mislabels 11 kHz data and chipmunks.
    s.sampleRate = clamp_int(read_int(L"options", L"sampleRate", 1, file), 0, 1);

    s.debugLogging = read_int(L"logging", L"debug", 0, file) != 0;
    return s;
}

bool SettingsStore::save(const Settings& s)
{
    const std::wstring file = path();

    wchar_t langBuf[16];
    MultiByteToWideChar(CP_ACP, 0, voices::languages[clamp_int(s.languageIndex, 0, voices::language_count - 1)].code, -1, langBuf, 16);
    WritePrivateProfileStringW(L"speech", L"language", langBuf, file.c_str());

    write_int(L"speech", L"variant", s.variant, file);
    write_int(L"speech", L"rate", s.rate, file);
    write_int(L"speech", L"pitch", s.pitch, file);
    write_int(L"speech", L"inflection", s.inflection, file);
    write_int(L"speech", L"headSize", s.headSize, file);
    write_int(L"speech", L"roughness", s.roughness, file);
    write_int(L"speech", L"breathiness", s.breathiness, file);
    write_int(L"speech", L"volume", s.volume, file);

    write_int(L"options", L"backquoteVoiceTags", s.backquoteVoiceTags ? 1 : 0, file);
    write_int(L"options", L"abbreviationExpansion", s.abbreviationExpansion ? 1 : 0, file);
    write_int(L"options", L"phrasePrediction", s.phrasePrediction ? 1 : 0, file);
    write_int(L"options", L"pauseMode", s.pauseMode, file);
    write_int(L"options", L"sendParams", s.sendParams ? 1 : 0, file);
    write_int(L"options", L"sampleRate", s.sampleRate, file);

    write_int(L"logging", L"debug", s.debugLogging ? 1 : 0, file);
    return true;
}

const Settings& SettingsStore::current()
{
    InitOnceExecuteOnce(&g_lockInit, init_lock, nullptr, nullptr);
    EnterCriticalSection(&g_cacheLock);

    FILETIME ft = {};
    const bool haveTime = file_time(ft);
    const bool changed = !g_cacheValid ||
        (haveTime && CompareFileTime(&ft, &g_cachedTime) != 0);

    if (changed) {
        g_cached = load();
        if (haveTime) {
            g_cachedTime = ft;
        }
        g_cacheValid = true;
    }

    static thread_local Settings snapshot;
    snapshot = g_cached;
    LeaveCriticalSection(&g_cacheLock);
    return snapshot;
}

}
