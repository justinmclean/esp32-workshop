// Copy this file to secrets.h (same folder) and fill it in.
// secrets.h is gitignored: never commit it, never paste it into an issue or PR.
// Every workshop project picks it up from the repo root.
#pragma once

// WiFi. Must be a 2.4 GHz network: the ESP32 cannot see 5 GHz.
// Phone hotspots: iPhone "Maximise Compatibility" on, Android band 2.4 GHz.
#define WIFI_SSID     "your-hotspot-name"
#define WIFI_PASSWORD "your-hotspot-password"

// ElevenLabs (Code 3). API key from elevenlabs.io > Developers > API Keys.
#define ELEVENLABS_API_KEY  ""
// Agent ID of a public ElevenLabs agent, for press to chat.
#define ELEVENLABS_AGENT_ID ""
