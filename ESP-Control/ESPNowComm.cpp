#include "ESPNowComm.h"

ESPNowComm g_espNow;
ESPNowComm* ESPNowComm::s_instance = nullptr;

ESPNowComm::ESPNowComm() : m_initialized(false), m_receiveCallback(nullptr) {
  s_instance = this;
}

bool ESPNowComm::begin() {
  if (m_initialized) {
    return true;
  }

  // WiFi in Station-Mode setzen
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  DEBUG_PRINT("[ESPNow] MAC-Adresse: ");
  DEBUG_PRINTLN(WiFi.macAddress());

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    DEBUG_PRINTLN("[ESPNow] Fehler bei Initialisierung");
    return false;
  }

  // Callbacks registrieren
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  m_initialized = true;
  DEBUG_PRINTLN("[ESPNow] Initialisiert");

  return true;
}

void ESPNowComm::stop() {
  if (!m_initialized) {
    return;
  }

  esp_now_deinit();
  m_initialized = false;

  DEBUG_PRINTLN("[ESPNow] Gestoppt");
}

bool ESPNowComm::sendBroadcast(const uint8_t* data, size_t len) {
  if (!m_initialized) {
    return false;
  }

  uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Broadcast-Peer muss registriert sein
  if (!isPeerRegistered(broadcastMAC)) {
    addPeer(broadcastMAC);
  }

  esp_err_t result = esp_now_send(broadcastMAC, data, len);
  return (result == ESP_OK);
}

bool ESPNowComm::sendUnicast(const uint8_t* mac, const uint8_t* data, size_t len) {
  if (!m_initialized) {
    return false;
  }

  // Peer muss registriert sein
  if (!isPeerRegistered(mac)) {
    if (!addPeer(mac)) {
      return false;
    }
  }

  esp_err_t result = esp_now_send(mac, data, len);
  return (result == ESP_OK);
}

bool ESPNowComm::addPeer(const uint8_t* mac) {
  if (!m_initialized) {
    return false;
  }

  if (isPeerRegistered(mac)) {
    return true;  // bereits registriert
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;  // aktueller Channel
  peerInfo.encrypt = false;  // keine Verschlüsselung (wir machen das selbst)

  esp_err_t result = esp_now_add_peer(&peerInfo);

  if (result == ESP_OK) {
    DEBUG_PRINTF("[ESPNow] Peer hinzugefügt: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
  }

  DEBUG_PRINTF("[ESPNow] Fehler beim Hinzufügen von Peer: %d\n", result);
  return false;
}

bool ESPNowComm::removePeer(const uint8_t* mac) {
  if (!m_initialized) {
    return false;
  }

  esp_err_t result = esp_now_del_peer(mac);
  return (result == ESP_OK);
}

bool ESPNowComm::isPeerRegistered(const uint8_t* mac) {
  return esp_now_is_peer_exist(mac);
}

void ESPNowComm::setReceiveCallback(MessageCallback callback) {
  m_receiveCallback = callback;
}

void ESPNowComm::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  // Optional: Sende-Status loggen
  if (status != ESP_NOW_SEND_SUCCESS) {
    DEBUG_PRINTLN("[ESPNow] Fehler beim Senden");
  }
}

void ESPNowComm::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (s_instance && s_instance->m_receiveCallback) {
    s_instance->m_receiveCallback(info->src_addr, data, len);
  }
}
