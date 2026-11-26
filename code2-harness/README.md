# Code 2: the harness

A touch menu for the CoreS3 with three working screens, which Code 3 builds on:

- **WiFi**: scan, connect to your hotspot, disconnect
- **Record**: hold the button to record your voice to the SD card
- **Play**: tap a recording to hear it back

**Try it yourself first.** Start a new folder, open Claude Code there and use the prompts below, one feature at a time. This folder is the reference implementation: use it if you get stuck, run short on time, or want to compare.

## Before you start

1. In the repo root, copy `secrets.example.h` to `secrets.h` and put in your hotspot name and password. `secrets.h` is gitignored. Never commit it.
2. Put a FAT32 micro SD card in the CoreS3.

## Run the reference

```
pio run -t upload
pio device monitor
```

## Fallback prompts

Paste these into Claude Code in an empty folder, in order. Each builds on the last. Tell Claude to keep build output short (`pio run -s`) to save your usage.

### 1. Harness UI

```
Create a PlatformIO project for the M5Stack CoreS3 (board m5stack-cores3,
Arduino framework, lib m5stack/M5Unified). Show a header bar and a home
screen with four big touch buttons: WiFi, Record, Play, Chat. Chat is
greyed out for now. Tapping a button opens that screen, each screen has a
Back button. Use M5.Touch wasClicked() for taps. Add -I${PROJECT_DIR}/..
to build_flags so it can include secrets.h from the parent folder. Build
it with pio run -s and fix any errors.
```

### 2. WiFi log in and out

```
On the WiFi screen add Scan, Connect and Disconnect buttons. Scan lists up
to 8 networks with channel and RSSI and highlights WIFI_SSID from
secrets.h. Connect uses WIFI_SSID and WIFI_PASSWORD, times out after 15 s
and explains the failure: network not found (probably 5 GHz), wrong
password, or timeout (captive portal). Show IP and RSSI when connected,
and a green or red dot in the header. Include secrets.h through a
__has_include check with a clear #error if it is missing.
```

### 3. Record and play to SD

```
Add a Record screen: hold a big button to record 16 kHz mono from M5.Mic
into a PSRAM buffer (max 30 s), show a level meter, and on release save a
WAV file to /rec/recNNN.wav on the SD card. Add a Play screen listing the
newest six recordings as buttons; tapping one plays it with
M5.Speaker.playWav, tapping again stops it.
CoreS3 details: SD is on SPI with CS 4, SCK 36, MISO 35, MOSI 37, shared
with the screen, so start M5 first, then SPI.begin and SD.begin(4, SPI,
25000000). The mic and speaker share one I2S bus: call M5.Speaker.end()
before M5.Mic.begin(), and M5.Mic.end() before M5.Speaker.begin(). Keep
the WAV buffer allocated until playback finishes.
```

## Common problems

| Symptom | Cause |
|---|---|
| `secrets.h not found` | Copy `secrets.example.h` to `secrets.h` in the repo root |
| Hotspot not in the scan | It is on 5 GHz. iPhone: Maximise Compatibility. Android: band 2.4 GHz |
| Connect times out on venue WiFi | Captive login page. Use a phone hotspot |
| Recording is silent or the speaker stops working | Mic and speaker both started. End one before beginning the other |
| `No SD card` | Card not FAT32, or not pushed in fully. Reset after inserting |
| Playback crashes | WAV buffer freed while still playing |

## Files

| Path | What |
|---|---|
| `src/main.cpp` | Home menu and screen switching |
| `include/ui.h` | Header, buttons, status line, tap helper |
| `include/wifi_screen.h` | Scan, connect, disconnect |
| `include/recorder.h` | SD setup, WAV writing, record and play screens |
