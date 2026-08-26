#pragma once

#include <string>
#include <windows.h>

// Static description of everything the Outloud engine offers:
// 17 languages x 8 preset voice variants, plus one "Configured Voice"
// SAPI token that follows the settings chosen in the configuration utility.

namespace Outloud {
namespace voices {

struct language_info {
    int dialect;              // ECILanguageDialect
    const char* code;         // three-letter id (enu, eng, ...)
    const wchar_t* name;      // English display name
    const wchar_t* lcid;      // SAPI Language attribute (hex LANGID)
    WORD langid;              // primary LANGID for SPVSTATE matching
    UINT codepage;            // codepage the engine expects for this language
    const char* annotation;   // `lX.Y language switch annotation
};

// Order matters: voice tokens are generated in this order.
inline constexpr language_info languages[] = {
    { 0x00010000, "enu", L"American English",       L"409", 0x0409, 1252, "`l1.0" },
    { 0x00010001, "eng", L"British English",        L"809", 0x0809, 1252, "`l1.1" },
    { 0x00020000, "esp", L"Castilian Spanish",      L"40A", 0x040A, 1252, "`l2.0" },
    { 0x00020001, "esm", L"Latin American Spanish", L"80A", 0x080A, 1252, "`l2.1" },
    { 0x00030000, "fra", L"French",                 L"40C", 0x040C, 1252, "`l3.0" },
    { 0x00030001, "frc", L"Canadian French",        L"C0C", 0x0C0C, 1252, "`l3.1" },
    { 0x00040000, "deu", L"German",                 L"407", 0x0407, 1252, "`l4.0" },
    { 0x00050000, "ita", L"Italian",                L"410", 0x0410, 1252, "`l5.0" },
    { 0x00060000, "chs", L"Mandarin Chinese",       L"804", 0x0804,  936, "`l6.0" },
    { 0x00070000, "ptb", L"Brazilian Portuguese",   L"416", 0x0416, 1252, "`l7.0" },
    { 0x00080000, "jpn", L"Japanese",               L"411", 0x0411,  932, "`l8.0" },
    { 0x00090000, "fin", L"Finnish",                L"40B", 0x040B, 1252, "`l9.0" },
    { 0x000A0000, "kor", L"Korean",                 L"412", 0x0412,  949, "`l10.0" },
    { 0x000B0001, "ctt", L"Hong Kong Cantonese",    L"C04", 0x0C04,  950, "`l11.1" },
    { 0x000D0000, "nor", L"Norwegian",              L"414", 0x0414, 1252, "`l13.0" },
    { 0x000E0000, "swe", L"Swedish",                L"41D", 0x041D, 1252, "`l14.0" },
    { 0x000F0000, "dan", L"Danish",                 L"406", 0x0406, 1252, "`l15.0" },
};
inline constexpr int language_count = sizeof(languages) / sizeof(languages[0]);

struct variant_info {
    int id;                 // 1..8, ECI preset voice number
    const wchar_t* name;    // classic Eloquence-style name
    const wchar_t* gender;  // SAPI Gender attribute
    const wchar_t* age;     // SAPI Age attribute
    // Engine-defined voice parameters for this preset (from the engine's own
    // Voice1..Voice8 configuration; identical across languages). Used by the
    // configuration utility to reset the sliders when the variant changes.
    int headSize;
    int pitch;              // eciPitchBaseline
    int inflection;         // eciPitchFluctuation
    int roughness;
    int breathiness;
    int speed;
    int volume;
};

inline constexpr variant_info variants[] = {
    { 1, L"Reed",     L"Male",   L"Adult",  50, 65, 30,  0,  0, 50,  92 },
    { 2, L"Shelley",  L"Female", L"Adult",  50, 81, 30,  0, 50, 50, 100 },
    { 3, L"Sandy",    L"Female", L"Child",  22, 93, 35,  0,  0, 50,  90 },
    { 4, L"Rocko",    L"Male",   L"Adult",  86, 56, 47,  0,  0, 50,  93 },
    { 5, L"Glen",     L"Male",   L"Adult",  50, 69, 34,  0,  0, 70,  92 },
    { 6, L"FastFlo",  L"Female", L"Adult",  56, 89, 35,  0, 40, 70,  95 },
    { 7, L"Grandma",  L"Female", L"Senior", 45, 68, 30,  3, 40, 50,  90 },
    { 8, L"Grandpa",  L"Male",   L"Senior", 30, 61, 44, 18, 20, 50,  90 },
};
inline constexpr int variant_count = sizeof(variants) / sizeof(variants[0]);

[[nodiscard]] inline int language_index_from_dialect(int dialect) noexcept
{
    for (int i = 0; i < language_count; ++i) {
        if (languages[i].dialect == dialect) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] inline int language_index_from_code(const char* code) noexcept
{
    for (int i = 0; i < language_count; ++i) {
        if (_stricmp(languages[i].code, code) == 0) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] inline int language_index_from_langid(WORD langid) noexcept
{
    for (int i = 0; i < language_count; ++i) {
        if (languages[i].langid == langid) {
            return i;
        }
    }
    // second pass: match on primary language only
    for (int i = 0; i < language_count; ++i) {
        if (PRIMARYLANGID(languages[i].langid) == PRIMARYLANGID(langid)) {
            return i;
        }
    }
    return -1;
}

}

namespace sapi {

// Voice token identity. Index 0 is the "Configured Voice" that follows the
// configuration utility; the rest are language x variant combinations.
class voice_attributes
{
public:
    explicit voice_attributes(int token_index = 0) noexcept
        : token_index_(token_index)
    {
        const int max_index = voices::language_count * voices::variant_count;
        if (token_index_ < 0 || token_index_ > max_index) {
            token_index_ = 0;
        }
    }

    [[nodiscard]] bool is_configured() const noexcept { return token_index_ == 0; }

    [[nodiscard]] int language_index() const noexcept
    {
        return is_configured() ? 0 : (token_index_ - 1) / voices::variant_count;
    }

    [[nodiscard]] int variant_index() const noexcept
    {
        return is_configured() ? 0 : (token_index_ - 1) % voices::variant_count;
    }

    [[nodiscard]] int token_index() const noexcept { return token_index_; }

    [[nodiscard]] std::wstring get_name() const
    {
        if (is_configured()) {
            return L"Outloud Configured Voice";
        }
        const auto& lang = voices::languages[language_index()];
        const auto& var = voices::variants[variant_index()];
        return std::wstring(L"Outloud ") + var.name + L" (" + lang.name + L")";
    }

    [[nodiscard]] std::wstring get_age() const
    {
        return is_configured() ? L"Adult" : voices::variants[variant_index()].age;
    }

    [[nodiscard]] std::wstring get_gender() const
    {
        return is_configured() ? L"Male" : voices::variants[variant_index()].gender;
    }

    [[nodiscard]] std::wstring get_language() const
    {
        return voices::languages[language_index()].lcid;
    }

private:
    int token_index_;
};

inline constexpr int voice_token_count = 1 + voices::language_count * voices::variant_count; // 137

}
}
