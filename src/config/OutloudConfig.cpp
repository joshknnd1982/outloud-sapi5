// OutloudConfig.exe - configuration utility for the Outloud SAPI5 voices.
//
// Every control is in the tab order with a proper label for screen readers.
// Changes are written to settings.ini immediately, so the SAPI voices pick
// them up on the very next spoken utterance. The Preview button speaks a
// sample through the shared Outloud host so adjustments are audible at once.

#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include "resource.h"
#include "../settings.h"
#include "../voice_data.hpp"
#include "../engine_client.h"
#include "../outloud_log.h"
#include "../installed_voices.h"
#include "../utils.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

namespace {

using namespace Outloud;

Settings g_settings;
bool g_loading = true; // guard: dialogs receive control notifications before WM_INITDIALOG completes
EngineClient* g_client = nullptr;
std::vector<char> g_previewWav; // must stay alive while PlaySound is async

const wchar_t* preview_text_for(int langIndex)
{
    switch (langIndex) {
    case 0: return L"Hello, this is a preview of the Outloud voice settings.";
    case 1: return L"Hello, this is a preview of the Outloud voice settings.";
    case 2: return L"Hola, esta es una prueba de la voz de Outloud.";
    case 3: return L"Hola, esta es una prueba de la voz de Outloud.";
    case 4: return L"Bonjour, ceci est un aper\x00E7u de la voix Outloud.";
    case 5: return L"Bonjour, ceci est un aper\x00E7u de la voix Outloud.";
    case 6: return L"Hallo, dies ist eine Vorschau der Outloud-Stimme.";
    case 7: return L"Ciao, questa \x00E8 un'anteprima della voce Outloud.";
    case 8: return L"\x4F60\x597D\xFF0C\x8FD9\x662F\x8BED\x97F3\x8BBE\x7F6E\x9884\x89C8\x3002";
    case 9: return L"Ol\x00E1, esta \x00E9 uma amostra da voz Outloud.";
    case 10: return L"\x3053\x3093\x306B\x3061\x306F\x3002\x3053\x308C\x306F\x97F3\x58F0\x8A2D\x5B9A\x306E\x30D7\x30EC\x30D3\x30E5\x30FC\x3067\x3059\x3002";
    case 11: return L"Hei, t\x00E4m\x00E4 on Outloud-\x00E4\x00E4nen esikatselu.";
    case 12: return L"\xC548\xB155\xD558\xC138\xC694. \xC774\xAC83\xC740 \xC74C\xC131 \xC124\xC815 \xBBF8\xB9AC\xBCF4\xAE30\xC785\xB2C8\xB2E4.";
    case 13: return L"\x4F60\x597D\xFF0C\x9019\x662F\x8A9E\x97F3\x8A2D\x5B9A\x9810\x89BD\x3002";
    case 14: return L"Hei, dette er en forh\x00E5ndsvisning av Outloud-stemmen.";
    case 15: return L"Hej, detta \x00E4r en f\x00F6rhandsgranskning av Outloud-r\x00F6sten.";
    case 16: return L"Hej, dette er et eksempel p\x00E5 Outloud-stemmen.";
    default: return L"Hello, this is a preview of the Outloud voice settings.";
    }
}

// ---- preview playback ----

struct PreviewAudio {
    std::vector<char> pcm;
};

bool preview_on_audio(const char* pcm, uint32_t bytes, void* user)
{
    auto* a = static_cast<PreviewAudio*>(user);
    a->pcm.insert(a->pcm.end(), pcm, pcm + bytes);
    return true;
}

void stop_preview()
{
    PlaySoundW(nullptr, nullptr, 0);
}

void play_preview(HWND dlg)
{
    stop_preview();
    if (!g_client) {
        static EngineClient client;
        g_client = &client;
    }

    const Settings& s = g_settings;
    const int langIndex = s.languageIndex;

    OutloudSpeakRequest req = {};
    req.dialect = voices::languages[langIndex].dialect;
    req.variant = s.variant;
    req.sampleRate = s.sampleRate;
    req.abbrDict = s.abbreviationExpansion ? 1 : 0;
    req.rate = s.rate;
    req.volume = s.volume;
    req.pitch = s.pitch;
    req.inflection = s.inflection;
    req.headSize = s.headSize;
    req.roughness = s.roughness;
    req.breathiness = s.breathiness;

    std::vector<SpeakSegment> segments;
    SpeakSegment seg;
    seg.text = utils::wstring_to_codepage(preview_text_for(langIndex),
        voices::languages[langIndex].codepage);
    segments.push_back(seg);

    PreviewAudio audio;
    bool aborted = false;
    if (!g_client->speak(req, segments, preview_on_audio, nullptr, &audio, aborted) ||
        audio.pcm.empty()) {
        MessageBoxW(dlg,
            L"Could not reach the Outloud speech engine. Make sure outloud_host.exe "
            L"is present next to the configuration utility.",
            L"Outloud TTS Configuration", MB_ICONWARNING | MB_OK);
        return;
    }

    const DWORD hz = (s.sampleRate == 0) ? 8000 : 11025;
    const DWORD dataSize = static_cast<DWORD>(audio.pcm.size());
    g_previewWav.clear();
    g_previewWav.reserve(44 + dataSize);
    auto push32 = [&](DWORD v) { g_previewWav.insert(g_previewWav.end(), reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v) + 4); };
    auto push16 = [&](WORD v) { g_previewWav.insert(g_previewWav.end(), reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v) + 2); };
    g_previewWav.insert(g_previewWav.end(), { 'R','I','F','F' });
    push32(36 + dataSize);
    g_previewWav.insert(g_previewWav.end(), { 'W','A','V','E','f','m','t',' ' });
    push32(16); push16(1); push16(1); push32(hz); push32(hz * 2); push16(2); push16(16);
    g_previewWav.insert(g_previewWav.end(), { 'd','a','t','a' });
    push32(dataSize);
    g_previewWav.insert(g_previewWav.end(), audio.pcm.begin(), audio.pcm.end());

    PlaySoundW(reinterpret_cast<LPCWSTR>(g_previewWav.data()), nullptr,
        SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

// ---- dialog data exchange ----

void save_settings()
{
    if (g_loading) {
        return;
    }
    SettingsStore::save(g_settings);
}

int read_edit(HWND dlg, int id, int lo, int hi, int fallback)
{
    BOOL ok = FALSE;
    int v = static_cast<int>(GetDlgItemInt(dlg, id, &ok, FALSE));
    if (!ok) {
        return fallback;
    }
    return v < lo ? lo : (v > hi ? hi : v);
}

void load_dialog(HWND dlg)
{
    g_loading = true;

    // Only what setup actually installed is offered. The real language index
    // and variant number travel in the item data, so the lists stay correct
    // however few entries they hold.
    if (!InstalledVoices::has_language(g_settings.languageIndex)) {
        g_settings.languageIndex = InstalledVoices::first_language();
    }
    if (!InstalledVoices::has_variant(g_settings.variant)) {
        g_settings.variant = InstalledVoices::first_variant();
    }

    HWND combo = GetDlgItem(dlg, IDC_LANGUAGE);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < voices::language_count; ++i) {
        if (!InstalledVoices::has_language(i)) {
            continue;
        }
        const int item = static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(voices::languages[i].name)));
        SendMessageW(combo, CB_SETITEMDATA, item, i);
        if (i == g_settings.languageIndex) {
            SendMessageW(combo, CB_SETCURSEL, item, 0);
        }
    }

    combo = GetDlgItem(dlg, IDC_VARIANT);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < voices::variant_count; ++i) {
        if (!InstalledVoices::has_variant(voices::variants[i].id)) {
            continue;
        }
        const int item = static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(voices::variants[i].name)));
        SendMessageW(combo, CB_SETITEMDATA, item, voices::variants[i].id);
        if (voices::variants[i].id == g_settings.variant) {
            SendMessageW(combo, CB_SETCURSEL, item, 0);
        }
    }

    combo = GetDlgItem(dlg, IDC_PAUSES);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Do not shorten pauses"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Shorten at end of text only"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Shorten all pauses"));
    SendMessageW(combo, CB_SETCURSEL, g_settings.pauseMode, 0);

    // Only the rates the engine can really render; its "22 kHz" mode mislabels
    // 11 kHz audio and would chipmunk.
    combo = GetDlgItem(dlg, IDC_SAMPLERATE);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"8 kilohertz"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"11 kilohertz"));
    SendMessageW(combo, CB_SETCURSEL, g_settings.sampleRate == 0 ? 0 : 1, 0);

    struct { int spin; int lo; int hi; int value; } spins[] = {
        { IDC_RATE_SPIN, 0, 250, g_settings.rate },
        { IDC_PITCH_SPIN, 0, 100, g_settings.pitch },
        { IDC_INFLECTION_SPIN, 0, 100, g_settings.inflection },
        { IDC_HEADSIZE_SPIN, 0, 100, g_settings.headSize },
        { IDC_ROUGHNESS_SPIN, 0, 100, g_settings.roughness },
        { IDC_BREATHINESS_SPIN, 0, 100, g_settings.breathiness },
        { IDC_VOLUME_SPIN, 0, 100, g_settings.volume },
    };
    for (const auto& sp : spins) {
        HWND spin = GetDlgItem(dlg, sp.spin);
        SendMessageW(spin, UDM_SETRANGE32, sp.lo, sp.hi);
        SendMessageW(spin, UDM_SETPOS32, 0, sp.value);
    }

    CheckDlgButton(dlg, IDC_BACKQUOTE, g_settings.backquoteVoiceTags ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_ABBREV, g_settings.abbreviationExpansion ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_PHRASE, g_settings.phrasePrediction ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_SENDPARAMS, g_settings.sendParams ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_DEBUGLOG, g_settings.debugLogging ? BST_CHECKED : BST_UNCHECKED);

    g_loading = false;
}

void on_command(HWND dlg, int id, int code)
{
    if (g_loading) {
        return;
    }

    switch (id) {
    case IDC_LANGUAGE:
        if (code == CBN_SELCHANGE) {
            const int item = static_cast<int>(SendDlgItemMessageW(dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0));
            if (item >= 0) {
                g_settings.languageIndex = static_cast<int>(
                    SendDlgItemMessageW(dlg, IDC_LANGUAGE, CB_GETITEMDATA, item, 0));
            }
            save_settings();
        }
        break;
    case IDC_VARIANT:
        if (code == CBN_SELCHANGE) {
            const int item = static_cast<int>(SendDlgItemMessageW(dlg, IDC_VARIANT, CB_GETCURSEL, 0, 0));
            if (item < 0) {
                break;
            }
            g_settings.variant = static_cast<int>(
                SendDlgItemMessageW(dlg, IDC_VARIANT, CB_GETITEMDATA, item, 0));
            // Like the NVDA driver: switching the variant loads that voice's
            // engine-defined character parameters; only the rate is kept.
            const auto& vd = voices::variants[g_settings.variant - 1];
            g_settings.headSize = vd.headSize;
            g_settings.pitch = vd.pitch;
            g_settings.inflection = vd.inflection;
            g_settings.roughness = vd.roughness;
            g_settings.breathiness = vd.breathiness;
            g_settings.volume = vd.volume;
            save_settings();
            // Refresh only the affected numeric fields; rebuilding the whole
            // dialog would disturb screen reader focus on the combo box.
            g_loading = true;
            SendDlgItemMessageW(dlg, IDC_HEADSIZE_SPIN, UDM_SETPOS32, 0, vd.headSize);
            SendDlgItemMessageW(dlg, IDC_PITCH_SPIN, UDM_SETPOS32, 0, vd.pitch);
            SendDlgItemMessageW(dlg, IDC_INFLECTION_SPIN, UDM_SETPOS32, 0, vd.inflection);
            SendDlgItemMessageW(dlg, IDC_ROUGHNESS_SPIN, UDM_SETPOS32, 0, vd.roughness);
            SendDlgItemMessageW(dlg, IDC_BREATHINESS_SPIN, UDM_SETPOS32, 0, vd.breathiness);
            SendDlgItemMessageW(dlg, IDC_VOLUME_SPIN, UDM_SETPOS32, 0, vd.volume);
            g_loading = false;
        }
        break;
    case IDC_PAUSES:
        if (code == CBN_SELCHANGE) {
            g_settings.pauseMode = static_cast<int>(SendDlgItemMessageW(dlg, IDC_PAUSES, CB_GETCURSEL, 0, 0));
            save_settings();
        }
        break;
    case IDC_SAMPLERATE:
        if (code == CBN_SELCHANGE) {
            g_settings.sampleRate = static_cast<int>(SendDlgItemMessageW(dlg, IDC_SAMPLERATE, CB_GETCURSEL, 0, 0));
            save_settings();
        }
        break;
    case IDC_RATE:
        if (code == EN_CHANGE) { g_settings.rate = read_edit(dlg, IDC_RATE, 0, 250, g_settings.rate); save_settings(); }
        break;
    case IDC_PITCH:
        if (code == EN_CHANGE) { g_settings.pitch = read_edit(dlg, IDC_PITCH, 0, 100, g_settings.pitch); save_settings(); }
        break;
    case IDC_INFLECTION:
        if (code == EN_CHANGE) { g_settings.inflection = read_edit(dlg, IDC_INFLECTION, 0, 100, g_settings.inflection); save_settings(); }
        break;
    case IDC_HEADSIZE:
        if (code == EN_CHANGE) { g_settings.headSize = read_edit(dlg, IDC_HEADSIZE, 0, 100, g_settings.headSize); save_settings(); }
        break;
    case IDC_ROUGHNESS:
        if (code == EN_CHANGE) { g_settings.roughness = read_edit(dlg, IDC_ROUGHNESS, 0, 100, g_settings.roughness); save_settings(); }
        break;
    case IDC_BREATHINESS:
        if (code == EN_CHANGE) { g_settings.breathiness = read_edit(dlg, IDC_BREATHINESS, 0, 100, g_settings.breathiness); save_settings(); }
        break;
    case IDC_VOLUME:
        if (code == EN_CHANGE) { g_settings.volume = read_edit(dlg, IDC_VOLUME, 0, 100, g_settings.volume); save_settings(); }
        break;
    case IDC_BACKQUOTE:
        g_settings.backquoteVoiceTags = IsDlgButtonChecked(dlg, IDC_BACKQUOTE) == BST_CHECKED;
        save_settings();
        break;
    case IDC_ABBREV:
        g_settings.abbreviationExpansion = IsDlgButtonChecked(dlg, IDC_ABBREV) == BST_CHECKED;
        save_settings();
        break;
    case IDC_PHRASE:
        g_settings.phrasePrediction = IsDlgButtonChecked(dlg, IDC_PHRASE) == BST_CHECKED;
        save_settings();
        break;
    case IDC_SENDPARAMS:
        g_settings.sendParams = IsDlgButtonChecked(dlg, IDC_SENDPARAMS) == BST_CHECKED;
        save_settings();
        break;
    case IDC_DEBUGLOG:
        g_settings.debugLogging = IsDlgButtonChecked(dlg, IDC_DEBUGLOG) == BST_CHECKED;
        save_settings();
        break;
    case IDC_PREVIEW:
        if (code == BN_CLICKED) {
            play_preview(dlg);
        }
        break;
    case IDC_STOPBTN:
        if (code == BN_CLICKED) {
            stop_preview();
        }
        break;
    case IDC_DEFAULTS:
        if (code == BN_CLICKED) {
            g_settings = Settings{};
            save_settings();
            load_dialog(dlg);
        }
        break;
    case IDOK:
    case IDCANCEL:
        stop_preview();
        SettingsStore::save(g_settings);
        EndDialog(dlg, 0);
        break;
    }
}

INT_PTR CALLBACK dialog_proc(HWND dlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (msg) {
    case WM_INITDIALOG:
        load_dialog(dlg);
        return TRUE;
    case WM_COMMAND:
        on_command(dlg, LOWORD(wParam), HIWORD(wParam));
        return TRUE;
    case WM_CLOSE:
        stop_preview();
        SettingsStore::save(g_settings);
        EndDialog(dlg, 0);
        return TRUE;
    }
    return FALSE;
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_settings = SettingsStore::load();
    logging::init(SettingsStore::log_dir().c_str(), L"config.log", g_settings.debugLogging);
    OL_LOG("config: started");

    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_CONFIG), nullptr, dialog_proc, 0);

    OL_LOG("config: exiting");
    return 0;
}
