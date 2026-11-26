// Cardputer Adv smoke test for the workshop stack. Runs each check once at
// boot and shows PASS / FAIL on screen and serial. Press G0 (BtnA) to rerun.
//
//   1. SD card: mount, write, read back
//   2. Mic and speaker: record 2 s, report the level, play it back
//   3. WiFi: connect with secrets.h
//   4. ElevenLabs: TLS with the pinned roots, WebSocket upgrade, metadata,
//      audio messages (including any over 15 KB), then play the greeting
//
// No PSRAM on this board, so audio buffers are small and in internal RAM.
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  // here so PlatformIO finds it for ws_client.h
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "secrets_check.h"
#include "ws_client.h"
#include "ca_certs.h"

// Cardputer SD pins
#define SD_SCK 40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS 12

#define RATE 16000
#define AUDIO_SAMPLES (RATE * 2)          // 2 s, 64 KB

static int16_t *audio;
static int passes, fails;

static void line(uint16_t colour, const char *fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  M5.Display.setTextColor(colour, TFT_BLACK);
  M5.Display.println(buf);
  Serial.println(buf);
}

static void result(const char *name, bool ok, const char *detail = "") {
  ok ? passes++ : fails++;
  line(ok ? TFT_GREEN : TFT_RED, "%s %s %s", ok ? "PASS" : "FAIL", name, detail);
}

static void free_heap() {
  line(TFT_DARKGREY, "  free heap %u KB, largest block %u KB",
       (unsigned)(ESP.getFreeHeap() / 1024),
       (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024));
}

// ---- 1. SD -------------------------------------------------------------------

static void test_sd() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  // The serial log shows why a mount fails: "no valid FAT volume" means the
  // card is exFAT or NTFS; a physical drive error means no card or bad contact.
  uint32_t hz = 25000000;
  if (!SD.begin(SD_CS, SPI, hz)) {
    SD.end();
    hz = 4000000;   // some cards only work at lower speeds
    if (!SD.begin(SD_CS, SPI, hz)) { result("SD mount", false, "(see serial log: format or no card)"); return; }
  }
  static const char *types[] = {"none", "MMC", "SD", "SDHC/XC", "unknown"};
  char detail[64];
  snprintf(detail, sizeof detail, "%s %llu MB at %lu MHz", types[min((int)SD.cardType(), 4)],
           SD.cardSize() / (1024 * 1024), (unsigned long)(hz / 1000000));
  result("SD mount", true, detail);
  const char *msg = "workshop smoke test";
  File f = SD.open("/smoke.txt", FILE_WRITE);
  bool wrote = f && f.print(msg) == strlen(msg);
  if (f) f.close();
  f = SD.open("/smoke.txt");
  String back = f ? f.readString() : "";
  if (f) f.close();
  SD.remove("/smoke.txt");
  result("SD write+read", wrote && back == msg);
}

// ---- 2. mic and speaker --------------------------------------------------------

static int peak(const int16_t *p, size_t n) {
  int m = 0;
  for (size_t i = 0; i < n; i++) m = max(m, abs((int)p[i]));
  return m;
}

static void play(const int16_t *p, size_t n) {
  M5.Mic.end();
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);
  M5.Speaker.playRaw(p, n, RATE, false, 1, 0, true);
  while (M5.Speaker.isPlaying()) delay(10);
  M5.Speaker.end();
}

static void test_audio() {
  line(TFT_YELLOW, "Speak now: recording 2 s...");
  M5.Speaker.end();
  if (!M5.Mic.begin()) { result("Mic start", false); return; }
  delay(1200);  // the ES8311 codec returns zeros for about a second after power-up
  M5.Mic.record(audio, AUDIO_SAMPLES, RATE);
  while (M5.Mic.isRecording()) delay(10);
  M5.Mic.end();
  int pk = peak(audio, AUDIO_SAMPLES);
  char detail[32];
  snprintf(detail, sizeof detail, "peak %d", pk);
  result("Mic level", pk > 300, detail);
  line(TFT_WHITE, "Playing it back: listen...");
  play(audio, AUDIO_SAMPLES);
  line(TFT_WHITE, "  (heard yourself? speaker OK)");
}

// ---- 3. WiFi --------------------------------------------------------------------

static bool test_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(250);
  char detail[64];
  bool ok = WiFi.status() == WL_CONNECTED;
  if (ok) snprintf(detail, sizeof detail, "%s %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  else snprintf(detail, sizeof detail, "status %d (2.4 GHz? password?)", WiFi.status());
  result("WiFi", ok, detail);
  return ok;
}

// ---- 4. ElevenLabs ----------------------------------------------------------------

static WsClient ws;
static bool got_meta;
static char formats[64];
static int audio_msgs, big_msgs;
static size_t max_msg, audio_bytes;
static uint32_t last_audio_ms;

static void on_message(char *text, size_t len) {
  max_msg = max(max_msg, len);
  if (len > 15 * 1024) big_msgs++;
  JsonDocument doc;
  if (deserializeJson(doc, text, len)) return;
  const char *type = doc["type"] | "";
  if (!strcmp(type, "conversation_initiation_metadata")) {
    auto m = doc["conversation_initiation_metadata_event"];
    snprintf(formats, sizeof formats, "in %s out %s",
             m["user_input_audio_format"] | "?", m["agent_output_audio_format"] | "?");
    got_meta = true;
  } else if (!strcmp(type, "audio")) {
    audio_msgs++;
    last_audio_ms = millis();
    const char *b64 = doc["audio_event"]["audio_base_64"] | "";
    size_t room = AUDIO_SAMPLES * 2 - audio_bytes, n = 0;
    // Decode into a temporary, keep what fits in the 2 s buffer.
    size_t raw_len = strlen(b64) * 3 / 4 + 4;
    uint8_t *raw = (uint8_t *)malloc(raw_len);
    if (raw && !mbedtls_base64_decode(raw, raw_len, &n, (const uint8_t *)b64, strlen(b64))) {
      size_t take = min(n, room);
      memcpy((uint8_t *)audio + audio_bytes, raw, take);
      audio_bytes += take;
    }
    free(raw);
  } else if (!strcmp(type, "ping")) {
    char pong[64];
    int n = snprintf(pong, sizeof pong, "{\"type\":\"pong\",\"event_id\":%d}", doc["ping_event"]["event_id"].as<int>());
    ws.send_text(pong, n);
  }
}

static void test_elevenlabs() {
  if (!ELEVENLABS_AGENT_ID[0]) { result("ElevenLabs", false, "(no agent ID in secrets.h)"); return; }
  got_meta = false; audio_msgs = big_msgs = 0; max_msg = audio_bytes = 0; last_audio_ms = 0;
  char path[160];
  snprintf(path, sizeof path, "/v1/convai/conversation?agent_id=%s", ELEVENLABS_AGENT_ID);
  free_heap();
  uint32_t t = millis();
  bool ok = ws.connect("api.elevenlabs.io", path, ELEVENLABS_ROOT_CA);
  char detail[96];
  if (ok) snprintf(detail, sizeof detail, "%lu ms", (unsigned long)(millis() - t));
  else snprintf(detail, sizeof detail, "(%s)", ws.error());
  result("TLS + WebSocket", ok, detail);
  if (!ok) return;
  free_heap();

  const char *init = "{\"type\":\"conversation_initiation_client_data\"}";
  ws.send_text(init, strlen(init));
  t = millis();
  while (ws.is_open() && millis() - t < 12000) {
    ws.poll(on_message);
    if (audio_msgs && millis() - last_audio_ms > 1500) break;   // greeting finished
    delay(5);
  }
  bool still_open = ws.is_open();
  ws.close();

  result("Metadata", got_meta, formats);
  snprintf(detail, sizeof detail, "%d msgs, largest %u B", audio_msgs, (unsigned)max_msg);
  result("Agent audio", audio_msgs > 0, detail);
  snprintf(detail, sizeof detail, "%d over 15 KB%s", big_msgs, still_open ? "" : ", but connection dropped");
  result("Large messages", still_open || big_msgs == 0, detail);
  if (!still_open) line(TFT_RED, "  ws error: %s", ws.error());
  if (audio_bytes) {
    line(TFT_WHITE, "Playing the agent's greeting (first 2 s)...");
    play(audio, audio_bytes / 2);
  }
}

// ---- run -------------------------------------------------------------------------

static void run_all() {
  passes = fails = 0;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  line(TFT_CYAN, "Workshop smoke test: %s", M5.getBoard() == m5::board_t::board_M5CardputerADV ? "Cardputer Adv" : "unknown board");
  free_heap();
  test_sd();
  test_audio();
  if (test_wifi()) test_elevenlabs();
  WiFi.disconnect(true);
  line(fails ? TFT_RED : TFT_GREEN, "Done: %d passed, %d failed. G0 = rerun", passes, fails);
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setTextSize(1);
  M5.Display.setTextScroll(true);
  audio = (int16_t *)malloc(AUDIO_SAMPLES * 2);
  if (!audio) { Serial.println("no memory for audio buffer"); return; }
  delay(1500);  // time to open the serial monitor
  run_all();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasClicked()) run_all();
  delay(10);
}
