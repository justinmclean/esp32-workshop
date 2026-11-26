# Feasibility review: ESP32 AI workshop on M5Stack CoreS3 SE

Scope: 10 to 15 participants, 3.5 hours, Claude Code, CoreS3 SE, ElevenLabs. Written 26 Nov 2026 from the docs, the esp32-ai source and a test build on macOS. No board has been tested yet, so anything marked **verify on hardware** is a desk judgement.

## Headline findings

1. **Claude Code is not on the free Claude plan.** Every participant needs Pro or better (or an API Console account). The brief assumes "free or basic paid"; change the pre-event email to say Pro is required, not recommended.
2. **Code 1 (esp32-ai TinyStories) will not run as is on the CoreS3.** It is an Arduino CLI project for a bare ESP32-S3 N16R8 with octal PSRAM and a wired OLED, flashed with Mac-only bash scripts. It needs porting before the day: PlatformIO, the CoreS3 screen, and probably quad PSRAM. Once ported it should fit and run. Participants should *flash* it, not port it.
3. **Pro usage limits are the real time limit for the hackathon.** Embedded work burns context fast (build logs, serial output). Expect some participants to hit their 5 hour window during Code 3. Fallback prompts and reference implementations matter more than usual.
4. **ElevenLabs free tier gives about 15 minutes of agent conversation.** Enough for Code 3 testing if people use press to talk, not enough for always-on monitoring. Use the company-supplied keys for Code 3.
5. **Mic and speaker share one I2S bus on the CoreS3.** Audio is half duplex in practice: stop the speaker to record, stop the mic to play. That suits press to talk and makes the "reverse tools" and always-on items harder.

## Item by item

Ratings: **Feasible** (most participants finish in the slot with Claude Code), **Stretch** (keen participants, or with the reference implementation), **Not feasible** in 3.5 hours.

| Item | Rating | Notes |
|---|---|---|
| Pre-code 1: first boot, demo app | Feasible | Factory demo. Allow 10 min, mostly cables. |
| Pre-code 2: SD card check | Feasible | Cards must be FAT32 and 32 GB or smaller to be safe. Preformat the supplied cards. |
| Pre-code 3: clone repo | Feasible | Only if done before the day. Toolchain is about 5 GB. |
| Pre-code 4: env check | Feasible | Use `env/check-*.{ps1,sh}`. Budget 20 min for the stragglers, mostly PATH and OneDrive on Windows. |
| Pre-code 5: ElevenLabs account and key | Feasible | Free signup is quick. Keys must go in a gitignored file (see risks). |
| Pre-code 6: 2.4 GHz hotspot | Feasible | Biggest live risk. Test with a WiFi scan sketch, not just the phone setting. |
| Code 1: local AI story app | Feasible **if pre-ported** | See next section. As a porting exercise it is Not feasible in the slot. |
| Code 2: harness UI | Feasible | M5Unified and M5GFX make a touch menu straightforward for Claude. Give a skeleton. |
| Code 2: WiFi log in and out | Feasible | Hard-coded credentials first. Most failures will be 5 GHz or captive portal, not code. |
| Code 2: record and play to SD | Feasible | M5Unified `M5.Mic` and `M5.Speaker` plus WAV header. Known traps: SD shares the SPI bus with the display, and mic and speaker cannot run together. Put both in the fallback prompt. |
| Code 3: press to chat with an ElevenLabs agent | Stretch | Agents WebSocket API, 16 kHz PCM in and out, base64 JSON frames, TLS. Doable on the S3 with PSRAM, but a lot to get right in one session. Most will need the reference implementation. Use a public agent (agent ID only, no key on device) to keep it simple. |
| Code 3: continuous monitoring with threshold, then speech to text | Stretch | The agenda says TTS; it should be **speech to text** (ElevenLabs Scribe). Energy threshold detection is easy. Credit burn is the issue: cap clip length and rate. |
| Code 3: keyword checks to invoke tools | Stretch | Simple string match on the STT transcript is fine. |
| Code 3: reverse tools (agent starts or stops recording) | Stretch, hard | Agent client tools arrive over the same WebSocket, so the plumbing is there. Recording while the agent is talking collides with the shared I2S bus. Scope it as "agent triggers a recording after it finishes speaking". |
| Advanced: dual mic focus forming | Not feasible as written | Hand-written beamforming is research-grade. The realistic version is ESP-SR's AFE with two mic channels for noise suppression. Even that is a long evening, not a slot. |
| Advanced: local wake word (WakeNet, MultiNet) | Stretch, demo only | ESP-SR supports the S3 with stock English wake words and commands. Needs its own `model` flash partition and owns the I2S input, which fights M5Unified. Feasible as a trainer demo, not as participant work. No custom wake words. |
| Advanced: local WiFi setup UI | Feasible | WiFiManager-style captive portal is a well-trodden library. Good for fast finishers. |

## Code 1 in detail

What the esp32-ai TinyStories firmware expects, from its source:

- Arduino CLI, FQBN `esp32:esp32:esp32s3` with `PSRAM=opi`, `FlashSize=16M`, a custom partition table, and Arduino core 3.x APIs (`esp_partition_mmap`).
- A 14.9 MB model written to a custom `model` partition at `0x110000` with esptool, then the firmware. The app partition is 1 MB.
- A 128x64 I2C OLED on GPIO 18 and 46, or an ST7789 SPI screen. Not the CoreS3 screen.
- `scripts/fetch_model.sh` and `scripts/deploy.sh`, bash, using `uv` and `/dev/cu.usbmodem*`. They will not run on Windows PowerShell.
- 9.88 tokens per second on the original board. The output head is PSRAM bandwidth bound (59 of 95 ms per token).

What this means for the CoreS3 SE:

- **Flash fits.** CoreS3 SE has 16 MB flash and 8 MB PSRAM. Model plus 1 MB app plus coredump fits the same table. If M5Unified pushes the app over 1 MB, move the model up: it needs about 14.3 MB of its 14.9 MB slot.
- **PSRAM type probably differs.** PlatformIO's `m5stack-cores3` board definition configures quad PSRAM, not octal. If so, expect roughly half the PSRAM bandwidth and something like 5 to 7 tokens per second. Still a fine demo. **Verify on hardware**: the sketch prints its PSRAM allocation at boot.
- **Display needs a small port.** Replace `display.h` with an M5GFX version (`display_begin`, `display_puts`). Under 100 lines.
- **Toolchain fits.** PlatformIO espressif32 8.0.0 here ships Arduino core 3.3.x, so the IDF 5 partition API is available. A minimal CoreS3 build already succeeds on this Mac.
- **Model delivery.** Participants download `model.bin` (15 MB) from Hugging Face, ideally before the day; 15 people is about 225 MB in total. Keep a copy on a USB stick in case the network is poor. The model is MIT licensed, so redistributing it is fine.

Recommended shape for Code 1: the trainer repo ports it once to a PlatformIO project for `m5stack-cores3` with the custom partition table and an M5GFX `display.h`, plus a cross-platform upload step (`pio run -t upload`, then `pio pkg exec -p tool-esptool -- esptool.py write_flash 0x110000 model.bin`). Participants clone, flash and watch it run in under 20 minutes. Claude's job in Code 1 is explaining the memory tiers, not porting.

**Status:** ported in `code1-tinystories/`. Builds for `m5stack-cores3` (firmware 85% of the 1 MB app partition, upstream partition table unchanged). `pio run -t uploadmodel` writes the model with PlatformIO's own esptool, so it works on Windows too. Not yet run on a board.

## Time budget

| Block | Minutes | Notes |
|---|---:|---|
| Intro, tour, aims | 20 | |
| Pre-code 1 to 6 | 45 | Shorter only if setup was done at home |
| Code 1 | 25 | Flash and explain |
| Break | 15 | |
| Code 2 | 50 | |
| Code 3 hackathon | 55 | Realistically one Code 3 feature per participant |
| **Total** | **210** | No slack. Cut Code 2 to two items if pre-code overruns. |

## Risks and mitigations

- **API keys in public repos.** The participant repo goes public. Ship `secrets.example.h` and gitignore `secrets.h` from the first commit. Brief everyone not to push keys from their forks.
- **Venue WiFi.** Captive portals block the ESP32. Hotspots on 2.4 GHz, plus one trainer hotspot as a spare. The ElevenLabs WebSocket and Claude Code both need it at once.
- **Claude Pro limits.** Tell people to use `pio run -s` and pipe serial output through `tail`, and to start fresh conversations per feature. Keep the reference implementations one `git checkout` away.
- **USB cables and ports.** Charge-only cables will eat the first 10 minutes. Bring a box of known-good data cables.
- **Board firmware state.** A crashing sketch can stop USB enumerating. Teach "hold reset 3 seconds for download mode" in pre-code 1.
- **ElevenLabs credits.** One free account per person for Code 2 and early Code 3. Company keys for Code 3 agent work. Cap continuous monitoring by design.

## To verify in the dry run

1. PSRAM type and token rate of the ported TinyStories build on a real CoreS3 SE.
2. Record and play to SD round trip with M5Unified, including the SPI sharing.
3. Press to chat end to end with a public ElevenLabs agent, and how many agent minutes a 3 minute test uses.
4. `check-windows.ps1` on a clean Windows 11 laptop.
5. How far Claude Pro gets through Code 2 and one Code 3 item before hitting limits.
6. ~~Push to talk turn ending.~~ Checked from a laptop with `tools/agent_check.py`: 1.2 s of trailing silence ends the turn reliably, and reply audio starts about 1 s after release.
7. The reference WebSocket client on the ESP32: TLS with the pinned Google Trust Services roots. Message size is confirmed to matter: half of all messages in a test conversation were over 15 KB, up to 41 KB.

Reference implementations for Code 2 and Code 3 are in `code2-harness/` (Chat screen for Code 3). Both build; neither has run on a board.

## Sources

- [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai): README, `firmware/esp32_tinystories`, `scripts/deploy.sh`
- [M5Stack CoreS3 SE docs](https://docs.m5stack.com/en/core/CoreS3-SE), [Hackster on the SE's removed sensors](https://www.hackster.io/news/m5stack-s-cores3-se-drops-the-sensors-slashes-the-price-53fa4adb680c)
- [ElevenLabs Agents WebSocket API](https://elevenlabs.io/docs/eleven-agents/libraries/web-sockets), [ElevenLabs Conversational AI overview](https://elevenlabs.io/docs/conversational-ai)
- [Claude Pro plan](https://support.claude.com/en/articles/8325606-what-is-the-pro-plan), [Pro plan usage](https://support.claude.com/en/articles/8324991-about-claude-s-pro-plan-usage)
- [ESP-SR model loading on ESP32-S3](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/flash_model/README.html), [esp-sr README](https://raw.githubusercontent.com/espressif/esp-sr/HEAD/README.md)
