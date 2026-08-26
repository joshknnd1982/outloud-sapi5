#pragma once

#include <windows.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "outloud_protocol.h"

// Client side of the Outloud host pipe, shared by the 32-bit SAPI DLL, the
// 64-bit SAPI DLL and the configuration utility. Launches outloud_host.exe on
// demand and streams synthesis results back through callbacks.

namespace Outloud {

// Return false to abort synthesis (the connection is dropped; the host stops).
using AudioCallback = bool(*)(const char* pcm, uint32_t bytes, void* user);
using IndexCallback = void(*)(int32_t index, void* user);

struct SpeakSegment {
    bool isIndex = false;
    int32_t index = 0;
    std::string text; // engine-codepage bytes, may contain annotations
};

class EngineClient {
public:
    EngineClient();
    ~EngineClient();

    EngineClient(const EngineClient&) = delete;
    EngineClient& operator=(const EngineClient&) = delete;

    bool ping();

    // Streams one utterance. Returns false on transport failure.
    // aborted is set when the audio callback requested an abort.
    bool speak(const OutloudSpeakRequest& request,
               const std::vector<SpeakSegment>& segments,
               AudioCallback onAudio, IndexCallback onIndex, void* user,
               bool& aborted);

    void shutdownServer();

private:
    bool ensureConnected();
    void disconnect();
    bool isServerRunning();
    bool launchServer();
    bool sendMessage(uint32_t type, const void* data, uint32_t size);
    bool readMessage(uint32_t& type, std::vector<char>& data);

    HANDLE pipe_;
    std::wstring serverPath_;
    CRITICAL_SECTION cs_;
};

}
