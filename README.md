# AI-powered ESP32 development with PlatformIO

Workshop materials for the M5Stack CoreS3 / CoreS3 SE, built with Claude Code. Web Directions AI Engineer.

You will put a language model on a microcontroller, build a touch harness with WiFi and voice recording, then talk to an ElevenLabs voice agent from the device.

## Before the day

Follow [env/SETUP.md](env/SETUP.md). It takes about an hour, including a 5 GB toolchain download that the venue WiFi cannot handle. Then run the check script for your OS from `env/`. You need:

- A Mac or Windows laptop with VS Code, PlatformIO, Git and Claude Code
- Claude **Pro or higher** (the free plan does not include Claude Code)
- An ElevenLabs account and a phone hotspot on 2.4 GHz
- A USB data cable for the CoreS3

Then copy `secrets.example.h` to `secrets.h` in this folder and fill it in. `secrets.h` is gitignored. Never commit it.

## The workshop

| Part | Folder | What you do |
|---|---|---|
| Setup checks | [env/](env/) | Check your machine, build `hello-cores3`, confirm the board, SD card and hotspot |
| Code 1 | [code1-tinystories/](code1-tinystories/) | Flash a 28.9M parameter story model and see how it fits in 512 KB of SRAM |
| Code 2 | [code2-harness/](code2-harness/) | Build a touch menu, WiFi log in and out, and voice record and play to SD |
| Code 3 | [code2-harness/](code2-harness/) (Chat screen) | Press to chat with an ElevenLabs agent, then extend it with tools |

Each part has a README with the commands, fallback prompts for Claude Code, and common problems. Try building each feature yourself with Claude first. The code in each folder is a reference implementation to use when you are stuck or short of time.

## Saving your Claude usage

Embedded work fills Claude's context fast. To make your allowance last:

- Build with `pio run -s` so only errors are shown.
- Pipe serial output through `tail` or copy just the relevant lines.
- Start a fresh Claude conversation for each feature.

## Other files

- [FEASIBILITY.md](FEASIBILITY.md): what is realistic in 3.5 hours, and the risks
- [presentation/](presentation/): the instructor's slides, a single HTML file that works offline

## Licences

Workshop code: to be confirmed by the organisers. `code1-tinystories` is adapted from [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai) (MIT).
