#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// ZENTRALE TYP-DEFINITIONEN FÜR ESP-CONTROL
// ============================================================================

// Rollen-Definition
enum DeviceRole {
  ROLE_ACTOR = 0,
  ROLE_CONTROLLER = 1
};

// Indikator-Typen
enum IndicatorType {
  INDICATOR_NONE = 0,
  INDICATOR_LED = 1,
  INDICATOR_PIXEL = 2
};

// Indikator-Konfiguration
struct IndicatorConfig {
  bool enabled;
  IndicatorType type;
  uint8_t pin;
  bool activeLow;  // nur für LED relevant
};

// Taster-Konfiguration
struct ButtonConfig {
  bool enabled;
  uint8_t pin;
  bool activeLow;
};

// Enable-Pin-Konfiguration (für Actor)
struct EnablePinConfig {
  bool enabled;
  uint8_t pin;
  bool activeLow;
};

// Display-Typen
enum DisplayType {
  DISPLAY_NONE = 0,
  DISPLAY_SSD1306 = 1,
  DISPLAY_BUILTIN = 2
};

// Display-Konfiguration
struct DisplayConfig {
  DisplayType type;
  uint8_t sda_pin;  // nur für SSD1306
  uint8_t scl_pin;  // nur für SSD1306
};

// System-Zustände
enum SystemState {
  STATE_INIT = 0,
  STATE_IDLE = 1,
  STATE_LINKED = 2,
  STATE_OTA_PREPARE = 3,
  STATE_OTA_ACTIVE = 4,
  STATE_ERROR = 5
};

// ESP-NOW Nachrichten-Typen
enum MessageType {
  MSG_BEACON = 0x01,           // Actor sendet Beacon
  MSG_BEACON_ACK = 0x02,       // Controller bestätigt Beacon
  MSG_CHALLENGE = 0x10,        // Security: Challenge
  MSG_RESPONSE = 0x11,         // Security: Response
  MSG_RELAY_TOGGLE = 0x20,     // Relais umschalten
  MSG_RELAY_STATE = 0x21,      // Relais-Status-Report
  MSG_OTA_PREPARE = 0x30,      // OTA vorbereiten
  MSG_OTA_ACK = 0x31,          // OTA bereit
  MSG_OTA_ABORT = 0x32,        // OTA abbrechen
  MSG_PING = 0x40,             // Ping
  MSG_PONG = 0x41              // Pong
};

// ESP-NOW Nachrichten-Header
struct __attribute__((packed)) MessageHeader {
  uint8_t msgType;           // MessageType
  uint8_t controllerId;      // Controller-ID (0-9)
  uint8_t actorId;           // Actor-ID (0-9)
  uint32_t timestamp;        // Milliseconds seit Boot
  uint32_t nonce;            // Zufallswert gegen Replay
};

// Beacon-Nachricht (Actor -> Broadcast)
struct __attribute__((packed)) BeaconMessage {
  MessageHeader header;
  uint8_t actorId;
  bool relayState;
  int8_t rssi;
};

// Challenge-Nachricht (Controller -> Actor)
struct __attribute__((packed)) ChallengeMessage {
  MessageHeader header;
  uint8_t challenge[16];     // Zufalls-Challenge
};

// Response-Nachricht (Actor -> Controller)
struct __attribute__((packed)) ResponseMessage {
  MessageHeader header;
  uint8_t response[32];      // HMAC über Challenge + Shared Secret
};

// Relay-Toggle-Nachricht (Controller -> Actor)
struct __attribute__((packed)) RelayToggleMessage {
  MessageHeader header;
  uint8_t hmac[32];          // HMAC über Header
};

// Relay-State-Nachricht (Actor -> Controller)
struct __attribute__((packed)) RelayStateMessage {
  MessageHeader header;
  bool relayState;
};

// OTA-Prepare-Nachricht (Controller -> Actor)
struct __attribute__((packed)) OTAPrepareMessage {
  MessageHeader header;
  uint8_t hmac[32];          // HMAC über Header
};

// OTA-ACK-Nachricht (Actor -> Controller)
struct __attribute__((packed)) OTAAckMessage {
  MessageHeader header;
};

// Maximale Nachrichtengröße für ESP-NOW
#define MAX_ESPNOW_MESSAGE_SIZE 250

// Timing-Konstanten
#define BEACON_INTERVAL_MS 2000        // Beacon alle 2 Sekunden
#define DISCOVERY_TIMEOUT_MS 5000      // Actor gilt als "weg" nach 5s
#define CHALLENGE_TIMEOUT_MS 1000      // Challenge-Response Timeout
#define OTA_TIMEOUT_MS 300000          // OTA-Timeout: 5 Minuten
#define LONG_PRESS_MS 2000             // Long-Press Erkennung
#define DEBOUNCE_MS 50                 // Taster-Entprellung

// Logging
extern bool g_debugEnabled;

#define DEBUG_PRINT(x) if(g_debugEnabled) Serial.print(x)
#define DEBUG_PRINTLN(x) if(g_debugEnabled) Serial.println(x)
#define DEBUG_PRINTF(...) if(g_debugEnabled) Serial.printf(__VA_ARGS__)

#endif // CONFIG_H
