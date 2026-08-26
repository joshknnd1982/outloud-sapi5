#pragma once

#include <string>

// Text conditioning applied before speech, ported from the NVDA IBMTTS driver
// (isIBM/ViaVoice code paths): crash-word workarounds, number/punctuation
// fixes, optional backquote stripping and JAWS-style pause shortening.

namespace Outloud {
namespace text {

// languageIndex: index into voices::languages.
// stripBackquotes: true when backquote voice tags are disabled.
// shortenAllPauses: true when pause mode is "shorten all pauses".
std::wstring preprocess(const std::wstring& input, int languageIndex,
                        bool stripBackquotes, bool shortenAllPauses);

// Replace backquotes with spaces so user text cannot inject engine
// annotations. Also covers characters that the codepage conversion would
// best-fit into a literal backquote (U+02CB, U+0300, U+2035, U+FF40).
void strip_backquotes(std::wstring& s);

}
}
