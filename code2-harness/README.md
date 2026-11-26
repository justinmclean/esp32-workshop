# Code 2 and 3: the harness

A touch menu for the CoreS3. Code 2 is the first three screens; Code 3 adds the fourth:

- **WiFi**: scan, connect to your hotspot, disconnect
- **Record**: hold the button to record your voice to the SD card
- **Play**: tap a recording to hear it back
- **Chat** (Code 3): hold to talk to an ElevenLabs voice agent, which can also put text on the screen

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
| `include/chat.h` | Code 3: ElevenLabs conversation, push to talk, client tool |
| `include/ws_client.h` | Small secure WebSocket client (handles big audio messages) |
| `include/ca_certs.h` | Root certificates for `api.elevenlabs.io` |

## Code 3: press to chat

### Set up an agent (once, in the ElevenLabs web app)

1. Create an agent. Give it a short first message, such as "Hi, I'm running on an ESP32. What shall we talk about?"
2. Make it **public** (no authentication). The device then needs only the agent ID, not your API key.
3. In the agent's voice or advanced settings, set both the **input and output audio format to PCM 16000 Hz**. The device reads the formats when it connects and refuses ulaw.
4. Add a **client tool** named `show_message` with one string parameter, `text`, described as "Show a short message on the user's device screen". Tick "wait for response".
5. Put the agent ID in `secrets.h` as `ELEVENLABS_AGENT_ID`.

Exact menu names in the ElevenLabs app change over time; ask a helper if you cannot find a setting.

### Use it

Connect WiFi first, then open Chat and tap Connect. Hold the button while you speak and let go when you finish. The reply plays once the agent has finished sending it. Ask it to "show hello on my screen" to try the tool. Leaving the screen hangs up, so you only use credits while the screen is open.

### How it works

| Step | Message |
|---|---|
| Connect | `wss://api.elevenlabs.io/v1/convai/conversation?agent_id=...`, then send `{"type":"conversation_initiation_client_data"}` |
| Server says hello | `conversation_initiation_metadata` with `user_input_audio_format` and `agent_output_audio_format`, such as `pcm_16000` |
| You talk | `{"user_audio_chunk":"<base64 16-bit PCM>"}` every 100 ms while held, then about 1.2 s of silence so the server detects the end of your turn |
| Agent replies | `audio` events (`audio_event.audio_base_64`), plus `user_transcript` and `agent_response` text |
| Keep alive | reply to each `ping` with `{"type":"pong","event_id":N}` |
| Tool call | `client_tool_call`, answered with `client_tool_result` |

### Fallback prompts

```
Add a Chat screen to my CoreS3 harness that talks to a public ElevenLabs
agent over a secure WebSocket: wss://api.elevenlabs.io/v1/convai/conversation
?agent_id=ELEVENLABS_AGENT_ID (from secrets.h). Send
{"type":"conversation_initiation_client_data"} on connect, read the audio
formats from conversation_initiation_metadata, answer every ping with a pong.
While a button is held, stream 100 ms mic chunks as
{"user_audio_chunk":"<base64 PCM>"}; on release send 1.2 s of silence.
Buffer the base64 audio events into PSRAM and play the reply with
M5.Speaker.playRaw when no audio has arrived for 700 ms. Show the
user_transcript and agent_response text. Remember the mic and speaker share
one I2S bus on the CoreS3. Incoming audio messages can exceed 15 KB, so do
not use a WebSocket library with a 15 KB limit.
```

```
Handle the ElevenLabs client_tool_call message. For tool_name
"show_message", show parameters.text on the screen and reply with
{"type":"client_tool_result","tool_call_id":...,"result":"...","is_error":false}.
Unknown tools get is_error true.
```

### Ideas to extend it

- **Reverse tool: record.** Add a `start_recording` client tool. When the agent calls it, wait for its reply to finish playing (the mic cannot run while the speaker plays), record five seconds to the SD card, then send the result.
- **Play while it streams.** Start playback as soon as about half a second of audio has arrived instead of waiting for the whole reply.
- **Activity mode.** Keep the mic open, measure the level of each chunk, and only open a conversation (or send a clip to ElevenLabs speech to text) when the level crosses a threshold. Cap clip length so it cannot burn your credits.
- **Keyword tools.** Check the `user_transcript` text for words like "lights" or "record" and trigger device actions locally.

### Known gaps, to check in the dry run

- The ElevenLabs docs do not define a push to talk end-of-turn message. The trailing silence relies on the server's voice detection.
- Agent minutes per test conversation, so we know how far the free tier goes.
