#ifndef ESPNOWCOMM_H
#define ESPNOWCOMM_H

#include "Config.h"
#include <esp_now.h>
#include <WiFi.h>

// ============================================================================
// ESP-NOW KOMMUNIKATIONS-LAYER
// ============================================================================

// Callback-Typen für empfangene Nachrichten
typedef void (*MessageCallback)(const uint8_t* mac, const uint8_t* data, int len);

class ESPNowComm {
public:
  ESPNowComm();

  // Initialisierung
  bool begin();

  // Stoppen (für OTA-Modus)
  void stop();

  // Senden
  bool sendBroadcast(const uint8_t* data, size_t len);
  bool sendUnicast(const uint8_t* mac, const uint8_t* data, size_t len);

  // Peer-Management
  bool addPeer(const uint8_t* mac);
  bool removePeer(const uint8_t* mac);
  bool isPeerRegistered(const uint8_t* mac);

  // Callback registrieren
  void setReceiveCallback(MessageCallback callback);

  // Status
  bool isInitialized() const { return m_initialized; }

  // ESP-NOW Callbacks (statisch)
  static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);
  static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);

private:
  bool m_initialized;
  MessageCallback m_receiveCallback;

  static ESPNowComm* s_instance;
};

extern ESPNowComm g_espNow;

#endif // ESPNOWCOMM_H
