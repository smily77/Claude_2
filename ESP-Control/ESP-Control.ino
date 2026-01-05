// ============================================================================
// ESP-CONTROL - ESP32-BASIERTES CONTROLLER/ACTOR-SYSTEM MIT ESP-NOW
// ============================================================================
//
// Dieses System ermöglicht die Steuerung von Relais über ESP-NOW.
// Ein Controller kann bis zu 3 Actoren steuern.
// Ein Actor kann von bis zu 10 Controllern gesteuert werden.
//
// Die Rolle (Actor/Controller) wird durch externe Konfigurationsdateien
// festgelegt, nicht im Code selbst.
//
// Erforderliche externe Dateien (nicht im Repo):
// - ESP_Code_Conf.h  : Gerätekonfiguration (Rolle, Pins, etc.)
// - ESP_Code_Key.h   : Sicherheitsschlüssel und MAC-Adressen
//
// ============================================================================

#include "Config.h"
#include "Security.h"
#include "ESPNowComm.h"
#include "Discovery.h"
#include "Actor.h"
#include "Controller.h"
#include "OTAManager.h"

// Externe Konfiguration einbinden
#include "ESP_Code_Conf.h"
#include "ESP_Code_Key.h"

// ============================================================================
// GLOBALE VARIABLEN
// ============================================================================

bool g_debugEnabled = true;  // Logging aktivieren/deaktivieren
SystemState g_systemState = STATE_INIT;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Serielle Konsole initialisieren
  Serial.begin(115200);
  delay(500);

  DEBUG_PRINTLN("\n\n========================================");
  DEBUG_PRINTLN("ESP-CONTROL");
  DEBUG_PRINTLN("========================================");

  // Rolle ausgeben
#if DEVICE_ROLE == ROLE_ACTOR
  DEBUG_PRINTLN("Rolle: ACTOR");
  DEBUG_PRINTF("Actor-ID: %d\n", ACTOR_ID);
#elif DEVICE_ROLE == ROLE_CONTROLLER
  DEBUG_PRINTLN("Rolle: CONTROLLER");
  DEBUG_PRINTF("Controller-ID: %d\n", CONTROLLER_ID);
#else
  #error "DEVICE_ROLE muss in ESP_Code_Conf.h definiert sein (ROLE_ACTOR oder ROLE_CONTROLLER)"
#endif

  DEBUG_PRINTLN("========================================\n");

  // Security initialisieren
  g_security.begin(SHARED_SECRET);

#if DEVICE_ROLE == ROLE_ACTOR
  // Session-Key generieren (Actor)
  g_security.generateSessionKey();
#endif

  // ESP-NOW initialisieren
  if (!g_espNow.begin()) {
    DEBUG_PRINTLN("[FEHLER] ESP-NOW Initialisierung fehlgeschlagen");
    g_systemState = STATE_ERROR;
    return;
  }

  // ESP-NOW Callback registrieren
  g_espNow.setReceiveCallback(onESPNowReceive);

  // OTA-Manager initialisieren
#ifdef OTA_WIFI_SSID
  g_otaManager.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD, OTA_HOSTNAME);
#else
  DEBUG_PRINTLN("[WARNUNG] OTA nicht konfiguriert (OTA_WIFI_SSID fehlt)");
#endif

  // Rollen-spezifische Initialisierung
#if DEVICE_ROLE == ROLE_ACTOR
  initializeActor();
#elif DEVICE_ROLE == ROLE_CONTROLLER
  initializeController();
#endif

  g_systemState = STATE_IDLE;

  DEBUG_PRINTLN("[Setup] Initialisierung abgeschlossen\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // System-State-Machine
  switch (g_systemState) {
    case STATE_IDLE:
    case STATE_LINKED:
      // Normale Betriebslogik
#if DEVICE_ROLE == ROLE_ACTOR
      g_actor.update();
#elif DEVICE_ROLE == ROLE_CONTROLLER
      g_controller.update();
#endif
      break;

    case STATE_OTA_PREPARE:
      // OTA vorbereiten
      DEBUG_PRINTLN("[State] OTA_PREPARE -> OTA_ACTIVE");
      delay(500);  // Kurze Pause für laufende Kommunikation

      if (g_otaManager.startOTA()) {
        g_systemState = STATE_OTA_ACTIVE;
      } else {
        DEBUG_PRINTLN("[FEHLER] OTA-Start fehlgeschlagen");
        g_systemState = STATE_IDLE;

        // ESP-NOW wieder starten
        g_espNow.begin();
        g_espNow.setReceiveCallback(onESPNowReceive);
      }
      break;

    case STATE_OTA_ACTIVE:
      // OTA-Modus
      g_otaManager.handle();

      // OTA abgeschlossen oder Timeout?
      if (!g_otaManager.isActive()) {
        DEBUG_PRINTLN("[State] OTA_ACTIVE -> IDLE");
        g_systemState = STATE_IDLE;

        // ESP-NOW wieder starten
        g_espNow.begin();
        g_espNow.setReceiveCallback(onESPNowReceive);
      }
      break;

    case STATE_ERROR:
      // Fehler-Zustand - LED blinken lassen
      delay(500);
      break;

    default:
      g_systemState = STATE_IDLE;
      break;
  }

  delay(10);  // Kleine Pause für Watchdog
}

// ============================================================================
// ROLLEN-SPEZIFISCHE INITIALISIERUNG
// ============================================================================

#if DEVICE_ROLE == ROLE_ACTOR

void initializeActor() {
  DEBUG_PRINTLN("[Init] Actor-Modus");

  // Indikatoren vorbereiten
  IndicatorConfig indicators[3] = {
    ACTOR_INDICATOR_1,
    ACTOR_INDICATOR_2,
    ACTOR_INDICATOR_3
  };

  // Actor initialisieren
  g_actor.begin(ACTOR_ID, ACTOR_RELAY_PIN, ACTOR_ENABLE_PIN, indicators);
}

#elif DEVICE_ROLE == ROLE_CONTROLLER

void initializeController() {
  DEBUG_PRINTLN("[Init] Controller-Modus");

  // Taster vorbereiten
  ButtonConfig buttons[3] = {
    CONTROLLER_BUTTON_1,
    CONTROLLER_BUTTON_2,
    CONTROLLER_BUTTON_3
  };

  // LEDs vorbereiten
  IndicatorConfig leds[3] = {
    CONTROLLER_LED_1,
    CONTROLLER_LED_2,
    CONTROLLER_LED_3
  };

  // Controller initialisieren
  g_controller.begin(CONTROLLER_ID, CONTROLLER_ACTOR_IDS, CONTROLLER_ACTOR_COUNT,
                     buttons, leds, CONTROLLER_DISPLAY);
}

#endif

// ============================================================================
// ESP-NOW EMPFANGS-CALLBACK
// ============================================================================

void onESPNowReceive(const uint8_t* mac, const uint8_t* data, int len) {
  // Nachricht an entsprechende Rolle weiterleiten
#if DEVICE_ROLE == ROLE_ACTOR
  g_actor.handleMessage(mac, data, len);
#elif DEVICE_ROLE == ROLE_CONTROLLER
  g_controller.handleMessage(mac, data, len);
#endif
}
