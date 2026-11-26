# Code 1: a 28.9M parameter LLM on your CoreS3

This runs a small language model entirely on the ESP32-S3 in your CoreS3. Nothing goes to the cloud. It writes a short children's story starting "Once upon a time" on the screen, then shows how fast it ran.

It is a port of [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai) (`firmware/esp32_tinystories`, commit `52ebcbb`) to PlatformIO and the CoreS3 screen. Code and model are MIT licensed; see `LICENSE.upstream`.

## Run it

1. Copy `model.bin` from the workshop USB stick or SD card into the `model/` folder.
2. Plug in the CoreS3 and run, from this folder:

   ```
   pio run -t uploadmodel
   pio run -t upload
   pio device monitor
   ```

   `uploadmodel` writes the 15 MB model to its own flash partition. It takes a minute or two and only needs doing once per board. After that, `pio run -t upload` is enough for code changes.

3. Press the reset button if the story has already finished before the monitor connected.

If the upload cannot find the board, add `--upload-port COM5` (Windows) or `--upload-port /dev/cu.usbmodem1101` (Mac), using the port from `pio device list`.

## What to expect

Serial output starts like this:

```
=== ESP32-S3 PLE TinyLM ===
model: Vin=32768 Vout=25353 D=96 L=6 H=4 F=66 P=128  (mapped 15.6 MB)
...
weights-> PSRAM  44 tensors int8, 4.19 MB allocated
build: bytes=14912348 fp=a9bdd778 ...
```

The `fp=a9bdd778` fingerprint confirms the model flashed correctly. Then the story streams out, followed by a tokens per second figure. The upstream board does about 10 tokens per second; the CoreS3 may be slower because of its PSRAM.

## How it fits

The ESP32-S3 has 512 KB of fast SRAM, 8 MB of PSRAM and 16 MB of flash. The model puts each part where its read pattern belongs:

| Memory | Holds | Why |
|---|---|---|
| SRAM | activations, norm weights | touched many times per token |
| PSRAM | core layers and output head, int8 | read once per token |
| Flash | 25M parameter embedding table | only about 450 bytes read per token |

Ask Claude to walk you through `include/llm.h` and `src/main.cpp` with that table in mind.

## Files

| Path | What |
|---|---|
| `src/main.cpp` | Upstream sketch, unchanged apart from the include lines |
| `include/llm.h` | Upstream inference runtime, unchanged |
| `include/display.h` | New: CoreS3 screen via M5Unified |
| `include/generated/vocab.h` | Token decode table, generated from the tokenizer |
| `partitions.csv` | Upstream table: 1 MB app, model partition at `0x110000` |
| `tools/model_upload.py` | Adds the `uploadmodel` target |
| `tools/generate_vocab.py` | Upstream: rebuilds `vocab.h` (needs Python `tokenizers` and `tokenizer.json`) |

## For trainers

Model files come from [huggingface.co/slvDev/esp32-ai-tinystories](https://huggingface.co/slvDev/esp32-ai-tinystories). SHA-256 of the files used here:

```
1d8326c05c383ccfa615f5455575802817cb453dbc7ab28875d41a9dbb45477e  model.bin
4e28163669f2249af31a528a54fc25064dcbd0a34edbfa7bedb16d2d600ec7ae  tokenizer.json
```

`model.bin` and `tokenizer.json` are gitignored. Put `model.bin` on the participant USB sticks and SD cards.
