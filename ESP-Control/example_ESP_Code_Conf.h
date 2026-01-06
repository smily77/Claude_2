#ifndef ESP_CODE_CONF_H
#define ESP_CODE_CONF_H

// ============================================================================
// ESP-CONTROL GERÄTE-KONFIGURATION
// ============================================================================
//
// WICHTIG: Diese Datei ist ein BEISPIEL!
//
// Um das System zu nutzen:
// 1. Kopiere diese Datei zu "ESP_Code_Conf.h" (ohne "example_")
// 2. Passe die Werte an deine Hardware an
// 3. "ESP_Code_Conf.h" ist in .gitignore und wird NICHT ins Repo committed
//
// ============================================================================

#include "Config.h"

// ============================================================================
// ROLLE FESTLEGEN (PFLICHT)
// ============================================================================
//
// Wähle die Rolle dieses Geräts:
// - ROLE_ACTOR      : Gerät steuert ein Relais
// - ROLE_CONTROLLER : Gerät steuert andere Actoren
//
// ============================================================================

#define DEVICE_ROLE ROLE_ACTOR    // <-- HIER ANPASSEN!

// Optional: Geräte-ID für Logging/Debugging
#define DEVICE_ID 1

// ============================================================================
// ACTOR-KONFIGURATION (NUR wenn DEVICE_ROLE == ROLE_ACTOR)
// ============================================================================

#if DEVICE_ROLE == ROLE_ACTOR

// --- Actor-ID (0-9) ---
// Diese ID identifiziert den Actor eindeutig im System
#define ACTOR_ID 1

// --- Relais-Pin ---
// GPIO-Pin, an dem das Relais angeschlossen ist
#define ACTOR_RELAY_PIN 23

// --- Enable-Pin (optional) ---
// Wenn vorhanden, steuert dieser Pin, ob der Actor "sichtbar" für Controller ist
// Beispiel: Ein Schalter, der den Actor aktiviert/deaktiviert
#define ACTOR_ENABLE_PIN { \
  .enabled = true, \
  .pin = 22, \
  .activeLow = false \
}

// Alternativ: Kein Enable-Pin (immer sichtbar)
// #define ACTOR_ENABLE_PIN { .enabled = false, .pin = 0, .activeLow = false }

// --- Indikator 1 (Relais-Status) ---
// Zeigt den Status des Relais an (EIN/AUS)
#define ACTOR_INDICATOR_1 { \
  .enabled = true, \
  .type = INDICATOR_LED, \
  .pin = 2, \
  .activeLow = false \
}

// Alternativ: Kein Indikator
// #define ACTOR_INDICATOR_1 { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }

// --- Indikator 2 (Enable-Status) ---
// Zeigt an, ob der Actor "sichtbar" ist
#define ACTOR_INDICATOR_2 { \
  .enabled = true, \
  .type = INDICATOR_LED, \
  .pin = 4, \
  .activeLow = false \
}

// --- Indikator 3 (ESP-NOW Status) ---
// Zeigt an, ob ESP-NOW aktiv ist
#define ACTOR_INDICATOR_3 { \
  .enabled = true, \
  .type = INDICATOR_LED, \
  .pin = 5, \
  .activeLow = false \
}

// --- NeoPixel Beispiel (für Indikator) ---
// #define ACTOR_INDICATOR_1 { \
//   .enabled = true, \
//   .type = INDICATOR_PIXEL, \
//   .pin = 27, \
//   .activeLow = false \
// }

#endif // ROLE_ACTOR

// ============================================================================
// CONTROLLER-KONFIGURATION (NUR wenn DEVICE_ROLE == ROLE_CONTROLLER)
// ============================================================================

#if DEVICE_ROLE == ROLE_CONTROLLER

// --- Controller-ID (0-9) ---
// Diese ID identifiziert den Controller eindeutig im System
#define CONTROLLER_ID 1

// --- Liste der Actor-IDs, die gesteuert werden sollen ---
// Maximal 10 Actoren können konfiguriert werden
// Die Reihenfolge bestimmt die Zuordnung zu den Tastern
#define CONTROLLER_ACTOR_IDS {1, 2, 3}
#define CONTROLLER_ACTOR_COUNT 3

// --- Taster 1 (PFLICHT) ---
// Steuert den ersten Actor in der Liste
#define CONTROLLER_BUTTON_1 { \
  .enabled = true, \
  .pin = 35, \
  .activeLow = true \
}

// --- Taster 2 (optional) ---
// Steuert den zweiten Actor in der Liste
#define CONTROLLER_BUTTON_2 { \
  .enabled = true, \
  .pin = 34, \
  .activeLow = true \
}

// --- Taster 3 (optional) ---
// Steuert den dritten Actor in der Liste
#define CONTROLLER_BUTTON_3 { \
  .enabled = true, \
  .pin = 39, \
  .activeLow = true \
}

// Alternativ: Taster deaktiviert
// #define CONTROLLER_BUTTON_2 { .enabled = false, .pin = 0, .activeLow = false }
// #define CONTROLLER_BUTTON_3 { .enabled = false, .pin = 0, .activeLow = false }

// --- LED 1 (Anzahl entdeckter Actoren) ---
#define CONTROLLER_LED_1 { \
  .enabled = true, \
  .type = INDICATOR_LED, \
  .pin = 2, \
  .activeLow = false \
}

// --- LED 2 (ESP-NOW Status) ---
#define CONTROLLER_LED_2 { \
  .enabled = true, \
  .type = INDICATOR_LED, \
  .pin = 4, \
  .activeLow = false \
}

// --- LED 3 (Relais-Status des ersten Actors) ---
#define CONTROLLER_LED_3 { \
  .enabled = true, \
  .type = INDICATOR_LED, \
  .pin = 5, \
  .activeLow = false \
}

// --- NeoPixel (optional) ---
#define CONTROLLER_PIXEL { \
  .enabled = false, \
  .type = INDICATOR_NONE, \
  .pin = 0, \
  .activeLow = false \
}

// --- Display (optional) ---
// Typ: DISPLAY_NONE, DISPLAY_SSD1306, DISPLAY_BUILTIN
#define CONTROLLER_DISPLAY { \
  .type = DISPLAY_NONE, \
  .sda_pin = 21, \
  .scl_pin = 22 \
}

// Beispiel: SSD1306 OLED Display
// #define CONTROLLER_DISPLAY { \
//   .type = DISPLAY_SSD1306, \
//   .sda_pin = 21, \
//   .scl_pin = 22 \
// }

// Beispiel: Built-in Display (z.B. ATOM 3S)
// #define CONTROLLER_DISPLAY { \
//   .type = DISPLAY_BUILTIN, \
//   .sda_pin = 0, \
//   .scl_pin = 0 \
// }

#endif // ROLE_CONTROLLER

// ============================================================================
// OTA-KONFIGURATION (BEIDE ROLLEN)
// ============================================================================

// WiFi-Zugangsdaten für OTA
// WICHTIG: Wenn diese nicht gesetzt sind, funktioniert OTA nicht!
#define OTA_WIFI_SSID "MeinWiFi"
#define OTA_WIFI_PASSWORD "MeinPasswort123"

// OTA-Hostname (erscheint in der Arduino IDE)
#if DEVICE_ROLE == ROLE_ACTOR
  #define OTA_HOSTNAME "ESP-Actor-1"
#elif DEVICE_ROLE == ROLE_CONTROLLER
  #define OTA_HOSTNAME "ESP-Controller-1"
#else
  #define OTA_HOSTNAME "ESP-Control"
#endif

// ============================================================================
// BEISPIEL-KONFIGURATIONEN
// ============================================================================

// --- Actor mit Enable-Pin und 3 LEDs ---
// #define DEVICE_ROLE ROLE_ACTOR
// #define ACTOR_ID 1
// #define ACTOR_RELAY_PIN 23
// #define ACTOR_ENABLE_PIN { .enabled = true, .pin = 22, .activeLow = false }
// #define ACTOR_INDICATOR_1 { .enabled = true, .type = INDICATOR_LED, .pin = 2, .activeLow = false }
// #define ACTOR_INDICATOR_2 { .enabled = true, .type = INDICATOR_LED, .pin = 4, .activeLow = false }
// #define ACTOR_INDICATOR_3 { .enabled = true, .type = INDICATOR_LED, .pin = 5, .activeLow = false }

// --- Actor ohne Enable-Pin ---
// #define DEVICE_ROLE ROLE_ACTOR
// #define ACTOR_ID 2
// #define ACTOR_RELAY_PIN 23
// #define ACTOR_ENABLE_PIN { .enabled = false, .pin = 0, .activeLow = false }
// #define ACTOR_INDICATOR_1 { .enabled = true, .type = INDICATOR_LED, .pin = 2, .activeLow = false }
// #define ACTOR_INDICATOR_2 { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }
// #define ACTOR_INDICATOR_3 { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }

// --- Controller mit 3 Tastern und 3 LEDs ---
// #define DEVICE_ROLE ROLE_CONTROLLER
// #define CONTROLLER_ID 1
// #define CONTROLLER_ACTOR_IDS {1, 2, 3}
// #define CONTROLLER_ACTOR_COUNT 3
// #define CONTROLLER_BUTTON_1 { .enabled = true, .pin = 35, .activeLow = true }
// #define CONTROLLER_BUTTON_2 { .enabled = true, .pin = 34, .activeLow = true }
// #define CONTROLLER_BUTTON_3 { .enabled = true, .pin = 39, .activeLow = true }
// #define CONTROLLER_LED_1 { .enabled = true, .type = INDICATOR_LED, .pin = 2, .activeLow = false }
// #define CONTROLLER_LED_2 { .enabled = true, .type = INDICATOR_LED, .pin = 4, .activeLow = false }
// #define CONTROLLER_LED_3 { .enabled = true, .type = INDICATOR_LED, .pin = 5, .activeLow = false }
// #define CONTROLLER_PIXEL { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }
// #define CONTROLLER_DISPLAY { .type = DISPLAY_NONE, .sda_pin = 0, .scl_pin = 0 }

// --- Controller mit nur 1 Taster für 3 Actoren (Priorisierung) ---
// #define DEVICE_ROLE ROLE_CONTROLLER
// #define CONTROLLER_ID 1
// #define CONTROLLER_ACTOR_IDS {1, 2, 3}
// #define CONTROLLER_ACTOR_COUNT 3
// #define CONTROLLER_BUTTON_1 { .enabled = true, .pin = 35, .activeLow = true }
// #define CONTROLLER_BUTTON_2 { .enabled = false, .pin = 0, .activeLow = false }
// #define CONTROLLER_BUTTON_3 { .enabled = false, .pin = 0, .activeLow = false }
// #define CONTROLLER_LED_1 { .enabled = true, .type = INDICATOR_LED, .pin = 2, .activeLow = false }
// #define CONTROLLER_LED_2 { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }
// #define CONTROLLER_LED_3 { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }
// #define CONTROLLER_PIXEL { .enabled = false, .type = INDICATOR_NONE, .pin = 0, .activeLow = false }
// #define CONTROLLER_DISPLAY { .type = DISPLAY_NONE, .sda_pin = 0, .scl_pin = 0 }
// In diesem Fall steuert der eine Taster den Actor mit der höchsten Priorität (Actor 1)

#endif // ESP_CODE_CONF_H
