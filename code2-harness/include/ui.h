// Minimal touch UI for the CoreS3 320x240 screen: a header bar, a status
// line and big tappable buttons. Kept deliberately simple so it is easy to
// read and extend.
#pragma once
#include <M5Unified.h>

#define SCR_W 320
#define SCR_H 240
#define HEADER_H 26
#define STATUS_Y (SCR_H - 22)

struct Button {
  int x, y, w, h;
  const char *label;
  uint16_t colour;
  bool enabled;
};

static bool button_hit(const Button &b, int x, int y) {
  return b.enabled && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

static void draw_button(const Button &b, bool pressed = false) {
  auto &d = M5.Display;
  uint16_t fill = !b.enabled ? TFT_DARKGREY : pressed ? TFT_WHITE : b.colour;
  uint16_t text = pressed ? TFT_BLACK : TFT_WHITE;
  d.fillRoundRect(b.x, b.y, b.w, b.h, 10, fill);
  d.setTextColor(text, fill);
  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
  d.setTextDatum(top_left);
}

// Header with a title on the left and a WiFi indicator on the right.
static void draw_header(const char *title, bool wifi_up) {
  auto &d = M5.Display;
  d.fillRect(0, 0, SCR_W, HEADER_H, TFT_NAVY);
  d.setTextSize(2);
  d.setTextColor(TFT_WHITE, TFT_NAVY);
  d.setCursor(8, 5);
  d.print(title);
  d.fillCircle(SCR_W - 14, HEADER_H / 2, 6, wifi_up ? TFT_GREEN : TFT_RED);
}

static void clear_body() {
  M5.Display.fillRect(0, HEADER_H, SCR_W, SCR_H - HEADER_H, TFT_BLACK);
}

// One line of status text along the bottom of the screen.
static void status(const char *msg, uint16_t colour = TFT_LIGHTGREY) {
  auto &d = M5.Display;
  d.fillRect(0, STATUS_Y, SCR_W, SCR_H - STATUS_Y, TFT_BLACK);
  d.setTextSize(1);
  d.setTextColor(colour, TFT_BLACK);
  d.setCursor(8, STATUS_Y + 7);
  d.print(msg);
  Serial.println(msg);
}

// Returns true once per tap, with the tap position.
static bool tapped(int &x, int &y) {
  auto t = M5.Touch.getDetail();
  if (!t.wasClicked()) return false;
  x = t.x;
  y = t.y;
  return true;
}
