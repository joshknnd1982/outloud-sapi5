# outloud-sapi5

A native Windows SAPI5 wrapper for the abandoned **IBM ViaVoice Outloud** text-to-speech engine. It exposes **all 17 languages and all 8 voice variants — 137 SAPI5 voices** — to any SAPI5-compatible application, including screen readers (NVDA, JAWS, Narrator) and reading apps (Balabolka, Bookworm), in both 32-bit and 64-bit programs.

> **This repository contains source code only.** The IBM engine binaries and language data are not included. A ready-to-use installer is published on the [Releases](../../releases) page.

## ViaVoice Outloud vs. Eloquence — what's the difference?

Both engines are branches of the same family tree, and they sound very similar — but their situations today are completely different.

**The shared ancestry.** The synthesizer was created by Eloquent Technology, Inc. (ETI), the company behind the "Eloquence" formant synthesis engine. Its programming interface is literally called ECI — the *Eloquence Command Interface*. In the late 1990s IBM licensed the engine and shipped it as **IBM ViaVoice Outloud** (later "IBM ViaVoice TTS"), the runtime this project drives: `ibmeci.dll` plus `etidev.dll` and per-language `*.syn` data files. ETI itself was acquired by SpeechWorks in 2000, which was absorbed into ScanSoft in 2003, which merged into Nuance in 2005 — now part of Microsoft.

**ViaVoice Outloud — abandoned.** IBM left the desktop speech business around 2003, selling that product line to ScanSoft. ViaVoice Outloud has had no sales channel, no support, and no updates for more than two decades. It survives only as runtime files from old SDKs and bundled products, kept alive by the community — most notably through the NVDA "IBMTTS" add-on. Technically, the IBM build reads its entire configuration from a machine-wide registry key written by IBM's installer, which is one of the main obstacles this wrapper solves (see below). Its DSP genuinely renders at 8 kHz and 11 kHz (the build's nominal 22 kHz mode just relabels 11 kHz data).

**Eloquence — alive and commercial.** The same core engine continued under SpeechWorks/ScanSoft/Nuance as **ETI-Eloquence** (commonly "Eloquence", `eci.dll`). Unlike the IBM branch, it is still actively licensed and supported today: Code Factory sells Eloquence products for Windows and mobile under license, and Vispero ships Eloquence as a built-in synthesizer in JAWS. It receives maintained installers and modern-OS compatibility from those vendors. Engineering-wise the Eloquence build reads a plain `eci.ini` file next to the DLL rather than the registry, so it never needed the tricks this project uses.

**Which one is this project for?** Strictly the abandoned IBM ViaVoice Outloud runtime. If you want a supported, legally purchasable engine of this family, buy Eloquence from Code Factory or use it inside JAWS — do not use those products' files with this wrapper. This project exists so that people who have the old IBM runtime can keep using it, with modern 64-bit applications, without any registry installation, on current versions of Windows.

## Features

- **Fully registry-free engine operation.** The IBM build reads its configuration only from `HKLM\Software\IBM\ViaVoice Outloud 5.0` (an "ini cache" written by IBM's installer). This wrapper hosts the engine in its own 32-bit process, builds that configuration in a **volatile** per-process registry view from a relocatable `eci.ini`, and redirects the engine onto it with `RegOverridePredefKey`. Nothing persistent is written; the real registry is never consulted by the engine.
- **32-bit and 64-bit SAPI5 interfaces.** Both DLLs are thin clients of one shared engine host process (`outloud_host.exe`) over a named pipe. An engine crash can never take down your screen reader.
- **137 voices**: 8 variants (Reed, Shelley, Sandy, Rocko, Glen, FastFlo, Grandma, Grandpa) x 17 languages (American & British English, Castilian & Latin American Spanish, French & Canadian French, German, Italian, Brazilian Portuguese, Finnish, Mandarin Chinese, Hong Kong Cantonese, Japanese, Korean, Norwegian, Swedish, Danish), plus an "Outloud Configured Voice" shaped entirely by the configuration utility. Variant voices keep their engine-defined personalities; the Configured Voice applies your custom parameters.
- **Accurate word boundaries and bookmarks** via engine index events, sample-synchronized with the audio stream. Engine annotations are treated as atomic tokens so they can never be split (and spoken) by segment boundaries.
- **Automatic language switching** from per-fragment SAPI locale information — mixed-language documents just work.
- **Crash-word protection**: the NVDA IBMTTS driver's text fixes, ported to C++.
- **Configuration utility** (`OutloudConfig.exe`): language, voice variant, rate, pitch, inflection, head size, roughness, breathiness, volume, backquote voice tags, abbreviation expansion, phrase prediction, pause shortening (3 modes), "Always Send Current Speech Settings", sample rate (8/11 kHz) and debug logging. Every control is labeled for screen readers; changes save instantly and take effect on the next utterance of any running SAPI5 client. Switching the variant loads that voice's engine-defined parameters, exactly like the NVDA driver.
- **Detailed logging** for the installer (`{app}\logs\install.log`) and all runtime components (`%APPDATA%\OutloudSAPI\logs`, toggled in the utility).

## Layout

- `src/` — SAPI5 engine DLL, 32-bit engine host, pipe client, configuration utility
- `installer/` — Inno Setup script, staging script, relative-path `eci.ini` template
- `test/` — registry-free engine smoke test, pipe protocol test client, sample-rate probe

## Building from source

```batch
build_all.bat
```

Produces `output\OutloudSAPI_Setup.exe`.

**Requirements:**
- Windows 10 or later
- Visual Studio 2022 (or Build Tools) with the C++ workload
- CMake 3.15+
- Inno Setup 6
- The ViaVoice Outloud engine files in `bin\` (`ibmeci.dll`, `etidev.dll`, the `*.syn` language data and the four `*rom.dll` romanizers), which you must supply yourself — they are IBM's property and are not part of this repository.

## Credits

- The IBM ViaVoice Outloud engine is Licensed Material — Property of IBM.
- Text preprocessing, crash-word fixes and driver behavior are ported from the [NVDA IBMTTS driver add-on](https://github.com/davidacm/NVDA-IBMTTS-Driver) by David CM and contributors (GPL).
- SAPI5 COM plumbing adapted from the Bestspeech SAPI wrapper by Gozaltech.

## License

GNU General Public License v2.0 or later — see [LICENSE](LICENSE). The license covers the wrapper source code only, not the IBM engine or its data files.
