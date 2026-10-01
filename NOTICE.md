# Notices

The code written for this project is licensed under the MIT License (see [LICENSE](LICENSE)). The material
below is not covered by that licence and stays under its own terms.

## The IBM ViaVoice Outloud engine

The files in `bin/engine/` are the IBM ViaVoice Outloud engine runtime: `ibmeci.dll`, `etidev.dll`, the `*.syn`
language data, the `*rom.dll` romanizers and the rest. As the README says, they are Licensed Material - Property
of IBM. They ship with the repository, and the MIT License does not cover them. `installer/eci.template.ini` is a
copy of the engine's `bin/engine/eci.ini` and is not covered either.

## Not covered: files adapted from the BestSpeech SAPI 5 wrapper

The files below were adapted from the BestSpeech SAPI 5 wrapper by Gozaltech
(<https://github.com/gozaltech/BstSpeech-sapi>) and still contain much of that project's
code: its SAPI 5 COM server and token enumerator skeleton. They are an exception to the
README's statement that the license covers the wrapper source code. The MIT License does not
cover them, and they stay under their original author's terms.

- `src/com.hpp` and `src/com.cpp`
- `src/registry.hpp` and `src/registry.cpp`
- `src/utils.hpp`
- `src/sapi_main.cpp`
- `src/ISpDataKeyImpl.hpp` and `src/ISpDataKeyImpl.cpp`
- `src/IEnumSpObjectTokensImpl.hpp` and `src/IEnumSpObjectTokensImpl.cpp`
- `src/ISpTTSEngineImpl.hpp`
- `src/voice_token.hpp` and `src/voice_token.cpp`

## Credit: the NVDA IBMTTS driver

This section is a credit, not an exception: the code it names is covered by the MIT License of this project.

`src/text_preprocess.h` and `src/text_preprocess.cpp` are a C++ port of the text conditioning in the NVDA IBMTTS
driver add-on (<https://github.com/davidacm/NVDA-IBMTTS-Driver>, `addon/synthDrivers/ibmeci.py`): the crash-word
fixes, the number and punctuation fixes, the backquote handling and the pause shortening. The README credits the
driver for the text preprocessing, the crash-word fixes and the driver behavior.

Copyright (C) 2009 - 2023 David CM and contributors.

The port is released under the MIT License of this project (see [LICENSE](LICENSE)).
