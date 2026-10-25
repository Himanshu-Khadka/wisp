# Third-party software

The original Wisp application, bridge, flash helpers, and tests are provided under the MIT license in LICENSE.

- Display driver, DEV_Config, GUI_Paint, bitmap fonts, and related headers: Waveshare, from https://github.com/waveshareteam/ESP32-S3-ePaper-3.97 at commit 9b12d40731a80213b927ee8a421cae4082952819. Original notices are retained in the source files. Changes disable the nonexistent GPIO -1 display-power output and bound the display BUSY wait to 15 seconds.
- ES8311 audio codec driver: Espressif Systems / Waveshare example, Apache-2.0 SPDX notices retained. Original source is in the same Waveshare repository's Arduino/examples/01_Audio_Test directory.
- ESP32 Arduino core 3.3.2 and its bundled ESP-IDF components: https://github.com/espressif/arduino-esp32/tree/3.3.2. Core code is LGPL-2.1; ESP-IDF components carry their own licenses, primarily Apache-2.0. Source is available at that pinned tag; build.sh downloads the matching release for rebuilding/relinking this application.
- ArduinoJson 7.4.2: MIT, https://github.com/bblanchon/ArduinoJson/tree/v7.4.2.
- esptool 5.1.0: GPL-2.0-or-later, https://github.com/espressif/esptool/tree/v5.1.0. Installed separately by the flash helper, not bundled as an executable.
- Optional bridge packages are installed separately through requirements.txt; faster-whisper is an optional separate installation.
- Weather data: Open-Meteo (https://open-meteo.com/), attribution appears in the briefing. Headlines retain their configured publisher sources; default feed is BBC World News.

Review each upstream project's license for redistribution requirements. The full application source and pinned rebuild instructions accompany this binary.

## Added in 0.2

- fflate 0.8.2, Arjun Barrett, MIT: https://github.com/101arrowz/fflate. Pinned UMD bytes from https://cdn.jsdelivr.net/npm/fflate@0.8.2/umd/index.js are included as vendor/fflate.cjs and compressed into the firmware. License in vendor/fflate-LICENSE.
- Google trust roots from https://pki.goog/roots.pem, embedded in certs.h for TLS verification. No insecure-TLS fallback is enabled.
- Followup reference review is documented in REFERENCE_NOTES.md. No Followup GPL source or art was copied.
