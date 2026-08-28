#pragma once

// Which languages and voice variants the installer actually put on this
// machine. Setup writes voices.ini next to the SAPI DLL (one entry per
// selected language and variant); everything reads it from there so the SAPI
// voice list and the configuration utility only ever offer what is installed.
//
// When voices.ini is missing - a development tree, or an installation made by
// an older setup - everything is reported as installed, which is the previous
// behavior.

namespace Outloud {

class InstalledVoices {
public:
    // index into voices::languages
    static bool has_language(int languageIndex);
    // variant number, 1..voices::variant_count
    static bool has_variant(int variant);

    // First installed language index / variant number, for falling back when a
    // saved setting refers to something that is no longer present.
    static int first_language();
    static int first_variant();

    // False when no manifest was found (everything is reported as installed).
    static bool has_manifest();
};

}
