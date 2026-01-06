# ESP-Control

ESP32-basiertes verteiltes Controller/Actor-System mit ESP-NOW Kommunikation.

## Überblick

ESP-Control ist ein System zur drahtlosen Steuerung von Relais über ESP-NOW:

- **Controller** können bis zu 3 Actoren steuern
- **Actoren** steuern Relais und können von bis zu 10 Controllern gesteuert werden
- Sichere Kommunikation mit Challenge-Response-Authentifizierung
- Koordinierter OTA-Update-Modus
- Discovery-Mechanismus für automatische Actor-Erkennung

## Features

- ✅ ESP-NOW basierte Kommunikation (keine WiFi-Infrastruktur nötig)
- ✅ Challenge-Response-Authentifizierung (ohne NVS-Speicher)
- ✅ Anti-Replay-Schutz mit In-Memory-Nonce-Cache
- ✅ Koordinierter OTA-Modus für Controller und Actoren
- ✅ Discovery-Mechanismus mit Beacon-System
- ✅ Flexible Hardware-Konfiguration über externe Header
- ✅ Unterstützung für LEDs, NeoPixel, Displays
- ✅ Long-Press für OTA-Modus
- ✅ Multi-Controller-Betrieb

## Erste Schritte

### 1. Konfigurationsdateien erstellen

Das Projekt benötigt zwei Konfigurationsdateien, die **nicht** im Repository enthalten sind:

```bash
cd ESP-Control
cp example_ESP_Code_Conf.h ESP_Code_Conf.h
cp example_ESP_Code_Key.h ESP_Code_Key.h
```

### 2. Hardware-Konfiguration anpassen

Öffne `ESP_Code_Conf.h` und passe folgende Werte an:

- **Rolle festlegen**: `ROLE_ACTOR` oder `ROLE_CONTROLLER`
- **Pins konfigurieren**: Relais, Taster, LEDs
- **WiFi-Credentials**: Für OTA-Updates

### 3. Sicherheits-Konfiguration

Öffne `ESP_Code_Key.h` und:

- Generiere einen **zufälligen Shared Secret** (32 Zeichen)
- Trage die **MAC-Adressen** deiner ESP32-Geräte ein

**Wichtig**: Diese Dateien enthalten sensible Daten und werden durch `.gitignore` geschützt!

### 4. Arduino IDE Einstellungen

- Board: **ESP32 Dev Module** (oder dein spezifisches Board)
- Upload Speed: **115200**
- Flash Frequency: **80MHz**
- Partition Scheme: **Default 4MB with spiffs**

### 5. Erforderliche Bibliotheken

Folgende Bibliotheken werden automatisch mit dem ESP32 Board-Package installiert:

- `WiFi.h`
- `esp_now.h`
- `ArduinoOTA.h`
- `mbedtls/*`

Keine zusätzlichen externen Bibliotheken erforderlich!

## Projektstruktur

```
ESP-Control/
├── ESP-Control.ino          # Hauptdatei mit setup() und loop()
├── Config.h                 # Zentrale Typ-Definitionen
├── Security.h/cpp           # Challenge-Response-Authentifizierung
├── ESPNowComm.h/cpp         # ESP-NOW Kommunikations-Layer
├── Discovery.h/cpp          # Actor-Discovery-Mechanismus
├── Actor.h/cpp              # Actor-Logik (Relais-Steuerung)
├── Controller.h/cpp         # Controller-Logik (Taster, Commands)
├── OTAManager.h/cpp         # Koordinierter OTA-Modus
├── example_ESP_Code_Conf.h  # Beispiel Hardware-Konfiguration
├── example_ESP_Code_Key.h   # Beispiel Security-Konfiguration
├── .gitignore               # Schützt lokale Konfiguration
└── README.md                # Diese Datei
```

## Bedienung

### Controller

- **Short Press** (< 2 Sekunden): Toggle Relais des zugeordneten Actors
- **Long Press** (≥ 2 Sekunden): Starte koordinierten OTA-Modus

### Actor

- **Enable-Pin** (optional): Steuert, ob Actor sichtbar für Controller ist
- **Indikatoren**:
  - LED 1: Relais-Status (EIN/AUS)
  - LED 2: Enable-Status (sichtbar/unsichtbar)
  - LED 3: ESP-NOW Status

## OTA-Updates

Der OTA-Modus wird koordiniert für alle Geräte aktiviert:

1. **Long Press** auf einem Controller-Taster
2. Controller sendet `OTA_PREPARE` an alle erreichbaren Actoren
3. Actoren bestätigen mit `OTA_ACK`
4. Alle Geräte:
   - Stoppen ESP-NOW
   - Verbinden mit WiFi
   - Starten ArduinoOTA
5. Updates über Arduino IDE durchführen
6. Nach Timeout oder erfolgreichem Update: Neustart

**Timeout**: 5 Minuten (konfigurierbar in `Config.h`)

## Sicherheit

Das System verwendet mehrere Sicherheitsebenen:

### 1. Challenge-Response-Authentifizierung

- Actoren generieren beim Boot einen zufälligen **Session-Key**
- Controller senden **Challenge**
- Actoren antworten mit **HMAC(SessionKey + SharedSecret, Challenge)**
- Kein Rolling-Code nötig (In-Memory-Cache)

### 2. Anti-Replay-Schutz

- Jede Nachricht enthält eine **Nonce** (Zufallswert)
- Nonce-Cache verhindert Replay-Angriffe
- Cache wird alle 10 Sekunden bereinigt

### 3. Command-Authentifizierung

- Alle Commands sind mit **HMAC-SHA256** signiert
- HMAC über Header + Shared Secret
- Ungültige HMACs werden abgelehnt

### 4. Keine persistente Speicherung

- Keine NVS-Zähler erforderlich
- Session-Keys werden bei jedem Boot neu generiert
- Sicherer gegen Flash-Wear-Angriffe

## Beispiel-Konfigurationen

### Actor mit Enable-Pin

```c
#define DEVICE_ROLE ROLE_ACTOR
#define ACTOR_ID 1
#define ACTOR_RELAY_PIN 23
#define ACTOR_ENABLE_PIN { .enabled = true, .pin = 22, .activeLow = false }
#define ACTOR_INDICATOR_1 { .enabled = true, .type = INDICATOR_LED, .pin = 2, .activeLow = false }
```

### Controller mit 3 Tastern

```c
#define DEVICE_ROLE ROLE_CONTROLLER
#define CONTROLLER_ID 1
#define CONTROLLER_ACTOR_IDS {1, 2, 3}
#define CONTROLLER_ACTOR_COUNT 3
#define CONTROLLER_BUTTON_1 { .enabled = true, .pin = 35, .activeLow = true }
#define CONTROLLER_BUTTON_2 { .enabled = true, .pin = 34, .activeLow = true }
#define CONTROLLER_BUTTON_3 { .enabled = true, .pin = 39, .activeLow = true }
```

### Controller mit 1 Taster für 3 Actoren (Priorisierung)

```c
#define DEVICE_ROLE ROLE_CONTROLLER
#define CONTROLLER_ID 1
#define CONTROLLER_ACTOR_IDS {1, 2, 3}
#define CONTROLLER_ACTOR_COUNT 3
#define CONTROLLER_BUTTON_1 { .enabled = true, .pin = 35, .activeLow = true }
#define CONTROLLER_BUTTON_2 { .enabled = false, .pin = 0, .activeLow = false }
#define CONTROLLER_BUTTON_3 { .enabled = false, .pin = 0, .activeLow = false }
```

In diesem Fall steuert der eine Taster den Actor mit der höchsten Priorität (Actor 1).

## MAC-Adressen herausfinden

Um die MAC-Adresse eines ESP32 zu ermitteln:

```cpp
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(100);
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.macAddress());
}

void loop() {}
```

Öffne den Serial Monitor und notiere die Adresse im Format `AA:BB:CC:DD:EE:FF`.

Trage sie dann in `ESP_Code_Key.h` ein:

```c
#define ACTOR_1_MAC {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
```

## Debugging

Logging kann über `g_debugEnabled` in `Config.h` aktiviert/deaktiviert werden:

```cpp
bool g_debugEnabled = true;  // Aktiviert
```

Log-Ausgaben erfolgen über `Serial` (115200 Baud).

## Troubleshooting

### ESP-NOW startet nicht

- Prüfe, ob WiFi im Station-Mode ist
- Stelle sicher, dass keine anderen WiFi-Verbindungen aktiv sind

### Actor wird nicht entdeckt

- Prüfe Enable-Pin (falls konfiguriert)
- Prüfe MAC-Adressen in `ESP_Code_Key.h`
- Prüfe Actor-IDs in `ESP_Code_Conf.h`
- Erhöhe Beacon-Intervall falls nötig

### Relais schaltet nicht

- Prüfe HMAC (Shared Secret muss übereinstimmen)
- Prüfe Nonce-Cache (Replay-Schutz)
- Prüfe Relais-Pin und Hardware

### OTA funktioniert nicht

- Prüfe WiFi-Credentials in `ESP_Code_Conf.h`
- Stelle sicher, dass Controller und Actoren im selben Netzwerk sind
- Prüfe Firewall-Einstellungen (Port 3232)

## Lizenz

Dieses Projekt ist Open Source.

## Autor

Erstellt mit Claude Code (Anthropic).
