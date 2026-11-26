// Code 2 reference harness for the M5Stack CoreS3 / CoreS3 SE.
//
// Home menu with four screens:
//   WiFi    scan, connect to the network in secrets.h, disconnect
//   Record  hold to record, saved as WAV on the SD card
//   Play    tap a recording to hear it
//   Chat    placeholder for Code 3 (ElevenLabs voice assistant)
//
// Each screen lives in its own header: wifi_screen.h, recorder.h. Add a new
// screen by adding a Screen value, a home button and a case in loop().
#include <M5Unified.h>
#include "ui.h"
#include "wifi_screen.h"
#include "recorder.h"

enum Screen { HOME, WIFI_SCREEN, RECORD, PLAY };
static Screen screen = HOME;

static Button home_btns[] = {
  {12,  HEADER_H + 12, 142, 80, "WiFi",   TFT_DARKCYAN,  true},
  {166, HEADER_H + 12, 142, 80, "Record", TFT_RED,       true},
  {12,  HEADER_H + 104, 142, 80, "Play",  TFT_DARKGREEN, true},
  {166, HEADER_H + 104, 142, 80, "Chat",  TFT_PURPLE,    false},  // Code 3
};

static void home_draw() {
  draw_header("ESP32 AI Workshop", wifi_up());
  clear_body();
  for (auto &b : home_btns) draw_button(b);
  char line[64];
  snprintf(line, sizeof line, "SD: %s   PSRAM free: %u KB", sd_ok ? "ok" : "missing",
           (unsigned)(ESP.getFreePsram() / 1024));
  status(line, sd_ok ? TFT_LIGHTGREY : TFT_ORANGE);
}

static void go(Screen s) {
  screen = s;
  switch (s) {
    case HOME:        home_draw(); break;
    case WIFI_SCREEN: wifi_draw(); break;
    case RECORD:      rec_draw();  break;
    case PLAY:        play_draw(); break;
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  Serial.println("\n=== ESP32 AI Workshop harness ===");

  WiFi.mode(WIFI_STA);
  sd_begin();
  go(HOME);
}

void loop() {
  M5.update();
  int x, y;

  switch (screen) {
    case HOME:
      if (tapped(x, y)) {
        if (button_hit(home_btns[0], x, y)) go(WIFI_SCREEN);
        else if (button_hit(home_btns[1], x, y)) go(RECORD);
        else if (button_hit(home_btns[2], x, y)) go(PLAY);
      }
      break;
    case WIFI_SCREEN:
      if (tapped(x, y) && !wifi_handle_tap(x, y)) go(HOME);
      break;
    case RECORD:
      if (!rec_handle()) go(HOME);
      break;
    case PLAY:
      if (tapped(x, y) && !play_handle_tap(x, y)) go(HOME);
      break;
  }
  delay(10);
}
