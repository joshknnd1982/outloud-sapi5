#include "engine_client.h"
#include "outloud_log.h"

namespace Outloud {

namespace {

class Lock {
public:
    explicit Lock(CRITICAL_SECTION* cs) noexcept : cs_(cs) { EnterCriticalSection(cs_); }
    ~Lock() { LeaveCriticalSection(cs_); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
private:
    CRITICAL_SECTION* cs_;
};

HMODULE current_module()
{
    HMODULE h = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&current_module), &h);
    return h;
}

bool file_exists(const std::wstring& p)
{
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

}

EngineClient::EngineClient()
    : pipe_(INVALID_HANDLE_VALUE)
{
    InitializeCriticalSection(&cs_);

    // Locate outloud_host.exe relative to this module: same directory first,
    // then the parent (the x64 DLL lives in an "x64" subdirectory).
    wchar_t modPath[MAX_PATH] = {};
    if (HMODULE hm = current_module()) {
        GetModuleFileNameW(hm, modPath, MAX_PATH);
    }
    std::wstring dir(modPath);
    size_t slash = dir.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        dir.resize(slash);
        std::wstring candidate = dir + L"\\outloud_host.exe";
        if (file_exists(candidate)) {
            serverPath_ = candidate;
        } else {
            slash = dir.find_last_of(L'\\');
            if (slash != std::wstring::npos) {
                candidate = dir.substr(0, slash) + L"\\outloud_host.exe";
                if (file_exists(candidate)) {
                    serverPath_ = candidate;
                }
            }
        }
    }
}

EngineClient::~EngineClient()
{
    disconnect();
    DeleteCriticalSection(&cs_);
}

bool EngineClient::isServerRunning()
{
    HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, OUTLOUD_SERVER_MUTEX);
    if (m) {
        CloseHandle(m);
        return true;
    }
    return false;
}

bool EngineClient::launchServer()
{
    if (isServerRunning()) {
        return true;
    }
    if (serverPath_.empty() || !file_exists(serverPath_)) {
        OL_LOG("client: outloud_host.exe not found (%S)", serverPath_.c_str());
        return false;
    }

    HANDLE launchMutex = CreateMutexW(nullptr, FALSE, OUTLOUD_LAUNCH_MUTEX);
    if (!launchMutex) {
        return false;
    }
    const DWORD wait = WaitForSingleObject(launchMutex, 5000);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
        CloseHandle(launchMutex);
        return false;
    }

    bool ok = isServerRunning();
    if (!ok) {
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(serverPath_.c_str(), nullptr, nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            for (int i = 0; i < 50 && !isServerRunning(); ++i) {
                Sleep(100);
            }
            ok = isServerRunning();
            OL_LOG("client: launched host (%s)", ok ? "ok" : "did not come up");
        } else {
            OL_LOG("client: CreateProcess failed (%lu)", GetLastError());
        }
    }

    ReleaseMutex(launchMutex);
    CloseHandle(launchMutex);
    return ok;
}

void EngineClient::disconnect()
{
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool EngineClient::ensureConnected()
{
    if (pipe_ != INVALID_HANDLE_VALUE) {
        return true;
    }
    for (int attempt = 0; attempt < 8; ++attempt) {
        pipe_ = CreateFileW(OUTLOUD_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe_ != INVALID_HANDLE_VALUE) {
            return true;
        }
        const DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            if (attempt == 0 && !launchServer()) {
                return false;
            }
            Sleep(200);
        } else if (err == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(OUTLOUD_PIPE_NAME, 2000);
        } else {
            break;
        }
    }
    return false;
}

bool EngineClient::sendMessage(uint32_t type, const void* data, uint32_t size)
{
    if (pipe_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    OutloudMessageHeader h = { type, size };
    DWORD written = 0;
    if (!WriteFile(pipe_, &h, sizeof(h), &written, nullptr)) {
        return false;
    }
    if (data && size) {
        if (!WriteFile(pipe_, data, size, &written, nullptr)) {
            return false;
        }
    }
    return true;
}

bool EngineClient::readMessage(uint32_t& type, std::vector<char>& data)
{
    if (pipe_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    OutloudMessageHeader h;
    DWORD rd = 0;
    if (!ReadFile(pipe_, &h, sizeof(h), &rd, nullptr) || rd != sizeof(h)) {
        return false;
    }
    type = h.type;
    data.clear();
    if (h.size > 0) {
        data.resize(h.size);
        DWORD total = 0;
        while (total < h.size) {
            if (!ReadFile(pipe_, data.data() + total, h.size - total, &rd, nullptr) || rd == 0) {
                return false;
            }
            total += rd;
        }
    }
    return true;
}

bool EngineClient::ping()
{
    Lock lock(&cs_);
    if (!ensureConnected() || !sendMessage(OL_CMD_PING, nullptr, 0)) {
        disconnect();
        return false;
    }
    uint32_t type = 0;
    std::vector<char> data;
    if (!readMessage(type, data) || type != OL_RESP_PONG) {
        disconnect();
        return false;
    }
    return true;
}

bool EngineClient::speak(const OutloudSpeakRequest& request,
    const std::vector<SpeakSegment>& segments,
    AudioCallback onAudio, IndexCallback onIndex, void* user, bool& aborted)
{
    Lock lock(&cs_);
    aborted = false;

    // Serialize request + segments into a single payload.
    std::vector<char> payload;
    payload.reserve(sizeof(OutloudSpeakRequest) + 256);
    OutloudSpeakRequest req = request;
    req.segmentCount = static_cast<uint32_t>(segments.size());
    payload.insert(payload.end(), reinterpret_cast<const char*>(&req),
        reinterpret_cast<const char*>(&req) + sizeof(req));
    for (const auto& seg : segments) {
        OutloudSegmentHeader sh;
        if (seg.isIndex) {
            sh.kind = OL_SEG_INDEX;
            sh.value = static_cast<uint32_t>(seg.index);
            payload.insert(payload.end(), reinterpret_cast<const char*>(&sh),
                reinterpret_cast<const char*>(&sh) + sizeof(sh));
        } else {
            sh.kind = OL_SEG_TEXT;
            sh.value = static_cast<uint32_t>(seg.text.size());
            payload.insert(payload.end(), reinterpret_cast<const char*>(&sh),
                reinterpret_cast<const char*>(&sh) + sizeof(sh));
            payload.insert(payload.end(), seg.text.begin(), seg.text.end());
        }
    }

    if (!ensureConnected() ||
        !sendMessage(OL_CMD_SPEAK, payload.data(), static_cast<uint32_t>(payload.size()))) {
        disconnect();
        // One retry with a fresh connection (host may have restarted).
        if (!ensureConnected() ||
            !sendMessage(OL_CMD_SPEAK, payload.data(), static_cast<uint32_t>(payload.size()))) {
            disconnect();
            return false;
        }
    }

    for (;;) {
        uint32_t type = 0;
        std::vector<char> data;
        if (!readMessage(type, data)) {
            disconnect();
            return false;
        }
        if (type == OL_RESP_AUDIO) {
            if (onAudio && !onAudio(data.data(), static_cast<uint32_t>(data.size()), user)) {
                // Abort: drop the connection so the host stops synthesizing.
                aborted = true;
                disconnect();
                return true;
            }
        } else if (type == OL_RESP_INDEX && data.size() >= sizeof(int32_t)) {
            if (onIndex) {
                int32_t idx;
                memcpy(&idx, data.data(), sizeof(idx));
                onIndex(idx, user);
            }
        } else if (type == OL_RESP_END) {
            return true;
        } else if (type == OL_RESP_ERROR) {
            OL_LOG("client: host error: %.*s", static_cast<int>(data.size()), data.data());
            return false;
        }
    }
}

void EngineClient::shutdownServer()
{
    Lock lock(&cs_);
    if (!isServerRunning()) {
        return;
    }
    if (ensureConnected()) {
        sendMessage(OL_CMD_SHUTDOWN, nullptr, 0);
        uint32_t type = 0;
        std::vector<char> data;
        readMessage(type, data);
        disconnect();
    }
    for (int i = 0; i < 20 && isServerRunning(); ++i) {
        Sleep(100);
    }
}

}
