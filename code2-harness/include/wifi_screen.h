// WiFi screen: scan for networks, connect to the one in secrets.h, disconnect.
// The scan doubles as the pre-workshop hotspot check: the ESP32 only sees
// 2.4 GHz, so if your hotspot is not in the list it is on 5 GHz.
#pragma once
#include <WiFi.h>
#include "ui.h"
#include "secrets_check.h"

static Button wifi_scan_btn  = {8,   HEADER_H + 8, 96, 44, "Scan",    TFT_DARKCYAN, true};
static Button wifi_conn_btn  = {112, HEADER_H + 8, 96, 44, "Connect", TFT_DARKGREEN, true};
static Button wifi_disc_btn  = {216, HEADER_H + 8, 96, 44, "Drop",    TFT_MAROON, true};
static Button wifi_back_btn  = {8,   STATUS_Y - 40, 80, 34, "Back",   TFT_DARKGREY, true};

#define WIFI_LIST_Y (HEADER_H + 60)
#define CONNECT_TIMEOUT_MS 15000

static bool wifi_up() { return WiFi.status() == WL_CONNECTED; }

static void wifi_show_state() {
  char line[96];
  if (wifi_up()) {
    snprintf(line, sizeof line, "Connected to %s  IP %s  %d dBm",
             WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    status(line, TFT_GREEN);
  } else {
    snprintf(line, sizeof line, "Not connected. Configured network: %s", WIFI_SSID);
    status(line);
  }
}

static void wifi_draw() {
  draw_header("WiFi", wifi_up());
  clear_body();
  wifi_conn_btn.enabled = !wifi_up();
  wifi_disc_btn.enabled = wifi_up();
  draw_button(wifi_scan_btn);
  draw_button(wifi_conn_btn);
  draw_button(wifi_disc_btn);
  draw_button(wifi_back_btn);
  wifi_show_state();
}

static void wifi_scan() {
  auto &d = M5.Display;
  status("Scanning...");
  int n = WiFi.scanNetworks();
  d.fillRect(0, WIFI_LIST_Y, SCR_W, STATUS_Y - 44 - WIFI_LIST_Y, TFT_BLACK);
  d.setTextSize(1);
  bool found = false;
  int shown = 0;
  for (int i = 0; i < n && shown < 8; i++, shown++) {
    bool mine = WiFi.SSID(i) == WIFI_SSID;
    found |= mine;
    d.setTextColor(mine ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
    d.setCursor(8, WIFI_LIST_Y + shown * 11);
    d.printf("%s%-24.24s ch%-2d %4d dBm", mine ? "> " : "  ",
             WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
    Serial.printf("  %-32s ch%-2d %4d dBm\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
  }
  WiFi.scanDelete();
  char line[96];
  if (n <= 0) status("No networks found.", TFT_ORANGE);
  else if (found) { snprintf(line, sizeof line, "%d networks. %s is visible (2.4 GHz OK).", n, WIFI_SSID); status(line, TFT_GREEN); }
  else { snprintf(line, sizeof line, "%s not seen. Is the hotspot on and set to 2.4 GHz?", WIFI_SSID); status(line, TFT_ORANGE); }
}

static void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  char line[64];
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
    snprintf(line, sizeof line, "Connecting to %s ... %lus", WIFI_SSID, (millis() - start) / 1000);
    status(line);
    delay(500);
  }
  wl_status_t st = WiFi.status();
  if (st != WL_CONNECTED) {
    WiFi.disconnect(true);
    if (st == WL_NO_SSID_AVAIL) status("Network not found. Check the name and that it is 2.4 GHz.", TFT_ORANGE);
    else if (st == WL_CONNECT_FAILED) status("Connection refused. Check the password.", TFT_ORANGE);
    else status("Timed out. Hotel WiFi with a login page will not work: use a hotspot.", TFT_ORANGE);
    return;
  }
  wifi_draw();
}

static void wifi_disconnect() {
  WiFi.disconnect(true);
  delay(200);
  wifi_draw();
}

// Returns false when the user taps Back.
static bool wifi_handle_tap(int x, int y) {
  if (button_hit(wifi_back_btn, x, y)) return false;
  if (button_hit(wifi_scan_btn, x, y)) wifi_scan();
  else if (button_hit(wifi_conn_btn, x, y)) wifi_connect();
  else if (button_hit(wifi_disc_btn, x, y)) wifi_disconnect();
  return true;
}
