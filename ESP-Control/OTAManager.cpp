#include "OTAManager.h"
#include "ESPNowComm.h"
#include <WiFi.h>
#include <string.h>

OTAManager g_otaManager;

OTAManager::OTAManager() : m_active(false), m_startTime(0) {
  memset(m_ssid, 0, sizeof(m_ssid));
  memset(m_password, 0, sizeof(m_password));
  memset(m_hostname, 0, sizeof(m_hostname));
}

void OTAManager::begin(const char* ssid, const char* password, const char* hostname) {
  strncpy(m_ssid, ssid, sizeof(m_ssid) - 1);
  strncpy(m_password, password, sizeof(m_password) - 1);
  strncpy(m_hostname, hostname, sizeof(m_hostname) - 1);

  DEBUG_PRINTLN("[OTA] Manager initialisiert");
}

bool OTAManager::startOTA() {
  if (m_active) {
    return true;
  }

  DEBUG_PRINTLN("[OTA] Starte OTA-Modus...");

  // ESP-NOW stoppen
  g_espNow.stop();

  // WiFi aktivieren
  WiFi.mode(WIFI_STA);
  WiFi.begin(m_ssid, m_password);

  DEBUG_PRINT("[OTA] Verbinde mit WiFi");

  // Warten auf WiFi-Verbindung (max. 15 Sekunden)
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 150) {
    delay(100);
    DEBUG_PRINT(".");
    timeout++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("\n[OTA] WiFi-Verbindung fehlgeschlagen");
    return false;
  }

  DEBUG_PRINTLN();
  DEBUG_PRINT("[OTA] Verbunden, IP: ");
  DEBUG_PRINTLN(WiFi.localIP());

  // ArduinoOTA konfigurieren
  ArduinoOTA.setHostname(m_hostname);

  ArduinoOTA.onStart(onStart);
  ArduinoOTA.onEnd(onEnd);
  ArduinoOTA.onProgress(onProgress);
  ArduinoOTA.onError(onError);

  ArduinoOTA.begin();

  m_active = true;
  m_startTime = millis();

  DEBUG_PRINTLN("[OTA] OTA-Modus aktiv");

  return true;
}

void OTAManager::stopOTA() {
  if (!m_active) {
    return;
  }

  DEBUG_PRINTLN("[OTA] Stoppe OTA-Modus");

  ArduinoOTA.end();
  WiFi.disconnect();

  m_active = false;

  // ESP neu starten
  DEBUG_PRINTLN("[OTA] Neustart...");
  delay(1000);
  ESP.restart();
}

void OTAManager::handle() {
  if (!m_active) {
    return;
  }

  // ArduinoOTA handle
  ArduinoOTA.handle();

  // Timeout prüfen
  if (millis() - m_startTime > OTA_TIMEOUT_MS) {
    DEBUG_PRINTLN("[OTA] Timeout - beende OTA-Modus");
    stopOTA();
  }
}

void OTAManager::onStart() {
  String type = (ArduinoOTA.getCommand() == U_FLASH) ? "Sketch" : "Filesystem";
  DEBUG_PRINTLN("[OTA] Start: " + type);
}

void OTAManager::onEnd() {
  DEBUG_PRINTLN("\n[OTA] Ende");
}

void OTAManager::onProgress(unsigned int progress, unsigned int total) {
  DEBUG_PRINTF("[OTA] Fortschritt: %u%%\r", (progress / (total / 100)));
}

void OTAManager::onError(ota_error_t error) {
  DEBUG_PRINTF("[OTA] Fehler[%u]: ", error);

  switch (error) {
    case OTA_AUTH_ERROR:
      DEBUG_PRINTLN("Auth fehlgeschlagen");
      break;
    case OTA_BEGIN_ERROR:
      DEBUG_PRINTLN("Begin fehlgeschlagen");
      break;
    case OTA_CONNECT_ERROR:
      DEBUG_PRINTLN("Connect fehlgeschlagen");
      break;
    case OTA_RECEIVE_ERROR:
      DEBUG_PRINTLN("Receive fehlgeschlagen");
      break;
    case OTA_END_ERROR:
      DEBUG_PRINTLN("End fehlgeschlagen");
      break;
    default:
      DEBUG_PRINTLN("Unbekannter Fehler");
      break;
  }
}
