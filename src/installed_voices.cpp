#include "installed_voices.h"
#include "voice_data.hpp"

#include <windows.h>
#include <string>

namespace Outloud {

namespace {

struct Manifest {
    bool found = false;
    bool languages[voices::language_count] = {};
    bool variants[voices::variant_count] = {};
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

// voices.ini sits next to the 32-bit DLL, the host and the configuration
// utility; the 64-bit DLL lives one level down in "x64", so look there too.
std::wstring manifest_path()
{
    wchar_t modPath[MAX_PATH] = {};
    if (HMODULE hm = current_module()) {
        GetModuleFileNameW(hm, modPath, MAX_PATH);
    }
    std::wstring dir(modPath);
    size_t slash = dir.find_last_of(L'\\');
    if (slash == std::wstring::npos) {
        return std::wstring();
    }
    dir.resize(slash);

    std::wstring candidate = dir + L"\\voices.ini";
    if (file_exists(candidate)) {
        return candidate;
    }
    slash = dir.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        candidate = dir.substr(0, slash) + L"\\voices.ini";
        if (file_exists(candidate)) {
            return candidate;
        }
    }
    return std::wstring();
}

Manifest load_manifest()
{
    Manifest m;
    const std::wstring file = manifest_path();
    if (file.empty()) {
        // No manifest: report everything as installed.
        for (bool& b : m.languages) { b = true; }
        for (bool& b : m.variants) { b = true; }
        return m;
    }

    m.found = true;
    for (int i = 0; i < voices::language_count; ++i) {
        wchar_t code[8] = {};
        MultiByteToWideChar(CP_ACP, 0, voices::languages[i].code, -1, code, 8);
        m.languages[i] = GetPrivateProfileIntW(L"languages", code, 0, file.c_str()) != 0;
    }
    for (int i = 0; i < voices::variant_count; ++i) {
        wchar_t key[8] = {};
        swprintf_s(key, L"%d", voices::variants[i].id);
        m.variants[i] = GetPrivateProfileIntW(L"variants", key, 0, file.c_str()) != 0;
    }

    // A manifest that selects nothing would leave no usable voice at all;
    // treat that as "everything" rather than hiding the engine completely.
    bool anyLang = false;
    for (bool b : m.languages) { anyLang = anyLang || b; }
    if (!anyLang) {
        for (bool& b : m.languages) { b = true; }
    }
    bool anyVariant = false;
    for (bool b : m.variants) { anyVariant = anyVariant || b; }
    if (!anyVariant) {
        for (bool& b : m.variants) { b = true; }
    }
    return m;
}

const Manifest& manifest()
{
    static const Manifest m = load_manifest();
    return m;
}

}

bool InstalledVoices::has_language(int languageIndex)
{
    if (languageIndex < 0 || languageIndex >= voices::language_count) {
        return false;
    }
    return manifest().languages[languageIndex];
}

bool InstalledVoices::has_variant(int variant)
{
    if (variant < 1 || variant > voices::variant_count) {
        return false;
    }
    return manifest().variants[variant - 1];
}

int InstalledVoices::first_language()
{
    for (int i = 0; i < voices::language_count; ++i) {
        if (manifest().languages[i]) {
            return i;
        }
    }
    return 0;
}

int InstalledVoices::first_variant()
{
    for (int i = 0; i < voices::variant_count; ++i) {
        if (manifest().variants[i]) {
            return voices::variants[i].id;
        }
    }
    return 1;
}

bool InstalledVoices::has_manifest()
{
    return manifest().found;
}

}
