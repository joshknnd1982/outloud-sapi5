#include "text_preprocess.h"
#include "voice_data.hpp"
#include <regex>
#include <vector>

namespace Outloud {
namespace text {

namespace {

struct Fix {
    std::wregex pattern;
    std::wstring replacement;
};

using FixList = std::vector<Fix>;

const auto icase = std::regex_constants::icase | std::regex_constants::ECMAScript;
const auto plain = std::regex_constants::ECMAScript;

// Applies to every language (ViaVoice builds).
const FixList& global_fixes()
{
    static const FixList fixes = {
        // Prevent spell-out when punctuation follows a word.
        { std::wregex(LR"(([a-z]+)([~#$%^*({|\[<%•]))", icase), L"$1 $2" },
        // Don't break phrases like "books(s)".
        { std::wregex(LR"(([a-z]+)\s+(\(s\)))", icase), L"$1$2" },
        // Remove spaces before trailing punctuation; ViaVoice dislikes them.
        { std::wregex(LR"(([a-z]+|\d+|\W+)\s+([:.!;,?](?![a-z]|\d)))", icase), L"$1$2" },
        // Collapse double spaces around brackets to reduce verbosity.
        { std::wregex(LR"(([\(\[]+)  (.))", plain), L"$1$2" },
        { std::wregex(LR"((.)  ([\)\]]+))", plain), L"$1$2" },
    };
    return fixes;
}

// English & CJK (languages that fall back to English text handling).
const FixList& english_fixes()
{
    static const FixList fixes = {
        { std::wregex(LR"(\b(Mc)\s+([A-Z][a-z]|[A-Z][A-Z]+))", plain), L"$1$2" },
        { std::wregex(LR"(c(ae|æ)sur(e)?)", icase), L"seizur" },
        { std::wregex(LR"(\b(|\d+|\W+)h'(r|v)[e])", icase), L"$1h $2e" },
        { std::wregex(LR"(\b(\w+[bdfhjlmnqrvyz])(h[he]s)([abcdefghjklmnopqrstvwy]\w+)\b)", icase), L"$1 $2$3" },
        { std::wregex(LR"(\b(\w+[bdfhjlmnqrvz])(h[he]s)(iron+[degins]?))", icase), L"$1 $2$3" },
        { std::wregex(LR"(\b(\w+'{1,}[bcdfghjklmnpqrstvwxyz])'*(h+[he]s)([abcdefghijklmnopqrstvwy]\w+)\b)", icase), L"$1 $2$3" },
        { std::wregex(LR"(\b(\w+[bcdfghjklmnpqrstvwxyz])('{1,}h+[he]s)([abcdefghijklmnopqrstvwy]\w+)\b)", icase), L"$1 $2$3" },
        { std::wregex(LR"((\d):(\d\d[snrt][tdh]))", icase), L"$1 $2" },
        { std::wregex(LR"(\b([bcdfghjklmnpqrstvwxz]+)'([bcdefghjklmnpqrstvwxz']+)'([drtv][aeiou]?))", icase), L"$1 $2 $3" },
        { std::wregex(LR"(\b(you+)'(re)+'([drv]e?))", icase), L"$1 $2 $3" },
        { std::wregex(LR"((re|un|non|anti)cosp)", icase), L"$1kosp" },
        { std::wregex(LR"(\b(\d+|\W+)?(\w+_+)?(_+)?([bcdfghjklmnpqrstvwxz]+)?(\d+)?t+z[s]che)", icase), L"$1 $2 $3 $4 $5 tz sche" },
        { std::wregex(LR"((juar)([a-z']{9,}))", icase), L"$1 $2" },
        // ViaVoice-specific crash words.
        { std::wregex(LR"((http://|ftp://)([a-z]+)(\W){1,3}([a-z]+)(/*\W){1,3}([a-z]){1})", icase), L"$1$2$3$4 $5$6" },
        { std::wregex(LR"((\d+)([-+*^/])(\d+)(\.)(\d+)(\.)(0{2,}))", icase), L"$1$2$3$4$5$6 $7" },
        { std::wregex(LR"((\d+)([-+*^/])(\d+)(\.)(\d+)(\.)(0\W))", icase), L"$1$2$3$4 $5$6$7" },
        { std::wregex(LR"((\d+)([-+*^/]+)(\d+)([-+*^/]+)([,.+])(0{2,}))", icase), L"$1$2$3$4$5 $6" },
        { std::wregex(LR"((\d+)(\.+)(\d+)(\.+)(0{2,})(\.\d*)\s*\.*([-+*^/]))", icase), L"$1$2$3$4 $5$6$7" },
        { std::wregex(LR"((\d+)\s*([-+*^/])\s*(\d+)(,)(00\b))", icase), L"$1$2$3$4 $5" },
        { std::wregex(LR"((\d+)\s*([-+*^/])\s*(\d+)(,)(0{4,}))", icase), L"$1$2$3$4 $5" },
        // Avoid "comma hundred" style misreads of comma-grouped numbers.
        { std::wregex(LR"(\b(\d{1,3}),(000),(\d{1,3})\b)", plain), L"$1$2$3" },
        { std::wregex(LR"(\b(\d{1,3}),(000),(\d{1,3}),(\d{1,3})\b)", plain), L"$1$2$3$4" },
        { std::wregex(LR"(\b(\d{1,3}),(000),(\d{1,3}),(\d{1,3}),(\d{1,3})\b)", plain), L"$1$2$3$4$5" },
        { std::wregex(LR"(\b(\d{1,3}),(000),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3})\b)", plain), L"$1$2$3$4$5$6" },
    };
    return fixes;
}

const FixList& spanish_fixes()
{
    static const FixList fixes = {
        // ViaVoice's Spanish time parser crashes on minutes 20-59; use colons.
        { std::wregex(LR"(([0-2]?[0-4])\.([2-5][0-9])\.([0-5][0-9]))", plain), L"$1:$2:$3" },
        { std::wregex(LR"((\d+) (\d{3}))", plain), L"$1  $2" },
    };
    return fixes;
}

const FixList& spanish_anticrash()
{
    static const FixList fixes = {
        { std::wregex(LR"(\b(0{1,12})(ª))", plain), L"$1 $2" },
        { std::wregex(LR"((\d{12,}[123679])(ª))", plain), L"$1 $2" },
    };
    return fixes;
}

const FixList& french_fixes()
{
    static const FixList fixes = {
        { std::wregex(LR"(([$€£])\s*(\d+)\s(000))", plain), L"$1$2$3" },
        { std::wregex(LR"((\d+)\s(000)\s*([$€£]))", plain), L"$1$2$3" },
    };
    return fixes;
}

const FixList& portuguese_fixes()
{
    static const FixList fixes = {
        { std::wregex(LR"((\d{1,2}):(00):(\d{1,2}))", plain), L"$1:$2 $3" },
    };
    return fixes;
}

const FixList& german_fixes()
{
    static const FixList fixes = {
        { std::wregex(LR"(dane-ben)", icase), L"dane `0 ben" },
        { std::wregex(LR"(dage-gen)", icase), L"dage `0 gen" },
    };
    return fixes;
}

// JAWS-style short pauses after punctuation.
const std::wregex& pause_pattern()
{
    static const std::wregex re(
        LR"(([a-zA-Z0-9]|\s)([-,.:;?!–—])(\2*?)(\s|[\\/]|$))", plain);
    return re;
}

void apply(const FixList& fixes, std::wstring& s)
{
    for (const auto& fix : fixes) {
        s = std::regex_replace(s, fix.pattern, fix.replacement);
    }
}

}

void strip_backquotes(std::wstring& s)
{
    for (auto& c : s) {
        if (c == L'`' || c == 0x02CB || c == 0x0300 || c == 0x2035 || c == 0xFF40) {
            c = L' ';
        }
    }
}

std::wstring preprocess(const std::wstring& input, int languageIndex,
                        bool stripBackquotes, bool shortenAllPauses)
{
    std::wstring s = input;

    // Trim trailing whitespace.
    while (!s.empty() && iswspace(s.back())) {
        s.pop_back();
    }

    if (stripBackquotes) {
        strip_backquotes(s);
    }

    const char* code = (languageIndex >= 0 && languageIndex < voices::language_count)
        ? voices::languages[languageIndex].code : "enu";

    try {
        apply(global_fixes(), s);

        if (!strcmp(code, "enu") || !strcmp(code, "eng") || !strcmp(code, "chs") ||
            !strcmp(code, "kor") || !strcmp(code, "ctt")) {
            apply(english_fixes(), s);
        } else if (!strcmp(code, "esp")) {
            apply(spanish_fixes(), s);
        } else if (!strcmp(code, "esm")) {
            apply(spanish_anticrash(), s);
        } else if (!strcmp(code, "fra")) {
            apply(french_fixes(), s);
        } else if (!strcmp(code, "ptb")) {
            apply(portuguese_fixes(), s);
        } else if (!strcmp(code, "deu")) {
            apply(german_fixes(), s);
        }

        if (shortenAllPauses) {
            s = std::regex_replace(s, pause_pattern(), L"$1 `p1$2$3$4");
        }
    } catch (const std::regex_error&) {
        // If any pattern misbehaves on unusual input, speak the text as-is.
    }

    return s;
}

}
}
