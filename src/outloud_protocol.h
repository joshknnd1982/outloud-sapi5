#pragma once

#include <stdint.h>

// Framed named-pipe protocol between the SAPI DLLs / config utility (clients)
// and the 32-bit engine host process (outloud_host.exe).

#define OUTLOUD_PIPE_NAME L"\\\\.\\pipe\\OutloudTTS"
#define OUTLOUD_SERVER_MUTEX L"Local\\OutloudTTSHostMutex"
#define OUTLOUD_LAUNCH_MUTEX L"Local\\OutloudTTSLaunchMutex"

enum OutloudCommand : uint32_t {
    OL_CMD_PING = 0,
    OL_CMD_SPEAK = 1,
    OL_CMD_GET_LANGUAGES = 2,
    OL_CMD_SHUTDOWN = 3,
};

enum OutloudResponse : uint32_t {
    OL_RESP_PONG = 0,
    OL_RESP_AUDIO = 1,      // payload: raw 16-bit mono PCM
    OL_RESP_INDEX = 2,      // payload: int32 index value (client-side marker id)
    OL_RESP_END = 3,        // payload: int32 status (0 = ok)
    OL_RESP_ERROR = 4,      // payload: ANSI error text
    OL_RESP_LANGUAGES = 5,  // payload: int32 count + count * int32 dialects
};

enum OutloudSegmentKind : uint32_t {
    OL_SEG_TEXT = 0,   // payload: engine-codepage text bytes (may hold annotations)
    OL_SEG_INDEX = 1,  // no payload; index value in the segment header
};

#pragma pack(push, 1)
struct OutloudMessageHeader {
    uint32_t type;
    uint32_t size; // payload bytes following the header
};

// Voice parameter override: -1 leaves the variant's default untouched.
struct OutloudSpeakRequest {
    int32_t dialect;      // ECILanguageDialect value
    int32_t variant;      // 1..8 preset voice to copy to the active voice
    int32_t sampleRate;   // 0=8000, 1=11025, 2=22050
    int32_t abbrDict;     // 1 = abbreviation dictionary active
    int32_t rate;         // eciSpeed 0..250, -1 = variant default
    int32_t pitch;        // eciPitchBaseline 0..100, -1 = default
    int32_t inflection;   // eciPitchFluctuation 0..100, -1 = default
    int32_t headSize;     // eciHeadSize 0..100, -1 = default
    int32_t roughness;    // eciRoughness 0..100, -1 = default
    int32_t breathiness;  // eciBreathiness 0..100, -1 = default
    int32_t volume;       // eciVolume 0..100, -1 = default
    uint32_t segmentCount;
};

struct OutloudSegmentHeader {
    uint32_t kind;  // OutloudSegmentKind
    uint32_t value; // OL_SEG_TEXT: byte count of text that follows; OL_SEG_INDEX: index id
};
#pragma pack(pop)
