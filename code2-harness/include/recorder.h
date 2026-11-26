// Voice recorder: hold to record into PSRAM, save as a WAV file on the SD
// card, tap a file to play it back.
//
// Two CoreS3 facts this code works around:
//  - The mic and speaker share one I2S bus. Only one runs at a time, so we
//    end() one before begin() on the other.
//  - The SD card shares the SPI bus with the screen. M5Unified starts the
//    screen first; SD then joins the same bus with its own chip select.
#pragma once
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include "ui.h"

#define SD_CS   4
#define SD_SCK  36
#define SD_MISO 35
#define SD_MOSI 37

#define REC_DIR      "/rec"
#define SAMPLE_RATE  16000
#define CHUNK        512                          // samples per mic request
#define MAX_SECONDS  30
#define MAX_SAMPLES  (SAMPLE_RATE * MAX_SECONDS)

static bool sd_ok = false;

static Button rec_btn      = {60, HEADER_H + 30, 200, 90, "Hold to talk", TFT_RED, true};
static Button rec_back_btn = {8, STATUS_Y - 40, 80, 34, "Back", TFT_DARKGREY, true};

// ---- SD -------------------------------------------------------------------

static bool sd_begin() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sd_ok = SD.begin(SD_CS, SPI, 25000000);
  if (sd_ok && !SD.exists(REC_DIR)) SD.mkdir(REC_DIR);
  Serial.printf("SD: %s\n", sd_ok ? "mounted" : "not found");
  return sd_ok;
}

static void next_rec_path(char *out, size_t n) {
  for (int i = 1; i < 1000; i++) {
    snprintf(out, n, REC_DIR "/rec%03d.wav", i);
    if (!SD.exists(out)) return;
  }
  snprintf(out, n, REC_DIR "/rec999.wav");
}

// ---- WAV ------------------------------------------------------------------

static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

// 44 byte header for 16-bit mono PCM.
static void wav_header(uint8_t *h, uint32_t samples) {
  uint32_t data_bytes = samples * 2;
  memcpy(h, "RIFF", 4);      put32(h + 4, 36 + data_bytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(h + 16, 16);         put16(h + 20, 1);           // PCM
  put16(h + 22, 1);          put32(h + 24, SAMPLE_RATE); // mono, rate
  put32(h + 28, SAMPLE_RATE * 2); put16(h + 32, 2);      // byte rate, block align
  put16(h + 34, 16);         memcpy(h + 36, "data", 4);
  put32(h + 40, data_bytes);
}

static bool save_wav(const char *path, const int16_t *pcm, uint32_t samples) {
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  uint8_t h[44];
  wav_header(h, samples);
  bool ok = f.write(h, 44) == 44 &&
            f.write((const uint8_t *)pcm, samples * 2) == samples * 2;
  f.close();
  return ok;
}

// ---- record ---------------------------------------------------------------

static void draw_level(int peak) {
  int w = map(constrain(peak, 0, 16000), 0, 16000, 0, 240);
  M5.Display.fillRect(40, HEADER_H + 132, 240, 12, TFT_DARKGREY);
  M5.Display.fillRect(40, HEADER_H + 132, w, 12, peak > 14000 ? TFT_RED : TFT_GREEN);
}

static void rec_draw() {
  draw_header("Record", WiFi.status() == WL_CONNECTED);
  clear_body();
  rec_btn.enabled = sd_ok;
  draw_button(rec_btn);
  draw_button(rec_back_btn);
  status(sd_ok ? "Hold the button and speak. Up to 30 seconds."
               : "No SD card. Insert a FAT32 card and reset.", sd_ok ? TFT_LIGHTGREY : TFT_ORANGE);
}

static void record_while_held() {
  int16_t *pcm = (int16_t *)ps_malloc(MAX_SAMPLES * sizeof(int16_t));
  if (!pcm) { status("Out of PSRAM.", TFT_RED); return; }

  M5.Speaker.end();
  M5.Mic.begin();
  draw_button(rec_btn, true);

  // Keep two requests queued so capture is continuous. Each record() call
  // queues CHUNK samples straight after the previous request.
  uint32_t queued = 0;
  uint32_t last_draw = 0;
  char line[48];
  while (queued + CHUNK <= MAX_SAMPLES) {
    M5.update();
    if (!M5.Touch.getDetail().isPressed()) break;
    if (M5.Mic.record(pcm + queued, CHUNK, SAMPLE_RATE)) queued += CHUNK;
    if (millis() - last_draw > 100 && queued >= 3 * CHUNK) {
      // Two newest chunks may still be filling; measure the one before.
      const int16_t *done = pcm + queued - 3 * CHUNK;
      int peak = 0;
      for (int i = 0; i < CHUNK; i++) peak = max(peak, abs((int)done[i]));
      draw_level(peak);
      snprintf(line, sizeof line, "Recording %.1f s", queued / (float)SAMPLE_RATE);
      status(line, TFT_RED);
      last_draw = millis();
    }
  }
  while (M5.Mic.isRecording()) delay(1);   // let the last chunks fill
  M5.Mic.end();
  draw_button(rec_btn);

  if (queued < SAMPLE_RATE / 2) {
    status("Too short. Hold the button while you speak.", TFT_ORANGE);
  } else {
    char path[32];
    next_rec_path(path, sizeof path);
    if (save_wav(path, pcm, queued)) {
      snprintf(line, sizeof line, "Saved %s (%.1f s)", path, queued / (float)SAMPLE_RATE);
      status(line, TFT_GREEN);
    } else {
      status("Could not write to the SD card.", TFT_RED);
    }
  }
  free(pcm);
}

// Returns false when the user taps Back. Uses press, not tap, for recording.
static bool rec_handle() {
  auto t = M5.Touch.getDetail();
  if (t.wasPressed() && button_hit(rec_btn, t.x, t.y)) { record_while_held(); return true; }
  if (t.wasClicked() && button_hit(rec_back_btn, t.x, t.y)) return false;
  return true;
}

// ---- play -----------------------------------------------------------------

#define PLAY_MAX_FILES 6
static char play_files[PLAY_MAX_FILES][32];
static int play_count = 0;
static Button play_btns[PLAY_MAX_FILES];
static Button play_back_btn = {8, STATUS_Y - 40, 80, 34, "Back", TFT_DARKGREY, true};

// Newest files first.
static void play_scan() {
  play_count = 0;
  if (!sd_ok) return;
  static char all[200][32];   // static: too big for the loop task stack
  int n = 0;
  File dir = SD.open(REC_DIR);
  for (File f = dir.openNextFile(); f && n < 200; f = dir.openNextFile()) {
    if (!f.isDirectory() && String(f.name()).endsWith(".wav"))
      snprintf(all[n++], 32, REC_DIR "/%s", f.name());
    f.close();
  }
  dir.close();
  qsort(all, n, 32, [](const void *a, const void *b) { return -strcmp((const char *)a, (const char *)b); });
  for (int i = 0; i < n && i < PLAY_MAX_FILES; i++) strcpy(play_files[play_count++], all[i]);
}

static void play_draw() {
  draw_header("Play", WiFi.status() == WL_CONNECTED);
  clear_body();
  play_scan();
  for (int i = 0; i < play_count; i++) {
    int col = i % 2, row = i / 2;
    play_btns[i] = {8 + col * 156, HEADER_H + 8 + row * 46, 148, 40,
                    play_files[i] + strlen(REC_DIR) + 1, TFT_DARKCYAN, true};
    draw_button(play_btns[i]);
  }
  draw_button(play_back_btn);
  if (!sd_ok) status("No SD card.", TFT_ORANGE);
  else if (!play_count) status("No recordings yet. Make one on the Record screen.");
  else status("Tap a recording to play it. Tap again to stop.");
}

static void play_file(const char *path) {
  File f = SD.open(path);
  if (!f) { status("Could not open file.", TFT_RED); return; }
  size_t len = f.size();
  uint8_t *wav = (uint8_t *)ps_malloc(len);
  if (!wav) { f.close(); status("File too big for PSRAM.", TFT_RED); return; }
  f.read(wav, len);
  f.close();

  M5.Mic.end();
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);
  M5.Speaker.playWav(wav, len);
  char line[48];
  snprintf(line, sizeof line, "Playing %s", path);
  status(line, TFT_GREEN);
  while (M5.Speaker.isPlaying()) {
    M5.update();
    if (M5.Touch.getDetail().wasClicked()) M5.Speaker.stop();
    delay(5);
  }
  free(wav);   // only after playback ends: the speaker reads from this buffer
  status("Done.");
}

static bool play_handle_tap(int x, int y) {
  if (button_hit(play_back_btn, x, y)) return false;
  for (int i = 0; i < play_count; i++)
    if (button_hit(play_btns[i], x, y)) { play_file(play_files[i]); break; }
  return true;
}
