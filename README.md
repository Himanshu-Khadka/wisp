# Wisp 0.2.2 — app grid, notes, and an SD library

For **Waveshare ESP32-S3-ePaper-3.97 only**: ESP32-S3, 800 × 480 display, 16 MB flash, 8 MB PSRAM.

A compiled flash package is included. Firmware compilation and software tests pass; **physical-board operation is not yet verified**. No board was connected during this build. Start with USB power.

## What changed

- **3 × 3 app grid:** Today, Notes, Tasks, Focus, Library, Briefing, Home, Companion, Settings. Press BOOT from any app to return to the grid.
- **Pocket Ghost companion:** a simple ghost with idle and resting poses; no large character illustration.
- **Actual voice notes:** record up to 30 seconds, stop with centre, save audio, then transcribe into an editable note. Notes and tasks are separate. A note can become a task explicitly.
- **Direct cloud or local transcription:** Gemini from the board over verified HTTPS using your API key, or Whisper on your optional local bridge. Transcription runs in a background worker. A failed/offline recording can be retried from the browser.
- **Wi-Fi provisioning:** nearby-network scan, DNS captive-portal redirection, saved credentials, automatic reconnect, and `wisp.local` on your home network. New secured networks still require you to enter their password once.
- **SD library:** upload, list, download, delete, and open books from the browser. TXT is read directly. DRM-free EPUBs are converted into a text reading copy locally in the browser; both original EPUB and TXT are stored on SD.
- **New browser workspace:** Overview, Notes, Tasks & focus, Library, Settings. Phone-friendly layout, progress feedback, and explicit storage/network status.

`app-grid.png` is rendered from the actual device drawing functions on a host test harness. `pocket-ghost.png` shows the awake and resting sprites rendered by the firmware drawing code.

## Flashing

### First install

1. Extract the ZIP. Connect the board using a USB **data** cable.
2. Open `Flash-Mac.command` on Mac or `Flash-Windows.bat` on Windows. Python 3 is required. The helper installs esptool on its first run.
3. Choose the board's serial port and type `FLASH`.
4. The helper backs up the existing 16 MB flash before writing and verifying the firmware.
5. If it does not restart automatically, release BOOT and power cycle.

**A full installation clears internal settings and data.** The SD card is not erased. Backups may contain private credentials; keep them private.

If the board is not detected, hold BOOT while powering it on, release BOOT, and retry with a known data cable directly connected to the computer. No USB device was visible on the development Mac at the last hardware check. Official recovery instructions: https://docs.waveshare.com/ESP32-S3-ePaper-3.97/Firmware-Flashing

Terminal alternative, from this folder:

```sh
python3 -m venv .flash-env
.flash-env/bin/python -m pip install esptool==5.1.0
.flash-env/bin/python flash.py
```

On Windows substitute `.flash-env\Scripts\python.exe` and `py -3` as appropriate.

### Upgrade an existing Deskling / Wisp installation

To preserve internal settings and notes, use the **app-only upgrade**:

```sh
.flash-env/bin/python flash.py --upgrade
```

This backs up the existing firmware, verifies the partition table matches, and writes only the app at **0x10000**. It refuses an incompatible partition layout. Use this only for an existing Deskling or Wisp installation, not an unrelated firmware that happens to share the same partition layout.

For a different flasher: `bin/Wisp-0.2.2-ESP32-S3-ePaper-3.97.bin` is the **full merged image**, written at **0x0**. `bin/Wisp.ino.bin` is the **application-only image**, written at **0x10000** only on the matching Wisp layout. Do not swap these offsets.

`flash.py --verify-only` checks package integrity without touching a board. `--baud 115200` can help with a poor connection. `--skip-backup` is available only if you intentionally want to skip saving the current flash.

## First connection

1. The board starts a **Wisp-xxxx** setup Wi-Fi network when no saved network is available.
2. Open Settings on the board to see the network password and login details.
3. Join that Wi-Fi network. Your phone may open the setup page automatically; if it does not, open **http://192.168.4.1** in a full browser.
4. Log in as **admin** with the eight-digit password displayed on the board.
5. In browser Settings, choose **Find Wi-Fi networks**, select your 2.4 GHz network, enter its password, and save.
6. Rejoin your home Wi-Fi. Visit **http://wisp.local** or the board's IP address shown in its Settings app.
7. The setup hotspot closes two minutes after a successful connection. Centre on the board's Settings app enables it again. If the saved network is unavailable, the setup hotspot comes back automatically.

The portal requires login on both setup Wi-Fi and your home network. It uses HTTP and local, unencrypted storage; keep it on a trusted LAN and do not expose its port to the internet. API keys/passwords are never returned by the settings endpoint. Blank password/token/key fields retain saved values. Current provisioning stores one Wi-Fi profile.

Default timezone is Edmonton with daylight saving time. The timezone setting accepts POSIX rules; UTC is `UTC0`. **Sync phone clock** works without internet and is useful before a TLS transcription request.

## Board controls

| Location | Up / down | Centre | Hold centre | BOOT |
|---|---|---|---|---|
| App grid | Choose app | Open app | Open app | Grid |
| Notes | Select note / turn transcript page | Open / close note | Start recording | Grid |
| Tasks | Select task | Complete / uncomplete | Complete / uncomplete | Grid |
| Focus | — | Start / pause | Reset | Grid |
| Library shelf | Select book | Read book | — | Grid |
| Reading | Turn page | — | Back to shelf | Grid |
| Home | Select entity | Toggle allowed light/switch | Same | Grid |
| Companion | — | Greet companion | Same | Grid |
| Settings | — | Enable setup hotspot | Same | Grid |

Buttons normally act on release. A hold is over 0.8 seconds. During recording, a centre press stops capture. BOOT held during **power-on** enters the hardware download mode instead.

Display refresh takes several seconds; let it finish before another button press. The focus timer tracks real elapsed seconds while its display updates once per minute. Each 25-minute work interval and five-minute break starts manually. Focus progress resets on restart; completed-session count persists.

## Voice → Notes

Open browser Settings and choose a transcription provider:

- **Save audio only:** default. No audio is sent to a provider.
- **Gemini:** enter your own Gemini API key and an audio-capable model ID available to your account. The default model ID is `gemini-3.8-flash`; it is editable. The board sends audio directly to Google over certificate-verified HTTPS. Your provider's charges and data terms apply. A valid clock and internet are required.
- **Local bridge:** configure the bridge URL/token and enable its Whisper model. Recordings go to that computer for local transcription.

Then hold/release centre in Notes, or click **Record on board** in the browser. Wait for the recording screen to finish refreshing, speak, and press centre to stop. Capture stops automatically at 30 seconds. The capture phase blocks portal handling briefly; the transcription phase runs in the background and leaves the interface responsive.

Each recording is saved first. With SD, audio and note metadata go to `/notes/<id>.wav` and `/notes/<id>.json`. Notes are also indexed internally. SD note archives are discovered at startup if missing from the internal index. Without SD, only the latest audio file is retained; older transcripts remain but their audio download is removed when superseded.

The browser lets you edit notes, download recordings, retry transcription, delete notes/audio, or **Make a task**. Up to 40 notes of 4,000 characters and 30 tasks are supported. Conversion to a task uses its first 100 characters. The screen uses an ASCII bitmap font; the browser preserves Unicode note text. A second transcription is rejected while one is running.

Cloud transcription is implemented but has **not been exercised against a real API key** here. Microphone capture likewise needs the first-board test.

## Books on SD

Use a FAT32 microSD card and restart after inserting it. In the browser's Library:

1. Select a TXT or DRM-free EPUB file (up to 20 MB).
2. Choose **Upload to SD**.
3. For EPUB, the browser reads the package/spine order, extracts text, and uploads both `title.epub` and `title.txt`. It does not send your book to a conversion service; the decompressor is bundled and works without a CDN.
4. Choose **Read** to open the TXT copy on the board, or select it from the board's Library app.

The reader pages text from SD instead of loading the whole book into RAM. It remembers reading positions per filename. Up to 64 TXT books are shown on the device; the browser lists up to 128 files. New uploads reject duplicate filenames instead of overwriting. If the original EPUB uploads but its TXT copy fails, delete the partial pair through the browser before retrying.

Text-only reading: images, CSS typography, PDF, DRM, complex scripts, and native EPUB rendering are not supported. Unicode ebook text is normalized to ASCII for this screen. Original EPUB bytes remain intact for downloading elsewhere. Browser file uploads stream into `.part` files and are renamed only on success; oversized/failed uploads are removed.

## Optional bridge for weather, calendar, news, home, local speech

The bridge is not needed for Wi-Fi setup, direct Gemini voice notes, or ebook uploads.

From `bridge/`:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
cp config.example.json config.json
.venv/bin/python -c 'import secrets; print(secrets.token_urlsafe(32))'
```

Put the generated token in `config.json`. Set coordinates/timezone, optional private ICS calendar URL, RSS feeds, and optionally Home Assistant URL/token. Entities are read-only unless a `light` or `switch` appears in both `home_entities` and `controllable_entities`.

```sh
.venv/bin/python server.py --config config.json
```

Enter `http://YOUR_COMPUTER_LAN_IP:8787` and the token in the board's browser Settings. Keep the computer and server running. First data usually appears in about a minute; regular refresh is every 15 minutes. Allow the server's port on your local firewall. Do not use `localhost` as the board's bridge address.

For local transcription:

```sh
.venv/bin/python -m pip install faster-whisper
.venv/bin/python -c 'from faster_whisper import WhisperModel; WhisperModel("tiny.en", device="cpu", compute_type="int8")'
```

Set `"transcription_model": "tiny.en"`, restart the bridge, and select **Local bridge** on the board's Settings page. The warm-up command downloads the model before the first request. If dependency wheels are unavailable for a new Python release, try Python 3.11/3.12.

## Build and tests

Run `./build.sh` with Arduino CLI installed. It regenerates the embedded portal and pins ESP32 Arduino 3.3.2 / ArduinoJson 7.4.2. Target:

```
esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=default_8MB,CDCOnBoot=cdc
```

The 16 MB chip uses the lower 8 MB partition layout, including 3.2 MB app slots and 1.5 MB LittleFS. No OTA updater is implemented. `package.py` creates the merged binary and checksummed manifest.

```sh
c++ -std=c++17 tests/test_core.cpp -o /tmp/wisp-test && /tmp/wisp-test
bridge/.venv/bin/python -m unittest discover -s tests -v
npm install
npm test
python3 flash.py --verify-only
```

See `VALIDATION.md` and `REFERENCE_NOTES.md`. USB desk use remains the target: battery optimization, conversational AI, automatic cloud task synchronization, and audio playback UI are not implemented.

## Wisp rename (0.2.2)

Previously named Deskling. Existing Deskling 0.1/0.2 installations can use `--upgrade` to preserve settings and files. The internal NVS namespace remains `deskling` for compatibility. The setup network is now `Wisp-xxxx`, and the local dashboard is at `http://wisp.local`.
