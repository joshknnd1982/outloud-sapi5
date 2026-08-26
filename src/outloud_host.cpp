// outloud_host.exe - 32-bit host process that owns the Outloud (IBM ViaVoice
// ECI) engine and serves synthesis requests to the 32/64-bit SAPI DLLs and the
// configuration utility over a named pipe.
//
// The engine's ViaVoice build reads its configuration exclusively from the
// registry key "Software\IBM\ViaVoice Outloud 5.0". To stay fully
// registry-free, this process builds that key inside a VOLATILE per-user
// scratch key from our own eci.ini and redirects HKLM/HKCU onto it with
// RegOverridePredefKey before the engine loads. Nothing persistent is written
// and the real registry is never consulted by the engine.
//
// Usage: outloud_host.exe            (start serving; exits if already running)
//        outloud_host.exe --shutdown (ask a running host to exit)

#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include "outloud_protocol.h"
#include "settings.h"
#include "outloud_log.h"

// ---------------- ECI API ----------------
typedef void* ECIHand;
typedef void* ECIDictHand;
enum {
    eciSynthMode = 0, eciInputType = 1, eciDictionary = 3, eciSampleRate = 5,
    eciLanguageDialect = 9
};
enum {
    eciGender = 0, eciHeadSize, eciPitchBaseline, eciPitchFluctuation,
    eciRoughness, eciBreathiness, eciSpeed, eciVolume
};
enum { eciWaveformBuffer = 0, eciPhonemeBuffer, eciIndexReply };
enum { eciDataNotProcessed = 0, eciDataProcessed, eciDataAbort };

typedef int(__stdcall* ECICallback)(ECIHand, int, long, void*);

struct EciApi {
    ECIHand(__stdcall* eciNewEx)(int);
    ECIHand(__stdcall* eciDelete)(ECIHand);
    int(__stdcall* eciGetAvailableLanguages)(int*, int*);
    void(__stdcall* eciVersion)(char*);
    int(__stdcall* eciGetParam)(ECIHand, int);
    int(__stdcall* eciSetParam)(ECIHand, int, int);
    int(__stdcall* eciGetVoiceParam)(ECIHand, int, int);
    int(__stdcall* eciSetVoiceParam)(ECIHand, int, int, int);
    int(__stdcall* eciCopyVoice)(ECIHand, int, int);
    int(__stdcall* eciGetVoiceName)(ECIHand, int, void*);
    int(__stdcall* eciAddText)(ECIHand, const void*);
    int(__stdcall* eciInsertIndex)(ECIHand, int);
    int(__stdcall* eciSynthesize)(ECIHand);
    int(__stdcall* eciSynchronize)(ECIHand);
    int(__stdcall* eciStop)(ECIHand);
    int(__stdcall* eciSetOutputBuffer)(ECIHand, int, short*);
    void(__stdcall* eciRegisterCallback)(ECIHand, ECICallback, void*);
    int(__stdcall* eciProgStatus)(ECIHand);
    void(__stdcall* eciErrorMessage)(ECIHand, void*);
    int(__stdcall* eciClearInput)(ECIHand);
    int(__stdcall* eciReset)(ECIHand);
    ECIDictHand(__stdcall* eciNewDict)(ECIHand);
    int(__stdcall* eciSetDict)(ECIHand, ECIDictHand);
    int(__stdcall* eciLoadDict)(ECIHand, ECIDictHand, int, const void*);
};

namespace {

EciApi g_eci = {};
ECIHand g_handle = nullptr;
HMODULE g_eciModule = nullptr;

std::wstring g_exeDir;
std::wstring g_engineDir;
std::string g_engineDirA;

constexpr int AUDIO_BUFFER_SAMPLES = 4096;
short g_audioBuffer[AUDIO_BUFFER_SAMPLES];

bool g_shutdownRequested = false;

// Per-utterance streaming state used by the ECI callback.
struct StreamContext {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    bool aborted = false;
    ULONGLONG bytesSent = 0;
};
StreamContext g_stream;

// Engine state that persists between requests to avoid redundant switches.
int g_currentDialect = -1;
int g_currentSampleRate = -1;
std::map<int, ECIDictHand> g_dictHandles;

// ---------------- helpers ----------------

std::string narrow(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

bool send_message(HANDLE pipe, uint32_t type, const void* data, uint32_t size)
{
    OutloudMessageHeader h = { type, size };
    DWORD written = 0;
    if (!WriteFile(pipe, &h, sizeof(h), &written, nullptr)) {
        return false;
    }
    if (data && size) {
        if (!WriteFile(pipe, data, size, &written, nullptr)) {
            return false;
        }
    }
    return true;
}

void send_error(HANDLE pipe, const char* msg)
{
    OL_LOG("host: error reply: %s", msg);
    send_message(pipe, OL_RESP_ERROR, msg, static_cast<uint32_t>(strlen(msg)));
}

// ---------------- virtual registry ----------------

// Absolutize a value if the key name starts with "Path" and the value is not
// already an absolute path. Paths in the shipped eci.ini are relative to the
// engine directory so the whole installation stays relocatable.
std::string resolve_path_value(const std::string& key, const std::string& value)
{
    if (_strnicmp(key.c_str(), "Path", 4) != 0 || value.empty()) {
        return value;
    }
    const bool absolute = (value.size() >= 2 && value[1] == ':') ||
                          (value.size() >= 2 && value[0] == '\\' && value[1] == '\\');
    if (absolute) {
        return value;
    }
    return g_engineDirA + "\\" + value;
}

bool build_virtual_registry()
{
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\OutloudSAPI\\VirtLM");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\OutloudSAPI\\VirtCU");

    HKEY hVirtLM = nullptr, hVirtCU = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudSAPI\\VirtLM", 0, nullptr,
            REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, &hVirtLM, nullptr) != ERROR_SUCCESS ||
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OutloudSAPI\\VirtCU", 0, nullptr,
            REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, &hVirtCU, nullptr) != ERROR_SUCCESS) {
        OL_LOG("host: cannot create virtual registry scratch keys");
        return false;
    }

    const std::wstring iniPath = g_engineDir + L"\\eci.ini";
    FILE* f = _wfsopen(iniPath.c_str(), L"rb", _SH_DENYWR);
    if (!f) {
        OL_LOG("host: cannot open %S", iniPath.c_str());
        return false;
    }

    const char* rootPath = "Software\\IBM\\ViaVoice Outloud 5.0";
    HKEY hSection = nullptr;
    char line[4096];
    int keys = 0, values = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = 0;
        if (!len || line[0] == ';') continue;
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (!end) continue;
            *end = 0;
            std::string sec = line + 1;
            std::string sub = std::string(rootPath) + "\\";
            if (sec.find('\\') == std::string::npos) sub += "ECIINI\\" + sec;
            else sub += sec;
            if (hSection) { RegCloseKey(hSection); hSection = nullptr; }
            if (RegCreateKeyExA(hVirtLM, sub.c_str(), 0, nullptr, REG_OPTION_VOLATILE,
                    KEY_ALL_ACCESS, nullptr, &hSection, nullptr) == ERROR_SUCCESS) {
                ++keys;
            } else {
                OL_LOG("host: cannot create virtual key %s", sub.c_str());
            }
        } else if (hSection) {
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const std::string value = resolve_path_value(line, eq + 1);
            if (RegSetValueExA(hSection, line, 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(value.c_str()),
                    static_cast<DWORD>(value.size() + 1)) == ERROR_SUCCESS) {
                ++values;
            }
        }
    }
    if (hSection) RegCloseKey(hSection);
    fclose(f);

    OL_LOG("host: virtual registry built (%d keys, %d values) from %S", keys, values, iniPath.c_str());
    if (keys == 0) {
        return false;
    }

    const LONG r1 = RegOverridePredefKey(HKEY_LOCAL_MACHINE, hVirtLM);
    const LONG r2 = RegOverridePredefKey(HKEY_CURRENT_USER, hVirtCU);
    OL_LOG("host: registry overrides applied (HKLM=%ld HKCU=%ld)", r1, r2);
    return r1 == ERROR_SUCCESS && r2 == ERROR_SUCCESS;
}

// ---------------- engine ----------------

bool bind_eci(HMODULE mod)
{
#define OL_BIND(n) *reinterpret_cast<FARPROC*>(&g_eci.n) = GetProcAddress(mod, #n); \
    if (!g_eci.n) { OL_LOG("host: missing export %s", #n); return false; }
    OL_BIND(eciNewEx) OL_BIND(eciDelete) OL_BIND(eciGetAvailableLanguages)
    OL_BIND(eciVersion) OL_BIND(eciGetParam) OL_BIND(eciSetParam)
    OL_BIND(eciGetVoiceParam) OL_BIND(eciSetVoiceParam) OL_BIND(eciCopyVoice)
    OL_BIND(eciGetVoiceName) OL_BIND(eciAddText) OL_BIND(eciInsertIndex)
    OL_BIND(eciSynthesize) OL_BIND(eciSynchronize) OL_BIND(eciStop)
    OL_BIND(eciSetOutputBuffer) OL_BIND(eciRegisterCallback) OL_BIND(eciProgStatus)
    OL_BIND(eciErrorMessage) OL_BIND(eciClearInput) OL_BIND(eciReset)
    OL_BIND(eciNewDict) OL_BIND(eciSetDict) OL_BIND(eciLoadDict)
#undef OL_BIND
    return true;
}

int __stdcall eci_callback(ECIHand, int msg, long lparam, void*)
{
    if (g_stream.aborted) {
        return eciDataAbort;
    }
    if (msg == eciWaveformBuffer && lparam > 0) {
        const uint32_t bytes = static_cast<uint32_t>(lparam) * 2;
        if (!send_message(g_stream.pipe, OL_RESP_AUDIO, g_audioBuffer, bytes)) {
            OL_LOG("host: audio write failed - aborting synthesis");
            g_stream.aborted = true;
            return eciDataAbort;
        }
        g_stream.bytesSent += bytes;
    } else if (msg == eciIndexReply) {
        int32_t idx = static_cast<int32_t>(lparam);
        if (!send_message(g_stream.pipe, OL_RESP_INDEX, &idx, sizeof(idx))) {
            g_stream.aborted = true;
            return eciDataAbort;
        }
    }
    return eciDataProcessed;
}

void log_eci_error(const char* what)
{
    char msg[512] = {};
    const int status = g_eci.eciProgStatus ? g_eci.eciProgStatus(g_handle) : -1;
    if (g_eci.eciErrorMessage) g_eci.eciErrorMessage(g_handle, msg);
    OL_LOG("host: %s failed (status=0x%08X, msg=\"%s\")", what, status, msg);
}

void apply_language_dictionaries(int dialect, const char* code);
const char* language_code_for_dialect(int dialect);

// The engine only honors a sample rate set on a fresh handle before the first
// synthesis; eciSetParam(eciSampleRate) on a live handle is silently ignored.
// So changing the rate means recreating the engine handle.
bool create_engine_handle(int dialect, int sampleRate)
{
    if (g_handle) {
        g_eci.eciDelete(g_handle);
        g_handle = nullptr;
        g_dictHandles.clear(); // dictionary handles died with the engine handle
    }

    g_handle = g_eci.eciNewEx(dialect);
    if (!g_handle) {
        OL_LOG("host: eciNewEx(0x%08X) failed", dialect);
        g_currentDialect = -1;
        g_currentSampleRate = -1;
        return false;
    }
    g_currentDialect = dialect;

    g_eci.eciRegisterCallback(g_handle, eci_callback, nullptr);
    g_eci.eciSetOutputBuffer(g_handle, AUDIO_BUFFER_SAMPLES, g_audioBuffer);
    g_eci.eciSetParam(g_handle, eciSynthMode, 1);
    g_eci.eciSetParam(g_handle, eciInputType, 1); // annotated text
    if (sampleRate >= 0) {
        g_eci.eciSetParam(g_handle, eciSampleRate, sampleRate);
    }
    g_currentSampleRate = g_eci.eciGetParam(g_handle, eciSampleRate);
    OL_LOG("host: engine handle created (dialect=0x%08X, sample rate mode %d)",
        dialect, g_currentSampleRate);

    apply_language_dictionaries(dialect, language_code_for_dialect(dialect));
    return true;
}

bool init_engine(int sampleRate)
{
    // etidev.dll must be resident before ibmeci.dll resolves against it.
    const std::wstring etidev = g_engineDir + L"\\etidev.dll";
    LoadLibraryExW(etidev.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

    const std::wstring eciDll = g_engineDir + L"\\ibmeci.dll";
    g_eciModule = LoadLibraryExW(eciDll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_eciModule) {
        OL_LOG("host: cannot load %S (error %lu)", eciDll.c_str(), GetLastError());
        return false;
    }
    if (!bind_eci(g_eciModule)) {
        return false;
    }

    char version[64] = {};
    g_eci.eciVersion(version);
    OL_LOG("host: ECI version %s", version);

    int nLangs = 0;
    g_eci.eciGetAvailableLanguages(nullptr, &nLangs);
    OL_LOG("host: %d languages available", nLangs);
    if (nLangs == 0) {
        return false;
    }

    std::vector<int> langs(nLangs);
    g_eci.eciGetAvailableLanguages(langs.data(), &nLangs);

    return create_engine_handle(langs[0], sampleRate);
}

// Load per-language user dictionaries (<code>main.dic / <code>root.dic /
// <code>abbr.dic in the engine directory), mirroring the NVDA driver.
void apply_language_dictionaries(int dialect, const char* code)
{
    auto it = g_dictHandles.find(dialect);
    if (it != g_dictHandles.end()) {
        g_eci.eciSetDict(g_handle, it->second);
        return;
    }
    const std::string base = g_engineDirA + "\\" + code;
    const std::string files[3] = { base + "main.dic", base + "root.dic", base + "abbr.dic" };
    bool any = false;
    for (const auto& fpath : files) {
        if (GetFileAttributesA(fpath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            any = true;
        }
    }
    if (!any) {
        return;
    }
    ECIDictHand dict = g_eci.eciNewDict(g_handle);
    if (!dict) {
        return;
    }
    for (int vol = 0; vol < 3; ++vol) {
        if (GetFileAttributesA(files[vol].c_str()) != INVALID_FILE_ATTRIBUTES) {
            g_eci.eciLoadDict(g_handle, dict, vol, files[vol].c_str());
            OL_LOG("host: loaded dictionary %s", files[vol].c_str());
        }
    }
    g_dictHandles[dialect] = dict;
    g_eci.eciSetDict(g_handle, dict);
}

const char* language_code_for_dialect(int dialect)
{
    switch (dialect) {
    case 0x00010000: return "enu"; case 0x00010001: return "eng";
    case 0x00020000: return "esp"; case 0x00020001: return "esm";
    case 0x00030000: return "fra"; case 0x00030001: return "frc";
    case 0x00040000: return "deu"; case 0x00050000: return "ita";
    case 0x00060000: return "chs"; case 0x00070000: return "ptb";
    case 0x00080000: return "jpn"; case 0x00090000: return "fin";
    case 0x000A0000: return "kor"; case 0x000B0001: return "ctt";
    case 0x000D0000: return "nor"; case 0x000E0000: return "swe";
    case 0x000F0000: return "dan"; default: return "enu";
    }
}

// ---------------- request handling ----------------

void handle_speak(HANDLE pipe, const char* payload, uint32_t size)
{
    if (size < sizeof(OutloudSpeakRequest)) {
        send_error(pipe, "speak request too small");
        return;
    }
    const auto* req = reinterpret_cast<const OutloudSpeakRequest*>(payload);
    const char* cursor = payload + sizeof(OutloudSpeakRequest);
    const char* endp = payload + size;

    OL_LOG("host: speak dialect=0x%08X variant=%d sr=%d segments=%u",
        req->dialect, req->variant, req->sampleRate, req->segmentCount);

    // Sample rate: a live handle silently ignores eciSetParam(eciSampleRate),
    // so a rate change requires a fresh engine handle.
    if (req->sampleRate >= 0 && req->sampleRate != g_currentSampleRate) {
        OL_LOG("host: sample rate change %d -> %d, recreating engine handle",
            g_currentSampleRate, req->sampleRate);
        if (!create_engine_handle(req->dialect, req->sampleRate)) {
            send_error(pipe, "engine reinitialization failed");
            return;
        }
    }

    // Language switch.
    if (req->dialect != g_currentDialect) {
        if (g_eci.eciSetParam(g_handle, eciLanguageDialect, req->dialect) < 0) {
            log_eci_error("eciSetParam(languageDialect)");
            send_error(pipe, "language not available");
            return;
        }
        g_currentDialect = req->dialect;
        apply_language_dictionaries(req->dialect, language_code_for_dialect(req->dialect));
    }

    // Abbreviation dictionary: eciDictionary 0 = ON, 1 = OFF.
    g_eci.eciSetParam(g_handle, eciDictionary, req->abbrDict ? 0 : 1);

    // Voice: copy the preset variant onto the active voice, then apply overrides.
    if (req->variant >= 1 && req->variant <= 8) {
        g_eci.eciCopyVoice(g_handle, req->variant, 0);
    }
    struct { int param; int value; } overrides[] = {
        { eciSpeed, req->rate }, { eciPitchBaseline, req->pitch },
        { eciPitchFluctuation, req->inflection }, { eciHeadSize, req->headSize },
        { eciRoughness, req->roughness }, { eciBreathiness, req->breathiness },
        { eciVolume, req->volume },
    };
    for (const auto& o : overrides) {
        if (o.value >= 0) {
            g_eci.eciSetVoiceParam(g_handle, 0, o.param, o.value);
        }
    }

    // Feed segments. Consecutive text segments are merged into one
    // eciAddText call so an engine annotation (backquote command) can never
    // be cut at a call boundary - the engine would speak a split annotation
    // literally.
    uint32_t fed = 0;
    std::string textAccum;
    auto flush_text = [&]() {
        if (!textAccum.empty()) {
            if (!g_eci.eciAddText(g_handle, textAccum.c_str())) {
                log_eci_error("eciAddText");
            }
            ++fed;
            textAccum.clear();
        }
    };
    for (uint32_t i = 0; i < req->segmentCount && cursor + sizeof(OutloudSegmentHeader) <= endp; ++i) {
        OutloudSegmentHeader seg;
        memcpy(&seg, cursor, sizeof(seg));
        cursor += sizeof(seg);
        if (seg.kind == OL_SEG_TEXT) {
            if (cursor + seg.value > endp) break;
            textAccum.append(cursor, seg.value);
            cursor += seg.value;
        } else if (seg.kind == OL_SEG_INDEX) {
            flush_text();
            g_eci.eciInsertIndex(g_handle, static_cast<int>(seg.value));
        }
    }
    flush_text();

    // Stream synthesis back to the client.
    g_stream.pipe = pipe;
    g_stream.aborted = false;
    g_stream.bytesSent = 0;

    int32_t status = 0;
    if (fed == 0) {
        g_eci.eciClearInput(g_handle);
    } else if (!g_eci.eciSynthesize(g_handle)) {
        log_eci_error("eciSynthesize");
        g_eci.eciClearInput(g_handle);
        status = 1;
    } else if (!g_eci.eciSynchronize(g_handle)) {
        log_eci_error("eciSynchronize");
        status = g_stream.aborted ? 2 : 1;
    }

    if (g_stream.aborted) {
        g_eci.eciStop(g_handle);
        OL_LOG("host: synthesis aborted after %llu bytes", g_stream.bytesSent);
        return; // client is gone; no END message possible
    }

    OL_LOG("host: synthesis complete (%llu bytes)", g_stream.bytesSent);
    send_message(pipe, OL_RESP_END, &status, sizeof(status));
}

void handle_get_languages(HANDLE pipe)
{
    int n = 0;
    g_eci.eciGetAvailableLanguages(nullptr, &n);
    std::vector<int32_t> data(1 + (n > 0 ? n : 0));
    data[0] = n;
    if (n > 0) {
        g_eci.eciGetAvailableLanguages(reinterpret_cast<int*>(data.data() + 1), &n);
    }
    send_message(pipe, OL_RESP_LANGUAGES, data.data(),
        static_cast<uint32_t>(data.size() * sizeof(int32_t)));
}

void handle_client(HANDLE pipe)
{
    for (;;) {
        OutloudMessageHeader header;
        DWORD bytesRead = 0;
        if (!ReadFile(pipe, &header, sizeof(header), &bytesRead, nullptr) ||
            bytesRead != sizeof(header)) {
            return;
        }
        std::vector<char> payload;
        if (header.size > 0) {
            if (header.size > 32 * 1024 * 1024) {
                return; // refuse absurd payloads
            }
            payload.resize(header.size);
            DWORD total = 0;
            while (total < header.size) {
                if (!ReadFile(pipe, payload.data() + total, header.size - total, &bytesRead, nullptr) || bytesRead == 0) {
                    return;
                }
                total += bytesRead;
            }
        }

        switch (header.type) {
        case OL_CMD_PING:
            send_message(pipe, OL_RESP_PONG, nullptr, 0);
            break;
        case OL_CMD_SPEAK:
            handle_speak(pipe, payload.data(), header.size);
            if (g_stream.aborted) {
                return; // connection is dead
            }
            break;
        case OL_CMD_GET_LANGUAGES:
            handle_get_languages(pipe);
            break;
        case OL_CMD_SHUTDOWN:
            send_message(pipe, OL_RESP_END, nullptr, 0);
            g_shutdownRequested = true;
            return;
        default:
            send_error(pipe, "unknown command");
            break;
        }
    }
}

int request_shutdown()
{
    HANDLE pipe = CreateFileW(OUTLOUD_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        return 0; // not running
    }
    OutloudMessageHeader h = { OL_CMD_SHUTDOWN, 0 };
    DWORD written = 0;
    WriteFile(pipe, &h, sizeof(h), &written, nullptr);
    char buf[64];
    DWORD rd = 0;
    ReadFile(pipe, buf, sizeof(buf), &rd, nullptr);
    CloseHandle(pipe);
    return 0;
}

}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--shutdown") == 0) {
            return request_shutdown();
        }
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exe(exePath);
    const size_t slash = exe.find_last_of(L'\\');
    g_exeDir = exe.substr(0, slash);
    g_engineDir = g_exeDir + L"\\engine";
    if (GetFileAttributesW((g_engineDir + L"\\ibmeci.dll").c_str()) == INVALID_FILE_ATTRIBUTES) {
        g_engineDir = g_exeDir; // engine files live next to the exe
    }
    g_engineDirA = narrow(g_engineDir);

    const Outloud::Settings settings = Outloud::SettingsStore::load();
    const std::wstring logDir = Outloud::SettingsStore::log_dir();
    Outloud::logging::init(logDir.c_str(), L"host.log", settings.debugLogging);
    // The engine writes its own trace files (tts.log) into the current
    // directory; keep those out of application folders.
    SetCurrentDirectoryW(logDir.c_str());
    OL_LOG("host: starting, engine dir = %S", g_engineDir.c_str());

    HANDLE mutex = CreateMutexW(nullptr, TRUE, OUTLOUD_SERVER_MUTEX);
    if (!mutex) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        OL_LOG("host: another instance is already running");
        return 0;
    }

    if (!build_virtual_registry()) {
        OL_LOG("host: FATAL - virtual registry setup failed");
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 2;
    }
    if (!init_engine(settings.sampleRate)) {
        OL_LOG("host: FATAL - engine initialization failed");
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 3;
    }

    OL_LOG("host: ready, waiting for clients");
    while (!g_shutdownRequested) {
        HANDLE pipe = CreateNamedPipeW(OUTLOUD_PIPE_NAME, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 1 << 16, 1 << 16, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }
        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            handle_client(pipe);
        }
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }

    OL_LOG("host: shutting down");
    if (g_handle) {
        g_eci.eciDelete(g_handle);
    }
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
