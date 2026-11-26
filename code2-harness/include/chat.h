// Code 3 reference: press to chat with an ElevenLabs agent.
//
// Flow:
//   Connect  open wss://api.elevenlabs.io/v1/convai/conversation?agent_id=...
//            The server replies with conversation_initiation_metadata, which
//            says which audio formats it expects and sends.
//   Talk     while the button is held, stream 100 ms mic chunks as
//            {"user_audio_chunk": "<base64 PCM>"}. On release, stream about a
//            second of silence so the server's voice detection ends the turn.
//   Listen   collect the agent's audio events. When they stop for 700 ms, play
//            the whole reply (the CoreS3 mic and speaker cannot run together).
//   Tools    the agent can call a client tool, show_message, which puts text
//            on the screen. This is the "reverse tools" pattern.
//
// Uses a public agent (agent ID only, no API key on the device). Leaving the
// screen hangs up so a forgotten session does not burn credits.
#pragma once
#include <ArduinoJson.h>
#include "ui.h"
#include "ws_client.h"
#include "ca_certs.h"
#include "secrets_check.h"

#define EL_HOST "api.elevenlabs.io"
#define CHUNK_MS 100
#define SILENCE_CHUNKS 12          // 1.2 s of silence after you let go
#define REPLY_GAP_MS 700           // agent audio quiet this long = reply done
#define MAX_REPLY_SECONDS 40       // 1.3 MB per buffer at 16 kHz; longer replies are cut

enum ChatState { CHAT_OFF, CHAT_STARTING, CHAT_READY, CHAT_TALKING, CHAT_WAITING };
static ChatState chat_state = CHAT_OFF;
static WsClient ws;

static uint32_t in_rate = 16000, out_rate = 16000;   // from the server's metadata

// Mic: two buffers alternate. The mic task hands back each full buffer
// through a queue; loop() sends it and queues it again.
static int16_t *mic_buf[2];
static size_t mic_samples = 0;
static QueueHandle_t mic_done_q;
static int silence_left = 0;
static uint32_t silence_next_ms = 0;

// Agent reply audio: incoming audio goes into one buffer while the other
// plays, so the speaker never reads memory that is being reallocated.
struct Pcm { int16_t *data = nullptr; size_t len = 0, cap = 0; };
static Pcm reply_buf[2];
static int reply_in = 0;
static uint32_t last_audio_ms = 0;

static char you_said[200] = "", agent_said[400] = "", tool_msg[120] = "";

// Scratch for base64 + JSON of one outgoing mic chunk.
static char *tx_json = nullptr;
static size_t tx_cap = 0;

static Button chat_conn_btn = {8,   HEADER_H + 8, 148, 40, "Connect", TFT_DARKGREEN, true};
static Button chat_hang_btn = {164, HEADER_H + 8, 148, 40, "Hang up", TFT_MAROON,    false};
static Button chat_talk_btn = {96,  STATUS_Y - 40, 216, 34, "Hold to talk", TFT_PURPLE, false};
static Button chat_back_btn = {8,   STATUS_Y - 40, 80, 34, "Back", TFT_DARKGREY, true};
#define TEXT_Y (HEADER_H + 56)
#define TEXT_H (STATUS_Y - 44 - TEXT_Y)

// ---- drawing ----------------------------------------------------------------

static void chat_draw_text() {
  auto &d = M5.Display;
  d.fillRect(0, TEXT_Y, SCR_W, TEXT_H, TFT_BLACK);
  d.setClipRect(0, TEXT_Y, SCR_W, TEXT_H);
  d.setTextSize(1);
  d.setTextWrap(true);
  d.setCursor(0, TEXT_Y + 2);
  if (tool_msg[0]) { d.setTextColor(TFT_CYAN, TFT_BLACK);   d.printf("[agent shows] %s\n\n", tool_msg); }
  if (you_said[0]) { d.setTextColor(TFT_YELLOW, TFT_BLACK); d.printf("You: %s\n\n", you_said); }
  if (agent_said[0]) { d.setTextColor(TFT_WHITE, TFT_BLACK); d.printf("Agent: %s", agent_said); }
  d.setTextWrap(false);
  d.clearClipRect();
}

static void chat_draw_buttons() {
  bool on = chat_state != CHAT_OFF;
  chat_conn_btn.enabled = !on && WiFi.status() == WL_CONNECTED && ELEVENLABS_AGENT_ID[0];
  chat_hang_btn.enabled = on;
  chat_talk_btn.enabled = chat_state == CHAT_READY;
  draw_button(chat_conn_btn);
  draw_button(chat_hang_btn);
  draw_button(chat_talk_btn, chat_state == CHAT_TALKING);
  draw_button(chat_back_btn);
}

static void chat_draw() {
  draw_header("Chat", WiFi.status() == WL_CONNECTED);
  clear_body();
  chat_draw_buttons();
  chat_draw_text();
  if (WiFi.status() != WL_CONNECTED) status("Connect to WiFi first (WiFi screen).", TFT_ORANGE);
  else if (!ELEVENLABS_AGENT_ID[0]) status("Set ELEVENLABS_AGENT_ID in secrets.h.", TFT_ORANGE);
  else if (chat_state == CHAT_OFF) status("Tap Connect to start a conversation.");
}

static void set_state(ChatState s, const char *msg = nullptr, uint16_t colour = TFT_LIGHTGREY) {
  chat_state = s;
  chat_draw_buttons();
  if (msg) status(msg, colour);
}

// Copy agent text without voice tags such as "[happy] " that ElevenLabs
// includes for expressive voices: they are for the voice, not the screen.
static void copy_without_tags(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  for (const char *p = src; *p && n + 1 < cap; p++) {
    if (*p == '[') {
      const char *end = strchr(p, ']');
      if (end) { p = end; if (p[1] == ' ') p++; continue; }
    }
    dst[n++] = *p;
  }
  dst[n] = 0;
}

// ---- audio helpers ------------------------------------------------------------

static uint32_t rate_from_format(const char *fmt) {   // "pcm_16000" -> 16000
  if (!fmt || strncmp(fmt, "pcm_", 4)) return 0;
  return atoi(fmt + 4);
}

static bool pcm_append(Pcm &p, const uint8_t *bytes, size_t n) {
  size_t samples = n / 2;
  size_t max_samples = (size_t)out_rate * MAX_REPLY_SECONDS;
  if (p.len + samples > max_samples) return false;
  if (p.len + samples > p.cap) {
    size_t cap = min(max_samples, max(p.len + samples, p.cap * 2 + (size_t)out_rate));
    auto *d = (int16_t *)heap_caps_realloc(p.data, cap * 2, MALLOC_CAP_SPIRAM);
    if (!d) return false;
    p.data = d;
    p.cap = cap;
  }
  memcpy(p.data + p.len, bytes, samples * 2);
  p.len += samples;
  return true;
}

// Base64-encode one PCM chunk into {"user_audio_chunk":"..."} and send it.
static void send_audio(const int16_t *pcm, size_t samples) {
  size_t bytes = samples * 2;
  size_t need = 4 * ((bytes + 2) / 3) + 32;
  if (need > tx_cap) {
    tx_json = (char *)heap_caps_realloc(tx_json, need, MALLOC_CAP_SPIRAM);
    tx_cap = tx_json ? need : 0;
    if (!tx_json) return;
  }
  const char *pre = "{\"user_audio_chunk\":\"";
  size_t p = strlen(pre), out;
  memcpy(tx_json, pre, p);
  mbedtls_base64_encode((uint8_t *)tx_json + p, tx_cap - p, &out, (const uint8_t *)pcm, bytes);
  p += out;
  tx_json[p++] = '"';
  tx_json[p++] = '}';
  ws.send_text(tx_json, p);
}

static void send_json(JsonDocument &doc) {
  String s;
  serializeJson(doc, s);
  ws.send_text(s.c_str(), s.length());
}

// ---- incoming messages --------------------------------------------------------

static void handle_tool_call(JsonObject call) {
  const char *name = call["tool_name"] | "";
  const char *id = call["tool_call_id"] | "";
  JsonDocument reply;
  reply["type"] = "client_tool_result";
  reply["tool_call_id"] = id;
  if (!strcmp(name, "show_message")) {
    strlcpy(tool_msg, call["parameters"]["text"] | "", sizeof tool_msg);
    chat_draw_text();
    reply["result"] = "Shown on the device screen.";
    reply["is_error"] = false;
  } else {
    reply["result"] = String("Unknown tool: ") + name;
    reply["is_error"] = true;
  }
  Serial.printf("tool call %s -> %s\n", name, reply["result"].as<const char *>());
  if (call["expects_response"] | true) send_json(reply);
}

static void on_message(char *text, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, text, len)) { Serial.println("bad JSON from server"); return; }
  const char *type = doc["type"] | "";

  if (!strcmp(type, "audio")) {
    const char *b64 = doc["audio_event"]["audio_base_64"] | "";
    size_t b64_len = strlen(b64), n;
    static uint8_t *raw = nullptr;
    static size_t raw_cap = 0;
    size_t need = b64_len * 3 / 4 + 4;
    if (need > raw_cap) {
      raw = (uint8_t *)heap_caps_realloc(raw, need, MALLOC_CAP_SPIRAM);
      raw_cap = raw ? need : 0;
    }
    if (raw && !mbedtls_base64_decode(raw, raw_cap, &n, (const uint8_t *)b64, b64_len))
      pcm_append(reply_buf[reply_in], raw, n);
    last_audio_ms = millis();
  } else if (!strcmp(type, "ping")) {
    JsonDocument pong;
    pong["type"] = "pong";
    pong["event_id"] = doc["ping_event"]["event_id"];
    send_json(pong);
  } else if (!strcmp(type, "conversation_initiation_metadata")) {
    auto meta = doc["conversation_initiation_metadata_event"];
    in_rate = rate_from_format(meta["user_input_audio_format"]);
    out_rate = rate_from_format(meta["agent_output_audio_format"]);
    Serial.printf("formats: in %s, out %s\n", meta["user_input_audio_format"].as<const char *>(),
                  meta["agent_output_audio_format"].as<const char *>());
    if (!in_rate || !out_rate) {
      ws.close();
      set_state(CHAT_OFF, "Agent audio must be PCM, not ulaw. Change it in the agent's settings.", TFT_ORANGE);
      return;
    }
    set_state(CHAT_READY, "Connected. Hold the button and talk.", TFT_GREEN);
  } else if (!strcmp(type, "user_transcript")) {
    strlcpy(you_said, doc["user_transcription_event"]["user_transcript"] | "", sizeof you_said);
    chat_draw_text();
  } else if (!strcmp(type, "agent_response")) {
    copy_without_tags(agent_said, sizeof agent_said, doc["agent_response_event"]["agent_response"] | "");
    chat_draw_text();
  } else if (!strcmp(type, "client_tool_call")) {
    handle_tool_call(doc["client_tool_call"]);
  } else if (!strcmp(type, "client_error")) {
    char line[96];
    snprintf(line, sizeof line, "Server error: %s", doc["error_event"]["message"] | "unknown");
    status(line, TFT_RED);
  }
}

// ---- conversation -------------------------------------------------------------

static void mic_released(void *, void *data, size_t) {
  xQueueSend(mic_done_q, &data, 0);   // runs in the mic task: hand off, never block
}

static void chat_connect() {
  if (!mic_done_q) mic_done_q = xQueueCreate(4, sizeof(void *));
  you_said[0] = agent_said[0] = tool_msg[0] = 0;
  chat_draw_text();
  set_state(CHAT_STARTING, "Connecting to ElevenLabs...");
  char path[160];
  snprintf(path, sizeof path, "/v1/convai/conversation?agent_id=%s", ELEVENLABS_AGENT_ID);
  if (!ws.connect(EL_HOST, path, ELEVENLABS_ROOT_CA)) {
    char line[96];
    snprintf(line, sizeof line, "Could not connect: %s", ws.error());
    set_state(CHAT_OFF, line, TFT_RED);
    return;
  }
  JsonDocument init;
  init["type"] = "conversation_initiation_client_data";
  send_json(init);
  status("Waiting for the agent...");
}

static void chat_hangup(const char *why = "Hung up.") {
  if (chat_state == CHAT_TALKING) M5.Mic.end();
  M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
  M5.Speaker.stop();
  ws.close();
  for (auto &p : reply_buf) { free(p.data); p = Pcm(); }
  for (auto &b : mic_buf) { free(b); b = nullptr; }
  set_state(CHAT_OFF, why);
}

static void start_talking() {
  mic_samples = in_rate * CHUNK_MS / 1000;
  for (auto &b : mic_buf)
    if (!b) b = (int16_t *)heap_caps_malloc(mic_samples * 2, MALLOC_CAP_SPIRAM);
  if (!mic_buf[0] || !mic_buf[1]) { status("Out of PSRAM.", TFT_RED); return; }
  M5.Speaker.end();
  xQueueReset(mic_done_q);
  M5.Mic.setBufferReleaseCallback(nullptr, mic_released);  // set before begin()
  M5.Mic.begin();
  M5.Mic.record(mic_buf[0], mic_samples, in_rate);
  M5.Mic.record(mic_buf[1], mic_samples, in_rate);
  set_state(CHAT_TALKING, "Listening... let go when you finish.", TFT_RED);
}

static void stop_talking() {
  M5.Mic.end();
  M5.Mic.setBufferReleaseCallback(nullptr, nullptr);       // safe after end()
  void *p;
  while (xQueueReceive(mic_done_q, &p, 0)) send_audio((int16_t *)p, mic_samples);
  silence_left = SILENCE_CHUNKS;
  silence_next_ms = millis();
  set_state(CHAT_WAITING, "Thinking...");
}

static void play_reply() {
  Pcm &p = reply_buf[reply_in];
  reply_in ^= 1;                  // new audio now lands in the other buffer
  reply_buf[reply_in].len = 0;
  status("Agent speaking. Tap to stop.", TFT_GREEN);
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);
  M5.Speaker.playRaw(p.data, p.len, out_rate, false, 1, 0, true);
  while (M5.Speaker.isPlaying()) {
    ws.poll(on_message);          // keep answering pings while we play
    M5.update();
    if (M5.Touch.getDetail().wasClicked()) M5.Speaker.stop();
    delay(5);
  }
  p.len = 0;
  if (ws.is_open()) set_state(CHAT_READY, "Your turn. Hold the button and talk.", TFT_GREEN);
}

// Called every loop() while the Chat screen is showing.
static void chat_loop() {
  if (chat_state == CHAT_OFF) return;
  ws.poll(on_message);
  if (chat_state == CHAT_OFF) return;   // a message handler already hung up
  if (!ws.is_open()) {
    char line[96];
    snprintf(line, sizeof line, "Disconnected: %s", ws.error());
    chat_hangup(line);
    return;
  }

  if (chat_state == CHAT_TALKING) {
    void *p;
    while (xQueueReceive(mic_done_q, &p, 0)) {
      send_audio((int16_t *)p, mic_samples);
      M5.Mic.record((int16_t *)p, mic_samples, in_rate);
    }
    if (!M5.Touch.getDetail().isPressed()) stop_talking();
  } else if (chat_state == CHAT_WAITING && silence_left > 0 && millis() >= silence_next_ms) {
    static int16_t zeros[4800] = {0};               // up to 100 ms at 48 kHz
    send_audio(zeros, min(mic_samples, (size_t)4800));
    silence_left--;
    silence_next_ms += CHUNK_MS;
  }

  if (chat_state != CHAT_TALKING && reply_buf[reply_in].len &&
      millis() - last_audio_ms > REPLY_GAP_MS)
    play_reply();
}

// Returns false when the user taps Back.
static bool chat_handle() {
  auto t = M5.Touch.getDetail();
  if (t.wasPressed() && button_hit(chat_talk_btn, t.x, t.y)) { start_talking(); return true; }
  if (!t.wasClicked()) return true;
  if (button_hit(chat_back_btn, t.x, t.y)) { if (chat_state != CHAT_OFF) chat_hangup(); return false; }
  if (button_hit(chat_conn_btn, t.x, t.y)) chat_connect();
  else if (button_hit(chat_hang_btn, t.x, t.y)) chat_hangup();
  return true;
}
