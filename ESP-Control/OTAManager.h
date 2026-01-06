#ifndef OTAMANAGER_H
#define OTAMANAGER_H

#include "Config.h"
#include <ArduinoOTA.h>

// ============================================================================
// OTA-MANAGER FÜR KOORDINIERTEN OTA-MODUS
// ============================================================================

class OTAManager {
public:
  OTAManager();

  // Initialisierung (WiFi-Credentials aus ESP_Code_Conf.h)
  void begin(const char* ssid, const char* password, const char* hostname);

  // OTA-Modus starten
  bool startOTA();

  // OTA-Modus stoppen
  void stopOTA();

  // OTA-Update-Loop (regelmäßig aufrufen im OTA-Modus)
  void handle();

  // Status
  bool isActive() const { return m_active; }
  uint32_t getStartTime() const { return m_startTime; }

private:
  bool m_active;
  uint32_t m_startTime;

  char m_ssid[64];
  char m_password[64];
  char m_hostname[64];

  // OTA-Callbacks
  static void onStart();
  static void onEnd();
  static void onProgress(unsigned int progress, unsigned int total);
  static void onError(ota_error_t error);
};

extern OTAManager g_otaManager;

#endif // OTAMANAGER_H
