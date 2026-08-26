#pragma once

#include <windows.h>
#include <string>

// Persistent user settings, stored in %APPDATA%\OutloudSAPI\settings.ini.
// The SAPI DLLs reload the file whenever its timestamp changes, so changes
// made in the configuration utility take effect on the very next utterance.

namespace Outloud {

struct Settings {
    // [speech]
    int languageIndex = 0;   // index into voices::languages (configured voice)
    int variant = 1;         // 1..8
    int rate = 50;           // eciSpeed 0..250
    int pitch = 65;          // 0..100
    int inflection = 30;     // 0..100
    int headSize = 50;       // 0..100
    int roughness = 0;       // 0..100
    int breathiness = 0;     // 0..100
    int volume = 92;         // 0..100

    // [options]
    bool backquoteVoiceTags = false;
    bool abbreviationExpansion = true;
    bool phrasePrediction = false;
    int pauseMode = 2;       // 0 = keep, 1 = shorten at end only, 2 = shorten all
    bool sendParams = true;  // "Always Send Current Speech Settings"
    // 0=8000 Hz, 1=11025 Hz. The engine's DSP only supports these two rates;
    // its "22 kHz" mode outputs 11 kHz samples with a wrong label (chipmunk),
    // so it is not offered.
    int sampleRate = 1;

    // [logging]
    bool debugLogging = false;
};

class SettingsStore {
public:
    // Full path of settings.ini (%APPDATA%\OutloudSAPI\settings.ini).
    static std::wstring path();
    // Directory for log files (%APPDATA%\OutloudSAPI\logs), created on demand.
    static std::wstring log_dir();

    static Settings load();
    static bool save(const Settings& s);

    // Returns the cached settings, reloading them if settings.ini changed.
    static const Settings& current();

private:
    static bool file_time(FILETIME& ft);
};

}
