// CoreS3 / CoreS3 SE screen for the TinyStories demo, via M5Unified.
// Replaces upstream's OLED and ST7789 display.h with the same three calls:
// display_begin, display_puts, display_stats.
#ifndef DISPLAY_H
#define DISPLAY_H

#include <M5Unified.h>

#define SCR_W 320
#define SCR_H 240
#define TEXT_SIZE 2
#define CHAR_W (6 * TEXT_SIZE)
#define CHAR_H (8 * TEXT_SIZE)
#define LINE_H (CHAR_H + 2)
#define HEADER_H 22
#define MARGIN 4

static int dx = MARGIN, dy = HEADER_H + MARGIN;

static void display_header() {
  auto &d = M5.Display;
  d.fillRect(0, 0, SCR_W, HEADER_H, TFT_NAVY);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_NAVY);
  d.setCursor(MARGIN, 7);
  d.print("TinyStories 28.9M  |  on-device LLM");
}

static void display_home() {
  auto &d = M5.Display;
  d.fillRect(0, HEADER_H, SCR_W, SCR_H - HEADER_H, TFT_BLACK);
  d.setTextSize(TEXT_SIZE);
  d.setTextColor(TFT_GREEN, TFT_BLACK);
  dx = MARGIN;
  dy = HEADER_H + MARGIN;
}

static void newline() {
  dx = MARGIN;
  dy += LINE_H;
  if (dy + CHAR_H > SCR_H) display_home();
}

static void display_begin() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;  // the sketch already started Serial
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setTextWrap(false);  // wrap at token boundaries ourselves
  M5.Display.fillScreen(TFT_BLACK);
  display_header();
  display_home();
}

// Append one token's bytes. Wrap whole tokens, clear when the screen fills.
static void display_puts(const unsigned char *s, int len) {
  auto &d = M5.Display;
  if (dx + len * CHAR_W > SCR_W - MARGIN) newline();
  for (int i = 0; i < len; i++) {
    char c = (char)s[i];
    if (c == '\n') { newline(); continue; }
    if (c < 32 || c >= 127) continue;
    if (dx + CHAR_W > SCR_W - MARGIN) newline();
    d.setCursor(dx, dy);
    d.write(c);
    dx += CHAR_W;
  }
}

// Closing stats card shown when generation finishes.
static void display_stats(float tok_s, float ms) {
  auto &d = M5.Display;
  d.fillScreen(TFT_BLACK);
  display_header();
  d.setTextColor(TFT_GREEN, TFT_BLACK);
  d.setTextSize(2);
  d.setCursor(MARGIN + 6, 40);  d.print("ESP32-S3  PLE TinyLM");
  d.setCursor(MARGIN + 6, 70);  d.print("28.9M params");
  d.setCursor(MARGIN + 6, 95);  d.print("in 320 KB of RAM");
  d.setTextSize(4);
  d.setCursor(MARGIN + 6, 135); d.print(tok_s, 1); d.print(" tok/s");
  d.setTextSize(2);
  d.setCursor(MARGIN + 6, 190); d.print(ms, 0); d.print(" ms/token");
}

#endif
