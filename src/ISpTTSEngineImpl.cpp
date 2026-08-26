#include <new>
#include <string>
#include <cmath>
#include <cwctype>
#include <algorithm>
#include <vector>

#include "utils.hpp"
#include "ISpTTSEngineImpl.hpp"
#include "engine_client.h"
#include "settings.h"
#include "text_preprocess.h"
#include "outloud_log.h"

namespace Outloud {
namespace sapi {

namespace {

// ---- shared engine client (one pipe per process) ----

EngineClient* g_client = nullptr;
INIT_ONCE g_clientInit = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK create_client(PINIT_ONCE, PVOID, PVOID*)
{
    static EngineClient client;
    g_client = &client;

    const Settings& s = SettingsStore::current();
#ifdef _WIN64
    logging::init(SettingsStore::log_dir().c_str(), L"sapi64.log", s.debugLogging);
#else
    logging::init(SettingsStore::log_dir().c_str(), L"sapi32.log", s.debugLogging);
#endif
    return TRUE;
}

EngineClient& client()
{
    InitOnceExecuteOnce(&g_clientInit, create_client, nullptr, nullptr);
    return *g_client;
}

// ---- markers streamed back as ECI indices ----

enum class MarkerKind { Word, Sentence, Bookmark };

struct Marker {
    MarkerKind kind;
    ULONG offset = 0;   // character offset in the input text
    ULONG length = 0;
    std::wstring bookmark;
};

struct SpeakContext {
    ISpTTSEngineSite* site = nullptr;
    ULONGLONG bytesWritten = 0;
    bool aborted = false;
    std::vector<Marker>* markers = nullptr;
};

bool on_audio(const char* pcm, uint32_t bytes, void* user)
{
    auto* ctx = static_cast<SpeakContext*>(user);
    const DWORD actions = ctx->site->GetActions();
    if (actions & SPVES_ABORT) {
        ctx->aborted = true;
        return false;
    }
    if (actions & SPVES_SKIP) {
        ctx->site->CompleteSkip(0);
        ctx->aborted = true;
        return false;
    }

    ULONG written = 0;
    const HRESULT hr = ctx->site->Write(pcm, bytes, &written);
    if (FAILED(hr)) {
        OL_LOG("speak: site Write failed (0x%08lX)", hr);
        ctx->aborted = true;
        return false;
    }
    // Note: pcbWritten is not trustworthy across SAPI hosts; S_OK means the
    // buffer was accepted in full.
    ctx->bytesWritten += bytes;
    return true;
}

void on_index(int32_t index, void* user)
{
    auto* ctx = static_cast<SpeakContext*>(user);
    if (!ctx->markers || index < 1 ||
        static_cast<size_t>(index) > ctx->markers->size()) {
        return;
    }
    const Marker& m = (*ctx->markers)[static_cast<size_t>(index) - 1];

    SPEVENT ev = {};
    ev.ullAudioStreamOffset = ctx->bytesWritten;
    ev.ulStreamNum = 0;
    switch (m.kind) {
    case MarkerKind::Word:
        ev.eEventId = SPEI_WORD_BOUNDARY;
        ev.elParamType = SPET_LPARAM_IS_UNDEFINED;
        ev.lParam = static_cast<LPARAM>(m.offset);
        ev.wParam = static_cast<WPARAM>(m.length);
        break;
    case MarkerKind::Sentence:
        ev.eEventId = SPEI_SENTENCE_BOUNDARY;
        ev.elParamType = SPET_LPARAM_IS_UNDEFINED;
        ev.lParam = static_cast<LPARAM>(m.offset);
        ev.wParam = static_cast<WPARAM>(m.length);
        break;
    case MarkerKind::Bookmark: {
        ev.eEventId = SPEI_TTS_BOOKMARK;
        ev.elParamType = SPET_LPARAM_IS_STRING;
        ev.lParam = reinterpret_cast<LPARAM>(m.bookmark.c_str());
        long value = 0;
        try { value = std::stol(m.bookmark); } catch (...) {}
        ev.wParam = static_cast<WPARAM>(value);
        break;
    }
    }
    ctx->site->AddEvents(&ev, 1);
}

int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool is_word_char(wchar_t c)
{
    return iswalnum(c) || c == L'\'' || c == L'-' || c > 0x2E7F; // CJK runs count as words
}

bool is_annotation_body_char(wchar_t c)
{
    return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') ||
           (c >= L'A' && c <= L'Z') || c == L'.';
}

// Tokenize text into word runs and everything else, treating a backquote
// (0x60) plus its command body (`p1, `vs120, `l1.0, ...) as one atomic
// non-word token. An engine annotation must never be cut by a segment
// boundary or an index marker: a split annotation loses its meaning and the
// engine speaks it literally ("backquote p 1").
template<typename OnWord, typename OnOther>
void split_words(const std::wstring& s, OnWord onWord, OnOther onOther)
{
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        if (s[i] == L'`') {
            size_t j = i + 1;
            while (j < n && is_annotation_body_char(s[j])) ++j;
            onOther(i, j - i);
            i = j;
        } else if (is_word_char(s[i])) {
            size_t j = i;
            while (j < n && is_word_char(s[j])) ++j;
            onWord(i, j - i);
            i = j;
        } else {
            size_t j = i;
            while (j < n && s[j] != L'`' && !is_word_char(s[j])) ++j;
            onOther(i, j - i);
            i = j;
        }
    }
}

const wchar_t PUNCTUATION[] = L"-,.:;)(?!–—";

}

void ShutdownPipeServer()
{
    client().shutdownServer();
}

ISpTTSEngineImpl::ISpTTSEngineImpl()
    : token_index_(0)
{
}

ISpTTSEngineImpl::~ISpTTSEngineImpl() = default;

STDMETHODIMP ISpTTSEngineImpl::SetObjectToken(ISpObjectToken* pToken)
{
    if (!pToken) {
        return E_INVALIDARG;
    }

    try {
        token_index_ = 0;

        // Preferred: our voice tokens carry an explicit TokenIndex value.
        utils::out_ptr<wchar_t> index_str(CoTaskMemFree);
        if (SUCCEEDED(pToken->GetStringValue(L"TokenIndex", index_str.address())) && index_str.get()) {
            token_index_ = _wtoi(index_str.get());
        } else {
            // Fallback: match by Name attribute.
            ISpDataKeyPtr attrs;
            if (SUCCEEDED(pToken->OpenKey(L"Attributes", &attrs))) {
                utils::out_ptr<wchar_t> name(CoTaskMemFree);
                if (SUCCEEDED(attrs->GetStringValue(L"Name", name.address())) && name.get()) {
                    for (int i = 0; i < voice_token_count; ++i) {
                        if (_wcsicmp(voice_attributes(i).get_name().c_str(), name.get()) == 0) {
                            token_index_ = i;
                            break;
                        }
                    }
                }
            }
        }

        if (token_index_ < 0 || token_index_ >= voice_token_count) {
            token_index_ = 0;
        }

        token_ = pToken;
        OL_LOG("SetObjectToken: token index %d (%S)", token_index_,
            voice_attributes(token_index_).get_name().c_str());
        return S_OK;
    }
    catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
    catch (...) {
        return E_UNEXPECTED;
    }
}

STDMETHODIMP ISpTTSEngineImpl::GetObjectToken(ISpObjectToken** ppToken)
{
    if (!ppToken) {
        return E_POINTER;
    }
    *ppToken = nullptr;

    if (token_) {
        token_.AddRef();
        *ppToken = token_.GetInterfacePtr();
        return S_OK;
    }
    return E_UNEXPECTED;
}

STDMETHODIMP ISpTTSEngineImpl::GetOutputFormat(
    const GUID* /*pTargetFmtId*/,
    const WAVEFORMATEX* /*pTargetWaveFormatEx*/,
    GUID* pOutputFormatId,
    WAVEFORMATEX** ppCoMemOutputWaveFormatEx)
{
    if (!pOutputFormatId || !ppCoMemOutputWaveFormatEx) {
        return E_POINTER;
    }

    *pOutputFormatId = SPDFID_WaveFormatEx;
    *ppCoMemOutputWaveFormatEx = nullptr;

    auto* pwfex = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
    if (!pwfex) {
        return E_OUTOFMEMORY;
    }

    const Settings& s = SettingsStore::current();
    const DWORD hz = (s.sampleRate == 0) ? 8000 : 11025;

    pwfex->wFormatTag = WAVE_FORMAT_PCM;
    pwfex->nChannels = 1;
    pwfex->nSamplesPerSec = hz;
    pwfex->wBitsPerSample = 16;
    pwfex->nBlockAlign = 2;
    pwfex->nAvgBytesPerSec = hz * 2;
    pwfex->cbSize = 0;

    *ppCoMemOutputWaveFormatEx = pwfex;
    return S_OK;
}

STDMETHODIMP ISpTTSEngineImpl::Speak(
    DWORD /*dwSpeakFlags*/,
    REFGUID /*rguidFormatId*/,
    const WAVEFORMATEX* /*pWaveFormatEx*/,
    const SPVTEXTFRAG* pTextFragList,
    ISpTTSEngineSite* pOutputSite)
{
    if (!pTextFragList || !pOutputSite) {
        return E_INVALIDARG;
    }

    try {
        const Settings& s = SettingsStore::current();
        const voice_attributes attr(token_index_);

        int langIndex = attr.is_configured() ? s.languageIndex : attr.language_index();
        langIndex = clampi(langIndex, 0, voices::language_count - 1);
        const int variant = attr.is_configured()
            ? clampi(s.variant, 1, voices::variant_count)
            : voices::variants[attr.variant_index()].id;

        // Base request. The variant copied by the host (eciCopyVoice) defines
        // the voice character. Only the "Configured Voice" token applies the
        // configuration utility's character parameters on top - for the named
        // variant tokens (Reed, Shelley, ...) those must stay untouched or
        // every variant would sound alike (the utility's absolute values
        // would overwrite exactly what distinguishes the presets).
        OutloudSpeakRequest req = {};
        req.dialect = voices::languages[langIndex].dialect;
        req.variant = variant;
        req.sampleRate = s.sampleRate;
        req.abbrDict = s.abbreviationExpansion ? 1 : 0;
        req.pitch = req.inflection = req.headSize = req.roughness = req.breathiness = -1;
        if (attr.is_configured()) {
            req.pitch = s.pitch;
            req.inflection = s.inflection;
            req.headSize = s.headSize;
            req.roughness = s.roughness;
            req.breathiness = s.breathiness;
        }

        // SAPI runtime rate (-10..10, exponential; +-10 = 3x) scales the base rate.
        long sapiRate = 0;
        pOutputSite->GetRate(&sapiRate);
        sapiRate = clampi(static_cast<int>(sapiRate), -10, 10);
        USHORT sapiVolume = 100;
        pOutputSite->GetVolume(&sapiVolume);

        const int baseRate = s.rate;
        const int baseVolume = s.volume;
        auto scaled_rate = [&](int rateAdj) {
            const int combined = clampi(static_cast<int>(sapiRate) + rateAdj, -10, 10);
            const double factor = std::pow(3.0, combined / 10.0);
            return clampi(static_cast<int>(std::lround(baseRate * factor)), 0, 250);
        };
        auto scaled_volume = [&](ULONG fragVolume) {
            const double v = baseVolume * (sapiVolume / 100.0) * (clampi(static_cast<int>(fragVolume), 0, 100) / 100.0);
            return clampi(static_cast<int>(std::lround(v)), 0, 100);
        };
        req.rate = scaled_rate(0);
        req.volume = scaled_volume(100);

        ULONGLONG interest = 0;
        pOutputSite->GetEventInterest(&interest);
        const bool wantWords = (interest & SPFEI(SPEI_WORD_BOUNDARY)) != 0;
        const bool wantSentences = (interest & SPFEI(SPEI_SENTENCE_BOUNDARY)) != 0;

        std::vector<Marker> markers;
        std::vector<SpeakSegment> segments;

        int curLangIndex = langIndex;
        int curRate = req.rate;
        int curVolume = req.volume;
        int curPitch = -1000; // annotation not yet emitted

        auto add_text = [&](const std::wstring& w, int langIdx) {
            if (w.empty()) {
                return;
            }
            SpeakSegment seg;
            seg.text = utils::wstring_to_codepage(w, voices::languages[langIdx].codepage);
            if (!seg.text.empty()) {
                segments.push_back(std::move(seg));
            }
        };
        auto add_annotation = [&](const std::string& a) {
            SpeakSegment seg;
            seg.text = a;
            segments.push_back(std::move(seg));
        };
        auto add_marker = [&](Marker&& m) {
            markers.push_back(std::move(m));
            SpeakSegment seg;
            seg.isIndex = true;
            seg.index = static_cast<int32_t>(markers.size());
            segments.push_back(std::move(seg));
        };

        add_annotation(s.phrasePrediction ? "`pp1 " : "`pp0 ");

        // "Always Send Current Speech Settings": like the NVDA driver, prefix
        // the utterance with the current volume and speed annotations.
        if (s.sendParams) {
            char prefix[48];
            sprintf_s(prefix, "`vv%d `vs%d ", req.volume, req.rate);
            add_annotation(prefix);
        }

        wchar_t lastChar = 0;
        bool anyText = false;

        for (const SPVTEXTFRAG* frag = pTextFragList; frag; frag = frag->pNext) {
            const DWORD actions = pOutputSite->GetActions();
            if (actions & SPVES_ABORT) {
                return S_OK;
            }
            if (actions & SPVES_SKIP) {
                pOutputSite->CompleteSkip(0);
                return S_OK;
            }

            const SPVSTATE& st = frag->State;

            if (st.eAction == SPVA_Bookmark) {
                Marker m;
                m.kind = MarkerKind::Bookmark;
                if (frag->ulTextLen > 0 && frag->pTextStart) {
                    m.bookmark.assign(frag->pTextStart, frag->ulTextLen);
                }
                add_marker(std::move(m));
                continue;
            }

            if (st.eAction == SPVA_Silence) {
                if (st.SilenceMSecs > 0) {
                    char buf[32];
                    sprintf_s(buf, " `p%lu ", static_cast<unsigned long>(st.SilenceMSecs));
                    add_annotation(buf);
                }
                continue;
            }

            if (st.eAction != SPVA_Speak && st.eAction != SPVA_SpellOut &&
                st.eAction != SPVA_Pronounce) {
                continue;
            }
            if (frag->ulTextLen == 0 || !frag->pTextStart) {
                continue;
            }

            // Language auto-switching from fragment locale (e.g. Balabolka
            // documents with mixed languages).
            if (st.LangID != 0) {
                const int li = voices::language_index_from_langid(st.LangID);
                if (li >= 0 && li != curLangIndex) {
                    add_annotation(std::string(voices::languages[li].annotation) + " ");
                    curLangIndex = li;
                }
            }

            // Per-fragment prosody annotations when they differ from current.
            const int fragRate = scaled_rate(clampi(st.RateAdj, -10, 10));
            if (fragRate != curRate) {
                char buf[24];
                sprintf_s(buf, "`vs%d ", fragRate);
                add_annotation(buf);
                curRate = fragRate;
            }
            const int fragVolume = scaled_volume(st.Volume);
            if (fragVolume != curVolume) {
                char buf[24];
                sprintf_s(buf, "`vv%d ", fragVolume);
                add_annotation(buf);
                curVolume = fragVolume;
            }
            const int pitchAdj = clampi(st.PitchAdj.MiddleAdj, -10, 10);
            if (pitchAdj != 0) {
                const int basePitch = (req.pitch >= 0) ? req.pitch : s.pitch;
                const int fragPitch = clampi(basePitch + pitchAdj * 3, 0, 100);
                if (fragPitch != curPitch) {
                    char buf[24];
                    sprintf_s(buf, "`vb%d ", fragPitch);
                    add_annotation(buf);
                    curPitch = fragPitch;
                }
            } else if (curPitch != -1000) {
                const int basePitch = (req.pitch >= 0) ? req.pitch : s.pitch;
                char buf[24];
                sprintf_s(buf, "`vb%d ", basePitch);
                add_annotation(buf);
                curPitch = -1000;
            }

            const bool spell = (st.eAction == SPVA_SpellOut);
            if (spell) {
                add_annotation("`ts1 ");
            }

            const std::wstring original(frag->pTextStart, frag->ulTextLen);
            std::wstring processed;
            if (spell) {
                // Spell-out mode names every character, so pause annotations
                // and crash-word rewrites must never be injected here - the
                // engine would read them aloud as "backquote p 1".
                processed = original;
                if (!s.backquoteVoiceTags) {
                    text::strip_backquotes(processed);
                }
            } else {
                processed = text::preprocess(original, curLangIndex,
                    !s.backquoteVoiceTags, s.pauseMode == 2);
            }

            if (!processed.empty()) {
                anyText = true;
                for (auto it = processed.rbegin(); it != processed.rend(); ++it) {
                    if (!iswspace(*it)) { lastChar = *it; break; }
                }
            }

            if (wantSentences && !processed.empty()) {
                Marker m;
                m.kind = MarkerKind::Sentence;
                m.offset = frag->ulTextSrcOffset;
                m.length = frag->ulTextLen;
                add_marker(std::move(m));
            }

            if (wantWords && !spell && !processed.empty()) {
                // Word offsets are computed on the original text (exact SAPI
                // offsets); the processed text is split into the same number
                // of word runs whenever possible and paired up in order. The
                // original gets the same backquote treatment first so both
                // sides tokenize identically.
                std::wstring origForWords = original;
                if (!s.backquoteVoiceTags) {
                    text::strip_backquotes(origForWords);
                }
                struct WordPos { ULONG off; ULONG len; };
                std::vector<WordPos> originalWords;
                split_words(origForWords,
                    [&](size_t off, size_t len) {
                        originalWords.push_back({ frag->ulTextSrcOffset + static_cast<ULONG>(off),
                                                  static_cast<ULONG>(len) });
                    },
                    [](size_t, size_t) {});

                size_t wordNum = 0;
                std::wstring pending;
                split_words(processed,
                    [&](size_t off, size_t len) {
                        // flush text before the word, drop a marker, then the word
                        add_text(pending, curLangIndex);
                        pending.clear();
                        if (wordNum < originalWords.size()) {
                            Marker m;
                            m.kind = MarkerKind::Word;
                            m.offset = originalWords[wordNum].off;
                            m.length = originalWords[wordNum].len;
                            add_marker(std::move(m));
                        }
                        ++wordNum;
                        pending.append(processed, off, len);
                    },
                    [&](size_t off, size_t len) {
                        pending.append(processed, off, len);
                    });
                add_text(pending, curLangIndex);
            } else {
                add_text(processed, curLangIndex);
            }

            if (spell) {
                add_annotation(" `ts0 ");
            }

            add_text(L" ", curLangIndex);
        }

        if (!anyText && markers.empty()) {
            return S_OK;
        }

        // Pause shortening at end of text.
        if (s.pauseMode >= 1 && anyText && lastChar &&
            !wcschr(PUNCTUATION, lastChar)) {
            add_annotation(" `p1 ");
        }

        SpeakContext ctx;
        ctx.site = pOutputSite;
        ctx.markers = &markers;

        bool aborted = false;
        const bool ok = client().speak(req, segments, on_audio, on_index, &ctx, aborted);
        OL_LOG("speak: done (ok=%d aborted=%d bytes=%llu)", ok ? 1 : 0,
            (aborted || ctx.aborted) ? 1 : 0, ctx.bytesWritten);

        if (!ok) {
            return E_FAIL;
        }
        return S_OK;
    }
    catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
    catch (...) {
        return E_UNEXPECTED;
    }
}
}
}
