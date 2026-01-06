#ifndef ESP_CODE_KEY_H
#define ESP_CODE_KEY_H

// ============================================================================
// ESP-CONTROL SICHERHEITS-KONFIGURATION
// ============================================================================
//
// WICHTIG: Diese Datei ist ein BEISPIEL!
//
// Um das System zu nutzen:
// 1. Kopiere diese Datei zu "ESP_Code_Key.h" (ohne "example_")
// 2. Trage die MAC-Adressen deiner ESP32-Geräte ein
// 3. Ändere den Shared Secret zu einem eigenen, zufälligen Wert
// 4. "ESP_Code_Key.h" ist in .gitignore und wird NICHT ins Repo committed
//
// WICHTIG: Diese Datei enthält sicherheitsrelevante Informationen!
//          Teile sie NIEMALS öffentlich!
//
// ============================================================================

// ============================================================================
// SHARED SECRET (PFLICHT)
// ============================================================================
//
// Dieser Schlüssel wird für die Authentifizierung und Nachrichtenvalidierung
// verwendet. Ändere ihn zu einem eigenen, zufälligen Wert!
//
// Länge: 32 Zeichen (256 Bit)
//
// Tipp: Generiere einen zufälligen Schlüssel mit:
//       openssl rand -hex 32
//
// ============================================================================

#define SHARED_SECRET "0123456789ABCDEF0123456789ABCDEF"  // <-- HIER ÄNDERN!

// ============================================================================
// MAC-ADRESSEN DER CONTROLLER (max. 10)
// ============================================================================
//
// Trage hier die MAC-Adressen der Controller ein.
// Die Array-Index entspricht der Controller-ID (0-9).
//
// Um die MAC-Adresse eines ESP32 herauszufinden:
// 1. Lade einen einfachen Sketch hoch:
//    void setup() {
//      Serial.begin(115200);
//      WiFi.mode(WIFI_STA);
//      Serial.println(WiFi.macAddress());
//    }
//    void loop() {}
//
// 2. Öffne den Serial Monitor und notiere die MAC-Adresse
//
// ============================================================================

// MAC-Adressen im Format: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}

// Controller 0
#define CONTROLLER_0_MAC {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56}

// Controller 1
#define CONTROLLER_1_MAC {0x24, 0x0A, 0xC4, 0x23, 0x45, 0x67}

// Controller 2
#define CONTROLLER_2_MAC {0x24, 0x0A, 0xC4, 0x34, 0x56, 0x78}

// Controller 3
#define CONTROLLER_3_MAC {0x24, 0x0A, 0xC4, 0x45, 0x67, 0x89}

// Controller 4
#define CONTROLLER_4_MAC {0x24, 0x0A, 0xC4, 0x56, 0x78, 0x9A}

// Controller 5
#define CONTROLLER_5_MAC {0x24, 0x0A, 0xC4, 0x67, 0x89, 0xAB}

// Controller 6
#define CONTROLLER_6_MAC {0x24, 0x0A, 0xC4, 0x78, 0x9A, 0xBC}

// Controller 7
#define CONTROLLER_7_MAC {0x24, 0x0A, 0xC4, 0x89, 0xAB, 0xCD}

// Controller 8
#define CONTROLLER_8_MAC {0x24, 0x0A, 0xC4, 0x9A, 0xBC, 0xDE}

// Controller 9
#define CONTROLLER_9_MAC {0x24, 0x0A, 0xC4, 0xAB, 0xCD, 0xEF}

// ============================================================================
// MAC-ADRESSEN DER ACTOREN (max. 10)
// ============================================================================

// Actor 0
#define ACTOR_0_MAC {0x30, 0xAE, 0xA4, 0x12, 0x34, 0x56}

// Actor 1
#define ACTOR_1_MAC {0x30, 0xAE, 0xA4, 0x23, 0x45, 0x67}

// Actor 2
#define ACTOR_2_MAC {0x30, 0xAE, 0xA4, 0x34, 0x56, 0x78}

// Actor 3
#define ACTOR_3_MAC {0x30, 0xAE, 0xA4, 0x45, 0x67, 0x89}

// Actor 4
#define ACTOR_4_MAC {0x30, 0xAE, 0xA4, 0x56, 0x78, 0x9A}

// Actor 5
#define ACTOR_5_MAC {0x30, 0xAE, 0xA4, 0x67, 0x89, 0xAB}

// Actor 6
#define ACTOR_6_MAC {0x30, 0xAE, 0xA4, 0x78, 0x9A, 0xBC}

// Actor 7
#define ACTOR_7_MAC {0x30, 0xAE, 0xA4, 0x89, 0xAB, 0xCD}

// Actor 8
#define ACTOR_8_MAC {0x30, 0xAE, 0xA4, 0x9A, 0xBC, 0xDE}

// Actor 9
#define ACTOR_9_MAC {0x30, 0xAE, 0xA4, 0xAB, 0xCD, 0xEF}

// ============================================================================
// OPTIONALE ERWEITERTE KONFIGURATION
// ============================================================================

// Falls du eine whitelist für Controller implementieren möchtest:
// (Aktuell nicht genutzt, aber für zukünftige Erweiterungen)

// Erlaube nur bestimmte Controller-IDs
// #define ALLOWED_CONTROLLERS {0, 1, 2, 3}
// #define ALLOWED_CONTROLLERS_COUNT 4

// Erlaube nur bestimmte Actor-IDs
// #define ALLOWED_ACTORS {0, 1, 2}
// #define ALLOWED_ACTORS_COUNT 3

// ============================================================================
// BEISPIEL-KONFIGURATIONEN
// ============================================================================

// --- Beispiel 1: Einfaches Setup mit 1 Controller und 1 Actor ---
//
// Controller 1 MAC: 24:0A:C4:23:45:67
// Actor 1 MAC:      30:AE:A4:23:45:67
//
// #define CONTROLLER_1_MAC {0x24, 0x0A, 0xC4, 0x23, 0x45, 0x67}
// #define ACTOR_1_MAC {0x30, 0xAE, 0xA4, 0x23, 0x45, 0x67}
// #define SHARED_SECRET "MeinGeheimesPasswort123456789AB"

// --- Beispiel 2: Setup mit 2 Controllern und 3 Actoren ---
//
// Controller 0 steuert Actor 0, 1
// Controller 1 steuert Actor 1, 2
//
// (Die Zuordnung erfolgt in ESP_Code_Conf.h über CONTROLLER_ACTOR_IDS)

// --- Beispiel 3: Multi-Controller-Setup ---
//
// Actor 0 kann von Controller 0, 1, 2 gesteuert werden
// Actor 1 kann von Controller 1, 2, 3 gesteuert werden
//
// (Ein Actor akzeptiert Commands von jedem Controller, der den
//  korrekten Shared Secret kennt)

// ============================================================================
// HINWEISE ZUR SICHERHEIT
// ============================================================================
//
// 1. Der SHARED_SECRET ist der wichtigste Sicherheitsschlüssel!
//    - Verwende einen langen, zufälligen Wert
//    - Ändere ihn regelmäßig
//    - Teile ihn NIEMALS öffentlich
//
// 2. MAC-Adressen sind NICHT geheim, aber wichtig für die Zuordnung
//    - Sie identifizieren die Geräte eindeutig
//    - Notiere sie sorgfältig
//
// 3. Das System verwendet ein Challenge-Response-Verfahren
//    - Nachrichten können nicht abgehört und wiederholt werden
//    - Jede Nachricht hat eine Nonce (Zufallswert)
//    - HMACs schützen vor Manipulation
//
// 4. Kein Rolling-Code nötig
//    - Das System verwendet In-Memory-Nonce-Cache
//    - Keine persistente Speicherung erforderlich
//    - Nach Neustart werden neue Session-Keys generiert
//
// ============================================================================

#endif // ESP_CODE_KEY_H
