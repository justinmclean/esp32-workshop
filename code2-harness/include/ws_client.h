// Minimal secure WebSocket client (RFC 6455) for talking to ElevenLabs.
//
// Why not a library: the common Arduino WebSockets library drops any message
// over 15 KB, and ElevenLabs audio messages can be bigger. This client stores
// incoming messages in PSRAM, so size is not an issue.
//
// Covers what a client needs: TLS with a pinned root CA, the HTTP upgrade,
// masked outgoing text frames, fragmented incoming messages, ping/pong and
// close. It does not check Sec-WebSocket-Accept or support compression.
#pragma once
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <esp_heap_caps.h>
#include <esp_random.h>

typedef void (*WsMessageHandler)(char *text, size_t len);

class WsClient {
 public:
  bool connect(const char *host, const char *path, const char *root_ca) {
    close();
    tls_.setCACert(root_ca);
    if (!tls_.connect(host, 443)) { error_ = "TLS connect failed"; return false; }

    uint8_t raw[16];
    for (auto &b : raw) b = esp_random();
    char key[32];
    size_t key_len;
    mbedtls_base64_encode((uint8_t *)key, sizeof key, &key_len, raw, sizeof raw);
    key[key_len] = 0;

    tls_.printf("GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\n"
                "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n", path, host, key);

    char line[160];
    if (!read_line(line, sizeof line, 8000) || !strstr(line, " 101")) {
      error_ = "upgrade refused (check the agent ID and that the agent is public)";
      Serial.printf("WS handshake reply: %s\n", line);
      tls_.stop();
      return false;
    }
    do {  // skip the remaining response headers
      if (!read_line(line, sizeof line, 3000)) { error_ = "handshake timeout"; tls_.stop(); return false; }
    } while (line[0]);
    open_ = true;
    error_ = "";
    return true;
  }

  bool send_text(const char *text, size_t len) { return send_frame(0x1, (const uint8_t *)text, len); }

  // Read and handle any frames that have arrived. Call often from loop().
  void poll(WsMessageHandler on_text) {
    for (int frames = 0; open_ && frames < 8 && tls_.available() >= 2; frames++) read_frame(on_text);
    if (open_ && !tls_.connected()) { open_ = false; error_ = "connection closed by server"; }
  }

  void close() {
    if (open_) send_frame(0x8, nullptr, 0);
    open_ = false;
    tls_.stop();
  }

  bool is_open() const { return open_; }
  const char *error() const { return error_; }

 private:
  WiFiClientSecure tls_;
  bool open_ = false;
  const char *error_ = "";
  uint8_t *msg_ = nullptr;
  size_t msg_cap_ = 0, msg_len_ = 0;
  uint8_t msg_op_ = 0;

  bool read_exact(uint8_t *dst, size_t n, uint32_t timeout_ms) {
    uint32_t start = millis();
    size_t got = 0;
    while (got < n) {
      int avail = tls_.available();
      if (avail > 0) {
        int r = tls_.read(dst + got, min((size_t)avail, n - got));
        if (r > 0) { got += r; start = millis(); continue; }
      }
      if (!tls_.connected() || millis() - start > timeout_ms) return false;
      delay(1);
    }
    return true;
  }

  bool read_line(char *out, size_t cap, uint32_t timeout_ms) {
    size_t n = 0;
    uint8_t c;
    while (read_exact(&c, 1, timeout_ms)) {
      if (c == '\n') { out[n] = 0; return true; }
      if (c != '\r' && n + 1 < cap) out[n++] = c;
    }
    out[n] = 0;
    return false;
  }

  bool send_frame(uint8_t opcode, const uint8_t *data, size_t len) {
    if (!tls_.connected()) return false;
    uint8_t hdr[14];
    size_t h = 0;
    hdr[h++] = 0x80 | opcode;  // FIN + opcode
    if (len < 126) {
      hdr[h++] = 0x80 | len;   // mask bit: clients must mask
    } else if (len < 65536) {
      hdr[h++] = 0x80 | 126;
      hdr[h++] = len >> 8; hdr[h++] = len;
    } else {
      hdr[h++] = 0x80 | 127;
      for (int i = 7; i >= 0; i--) hdr[h++] = (uint64_t)len >> (8 * i);
    }
    uint8_t mask[4];
    for (auto &m : mask) m = esp_random();
    memcpy(hdr + h, mask, 4);
    h += 4;
    if (tls_.write(hdr, h) != h) return false;

    uint8_t block[512];
    for (size_t off = 0; off < len; off += sizeof block) {
      size_t n = min(sizeof block, len - off);
      for (size_t i = 0; i < n; i++) block[i] = data[off + i] ^ mask[(off + i) & 3];
      if (tls_.write(block, n) != n) return false;
    }
    return true;
  }

  bool reserve(size_t need) {
    if (need <= msg_cap_) return true;
    size_t cap = max(need, msg_cap_ * 2);
    uint8_t *p = (uint8_t *)heap_caps_realloc(msg_, cap, MALLOC_CAP_SPIRAM);
    if (!p) return false;
    msg_ = p;
    msg_cap_ = cap;
    return true;
  }

  void read_frame(WsMessageHandler on_text) {
    uint8_t h[2];
    if (!read_exact(h, 2, 2000)) { open_ = false; error_ = "read failed"; return; }
    bool fin = h[0] & 0x80;
    uint8_t op = h[0] & 0x0F;
    bool masked = h[1] & 0x80;
    uint64_t len = h[1] & 0x7F;
    uint8_t ext[8];
    if (len == 126) { read_exact(ext, 2, 2000); len = (ext[0] << 8) | ext[1]; }
    else if (len == 127) { read_exact(ext, 8, 2000); len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | ext[i]; }
    uint8_t mask[4] = {0};
    if (masked) read_exact(mask, 4, 2000);

    if (op >= 0x8) {  // control frame: at most 125 bytes
      uint8_t buf[125];
      size_t n = min((size_t)len, sizeof buf);
      read_exact(buf, n, 2000);
      if (masked) for (size_t i = 0; i < n; i++) buf[i] ^= mask[i & 3];
      if (op == 0x9) send_frame(0xA, buf, n);                     // ping -> pong
      else if (op == 0x8) { close(); error_ = "server closed the conversation"; }
      return;
    }

    if (op != 0x0) { msg_op_ = op; msg_len_ = 0; }                // new message
    if (!reserve(msg_len_ + len + 1) || !read_exact(msg_ + msg_len_, len, 5000)) {
      open_ = false; error_ = "message too big or truncated"; return;
    }
    if (masked) for (size_t i = 0; i < len; i++) msg_[msg_len_ + i] ^= mask[i & 3];
    msg_len_ += len;
    if (fin && msg_op_ == 0x1) {
      msg_[msg_len_] = 0;
      on_text((char *)msg_, msg_len_);
    }
  }
};
