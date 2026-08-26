#pragma once

#include <windows.h>
#include <stdio.h>
#include <share.h>
#include <time.h>
#include <stdarg.h>
#include <string>

// Runtime-switchable debug logging shared by every Outloud component.
// Enabled with [logging] debug=1 in settings.ini; each component writes its
// own file under %APPDATA%\OutloudSAPI\logs.
// Files are opened in plain "a" mode with _SH_DENYNO so several processes
// can log at once and a logging failure can never take the process down.

namespace Outloud {
namespace logging {

inline bool g_enabled = false;
inline wchar_t g_logPath[MAX_PATH] = {};

inline void init(const wchar_t* logDir, const wchar_t* fileName, bool enabled)
{
    g_enabled = enabled;
    if (logDir && fileName) {
        swprintf_s(g_logPath, L"%s\\%s", logDir, fileName);
    }
}

inline void write(const char* format, ...)
{
    if (!g_enabled || !g_logPath[0]) {
        return;
    }
    FILE* f = _wfsopen(g_logPath, L"a", _SH_DENYNO);
    if (!f) {
        return;
    }
    time_t now = time(nullptr);
    struct tm ti;
    localtime_s(&ti, &now);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &ti);
    fprintf(f, "[%s] [pid %lu tid %lu] ", stamp, GetCurrentProcessId(), GetCurrentThreadId());

    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);

    fputc('\n', f);
    fclose(f);
}

}
}

#define OL_LOG(...) ::Outloud::logging::write(__VA_ARGS__)
