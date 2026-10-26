# Validation — Wisp 0.2.3

## Passed

- Compiled for ESP32-S3, 16 MB flash, 8 MB octal PSRAM with ESP32 Arduino 3.3.2 and ArduinoJson 7.4.2.
- Compiler reports 1,420,599 bytes of application storage (42% of slot) and 52,444 bytes static RAM (16%). Dynamic JSON, TLS and recording allocations require additional runtime memory; hardware stress testing remains pending.
- Espressif esptool identifies the application as ESP32-S3 / 16 MB. Embedded checksum and validation hash pass.
- Merged image matches bootloader at 0x0, partition table at 0x8000, and app at 0x10000. Package integrity manifest includes hashes for both merged and app-only binaries.
- Core timer tests: elapsed time, pause/resume, interval transitions, millisecond rollover, fractional-second accounting; SHTC3 CRC reference vector.
- Eight optional-bridge tests: route authentication, default-token rejection, restricted home control, allowed toggle, disabled transcription, malformed audio, recurring calendar/RSS parsing, source failures.
- EPUB tests: spine order, paragraph boundaries, script removal, encrypted/missing/malformed/oversized input rejection, ASCII conversion, filename sanitization.
- JavaScript and Python syntax checks.
- Actual launcher/pixel-companion drawing functions rendered through Waveshare GUI_Paint and bitmap fonts on a host harness. Inspected the 800 × 480 output; corrected a pixel-fill endpoint artifact.
- Portal checked in the in-app browser using local mock device responses; inspected overview, Notes, and Library navigation. Screenshot is a mock preview, not evidence of hardware connectivity.

## Not yet hardware/live-provider verified

No board was connected and no firmware was flashed. Wi-Fi scanning/reconnect/captive behavior, SD uploads/downloads/bookmarks, microphone gain/capture, background TLS memory pressure, power behavior, sensor measurements, and physical button timing need device tests. Real Gemini/Whisper requests were not sent; no API key was provided. Home Assistant, calendar, weather and news integrations were tested with mocks only.

## First-board check

1. Flash (or use app-only upgrade if already running Wisp); verify readable app grid and BOOT-to-grid behavior.
2. Join setup Wi-Fi; scan/select the home network; reconnect through wisp.local or its displayed IP; verify the setup hotspot closes after two minutes.
3. Add/edit a typed note and task; restart and confirm persistence.
4. Insert FAT32 SD, upload a small TXT and DRM-free EPUB, read pages, restart and check the bookmark; download the original EPUB and compare its hash with the source.
5. Record a short voice note, stop with centre, download/play the WAV. Configure one provider and verify the resulting note; test retry after an offline recording.
6. Verify the focus timer and room sensor. Use USB power initially; battery optimization is not implemented.

0.2.1: Pocket Ghost replaces the cat on the board and portal. Both one-bit poses were rendered and inspected; the rebuilt ESP32-S3 image passes integrity checks. Hardware status remains unverified.

0.2.2: Renamed the project, sketch, dashboard, setup network, mDNS host, bridge, and flash package to Wisp. Retained the legacy NVS namespace for existing settings. Re-ran firmware build, core/EPUB/bridge tests, image integrity checks, and launcher rendering.

## 0.2.3 — Partial navigation updates

- Full ESP32-S3 build passed. Two additional 48 KB buffers are allocated in PSRAM; allocation failure falls back to full refresh.
- Frame-difference tests pass for unchanged images, byte-aligned bounds, edge pixels, packed row stride, erasing the old cursor, and idle cleanup across millis rollover.
- Host harness using the actual presentFrame function verifies initial full refresh, partial cursor changes, no display calls for unchanged frames, full refresh on app changes, cleanup deferred until 15 seconds without button input, and allocation fallback.
- Uses the bundled Waveshare Display_Base / Display_Partial sequence; controller is no longer deep-slept after each frame. The former three-second software delay is removed. Controller remains awake in USB desk mode; battery consumption has not been measured.
- Actual panel latency, ghosting, and partial-update positioning still require hardware verification. Navigate repeatedly in both directions, including grid wraparound and lists, then verify a single cleanup after 40 partial updates and 15 seconds idle. Repeated idle redraws must not flash unchanged content.
