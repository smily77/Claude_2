# ESP8266 ESP-NOW Sensor Node - Anleitung

## Überblick

Dieser Code wurde von ThingSpeak/WiFi auf ESP-NOW umgestellt, um **deutlich weniger Strom** zu verbrauchen. ESP-NOW ist perfekt für batteriebetriebene Sensoren!

### Vorteile gegenüber ThingSpeak/WiFi:
- ⚡ **10-50x schneller** (typisch 20-50ms statt 2-5 Sekunden)
- 🔋 **Viel weniger Stromverbrauch** (keine WiFi-Verbindung nötig)
- 📡 **Bis zu 200m Reichweite** (im Freien, teilweise mehr)
- 🚀 **Einfacher und zuverlässiger**

## Hardware

### ESP8266 ESP1 (Sender)
- **GPIO0** → BMP180 SDA
- **GPIO2** → BMP180 SCL
- **GPIO16** → RST (für Deep Sleep Wake-up!)
- **VCC** → 3.3V
- **GND** → GND

⚠️ **WICHTIG:** GPIO16 muss mit RST verbunden sein, damit der ESP aus dem Deep Sleep aufwachen kann!

### Empfänger
- Kann ein beliebiger ESP8266 oder ESP32 sein
- Läuft dauerhaft und empfängt die Daten
- Kann dann die Daten weiterverarbeiten (MQTT, Display, Datenbank, etc.)

**Zwei Empfänger-Beispiele verfügbar:**
- `ESP_NOW_Receiver/` - Für ESP8266 (9600 Baud)
- `ESP_NOW_Receiver_ESP32/` - Für ESP32-C3 und andere ESP32 (115200 Baud)

## Installation

### Arduino IDE Setup

#### Für den Sender (ESP8266):

1. **ESP8266 Board** installieren (falls noch nicht geschehen):
   - Datei → Voreinstellungen
   - Zusätzliche Boardverwalter-URLs: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Werkzeuge → Board → Boardverwalter → "esp8266" suchen und installieren

2. **Bibliotheken** installieren:
   - Sketch → Bibliothek einbinden → Bibliotheken verwalten
   - Suchen und installieren: **SFE_BMP180** (von SparkFun)

3. **Board-Einstellungen**:
   - Board: "Generic ESP8266 Module"
   - Flash Size: "1MB (FS:none OTA:~502KB)"
   - CPU Frequency: "80 MHz" (für weniger Stromverbrauch)
   - Upload Speed: "115200"

#### Für den Empfänger (ESP32-C3):

1. **ESP32 Board** installieren (falls noch nicht geschehen):
   - Datei → Voreinstellungen
   - Zusätzliche Boardverwalter-URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Werkzeuge → Board → Boardverwalter → "esp32" suchen und installieren

2. **Board-Einstellungen für ESP32-C3**:
   - Board: "ESP32C3 Dev Module"
   - Upload Speed: "115200"
   - CPU Frequency: "160 MHz"
   - Flash Mode: "QIO"
   - Flash Size: "4MB"

## Konfiguration

### 1. Sender-Sketch (ESP8266_ESP_NOW_Sensor.ino)

Öffnen Sie den Sketch und passen Sie folgende Konstanten an:

```cpp
// ==================== WICHTIGE EINSTELLUNGEN ====================

// DEBUG Mode
#define DEBUG false  // true = serielle Ausgabe, false = produktiv

// Empfänger MAC-Adresse
// Für Broadcast (alle empfangen):
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ODER für spezifischen Empfänger (siehe unten wie Sie die MAC finden):
// uint8_t receiverMAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// Sleep Zeit in Minuten
#define SLEEP_TIME_MINUTES 15  // Alle 15 Minuten aufwachen

// WiFi Kanal (MUSS beim Empfänger gleich sein!)
#define ESPNOW_CHANNEL 1  // Kanal 1-13

// Battery Protection
#define BATTERY_LIMIT 2600  // mV - darunter wird länger geschlafen
#define BATTERY_EXTRA_CYCLES 2  // Multiplikator bei niedrigem Akku
```

### 2. Empfänger MAC-Adresse herausfinden

**Option A: Broadcast nutzen (einfachste Methode)**
```cpp
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
```
- Vorteil: Funktioniert sofort, mehrere Empfänger möglich
- Nachteil: Etwas höherer Stromverbrauch

**Option B: Spezifische MAC-Adresse** (empfohlen für besten Stromverbrauch)

1. Laden Sie zuerst den **Empfänger-Sketch** auf den Empfänger
2. Öffnen Sie den Seriellen Monitor:
   - ESP8266 Empfänger: 9600 Baud
   - ESP32-C3 Empfänger: 115200 Baud
3. Die MAC-Adresse wird beim Start angezeigt, z.B.: `AA:BB:CC:DD:EE:FF`
4. Tragen Sie diese im Sender ein:
   ```cpp
   uint8_t receiverMAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
   ```

## Upload und Test

### 1. Empfänger starten

**Für ESP8266 Empfänger:**
1. `ESP_NOW_Receiver.ino` hochladen
2. Seriellen Monitor öffnen (9600 Baud)
3. MAC-Adresse notieren
4. Gerät läuft und wartet auf Daten

**Für ESP32-C3 Empfänger:**
1. `ESP_NOW_Receiver_ESP32.ino` hochladen
2. Seriellen Monitor öffnen (115200 Baud)
3. MAC-Adresse notieren (wird auch im C-Array Format angezeigt!)
4. Gerät läuft und wartet auf Daten

### 2. Sender flashen und testen

**WICHTIG:** Beim ersten Upload mit DEBUG aktivieren!

```cpp
#define DEBUG true  // Für ersten Test
```

1. Upload auf ESP8266
2. Seriellen Monitor öffnen (9600 Baud)
3. Sie sollten sehen:
   - Battery Voltage
   - Sensor Daten
   - ESP-NOW Send Status
   - "Going to sleep..."

4. Wenn alles funktioniert: DEBUG auf `false` setzen und neu hochladen!

```cpp
#define DEBUG false  // Für Produktivbetrieb
```

### 3. Empfänger prüfen

Im Seriellen Monitor des Empfängers sollten Sie sehen:
```
========================================
New Data Received!
========================================
From MAC: XX:XX:XX:XX:XX:XX

--- Sensor Data ---
Temperature: 21.50 °C
Pressure: 1013.25 mbar
Battery: 3200 mV
Duration: 45 ms
...
```

## Datenstruktur

Die gesendeten Daten enthalten:

```cpp
struct sensor_data {
  uint32_t timestamp;        // Millisekunden seit Start
  float temperature;         // Temperatur in °C (BMP180)
  float pressure;            // Luftdruck in mbar
  uint16_t battery_voltage;  // Batteriespannung in mV
  uint16_t duration;         // Wie lange die letzte Messung dauerte (ms)
  uint8_t battery_warning;   // 1 = Batterie niedrig
  uint8_t sensor_error;      // 0 = OK, >0 = Fehlercode
  uint8_t reset_reason;      // Grund für letzten Reset
  uint8_t reserved;          // Für zukünftige Nutzung
};
```

Gesamt: **20 Bytes** (ESP-NOW unterstützt bis 250 Bytes)

## Stromverbrauch Optimierung

Der Code ist bereits optimiert für minimalen Stromverbrauch:

### Was bereits gemacht wird:
✅ Keine WiFi-Verbindung (nur ESP-NOW)
✅ Ungenutzte Pins ausgeschaltet
✅ Deep Sleep zwischen Messungen
✅ Minimale Wakeup-Zeit (typisch 20-50ms)
✅ Battery Protection (längerer Sleep bei niedrigem Akku)

### Erwarteter Stromverbrauch:
- **Deep Sleep:** ~20 µA (0.02 mA)
- **Aktiv:** ~80 mA für 20-50ms
- **Durchschnitt** (15 Min Intervall): **~0.03 mA**

→ Mit 2000mAh Batterie: **Theoretisch >7 Jahre Laufzeit!**
   (In Praxis 1-2 Jahre wegen Batterieselbstentladung)

### Weitere Optimierungen möglich:
- CPU auf 80 MHz (statt 160 MHz) - bereits im Code
- Längere Sleep-Intervalle (z.B. 30 oder 60 Minuten)
- WiFi Kanal optimieren (niedrigere Kanäle = etwas weniger Strom)

## Fehlerbehebung

### Empfänger empfängt keine Daten

1. **WiFi Kanal prüfen:** Muss bei Sender und Empfänger gleich sein!
   ```cpp
   #define ESPNOW_CHANNEL 1
   ```

2. **MAC-Adresse prüfen:** Bei spezifischer MAC korrekt eingetragen?

3. **Reichweite:** Sender und Empfänger zu weit auseinander?
   - Test: Erst mal auf 1-2 Meter testen

4. **Debug aktivieren:** Am Sender `#define DEBUG true` setzen

### Sensor-Fehler

Wenn `sensor_error > 0`:
- **Code 1:** BMP180 nicht gefunden → I2C Verkabelung prüfen
- **Code 2-5:** BMP180 Lesefehler → Sensor defekt?

### ESP wacht nicht aus Deep Sleep auf

⚠️ **GPIO16 muss mit RST verbunden sein!**

### Batterie entlädt schnell

1. DEBUG ausgeschaltet? (`#define DEBUG false`)
2. Serielle Pins können Strom ziehen - nicht mit PC verbinden im Betrieb
3. BMP180 Stromverbrauch: ~0.005 mA (vernachlässigbar)

## Weiterverarbeitung der Daten

Im Empfänger-Sketch können Sie die Daten weiterverarbeiten:

### MQTT senden (z.B. an Home Assistant)
```cpp
#include <PubSubClient.h>
// ... in onDataRecv():
char topic[50];
sprintf(topic, "sensor/temp");
client.publish(topic, String(receivedData.temperature).c_str());
```

### Auf OLED Display anzeigen
```cpp
#include <Adafruit_SSD1306.h>
// ... in onDataRecv():
display.clearDisplay();
display.printf("Temp: %.1f C\n", receivedData.temperature);
display.display();
```

### An InfluxDB senden
```cpp
// Für Langzeit-Datenlogging
```

### An Thingspeak senden (vom Empfänger)
```cpp
// Wenn Sie doch noch ThingSpeak nutzen wollen,
// kann der Empfänger (der am Strom hängt) die Daten hochladen
```

## Technische Details

### ESP-NOW vs WiFi/ThingSpeak

| Feature | ThingSpeak/WiFi | ESP-NOW |
|---------|-----------------|---------|
| Verbindungszeit | 2-5 Sekunden | 20-50 ms |
| Stromverbrauch | ~300 mAh/Tag | ~1 mAh/Tag |
| Reichweite | WLAN-Reichweite | 200m+ |
| Latenz | Hoch | Sehr niedrig |
| Internet nötig | Ja | Nein |

### Reset Reasons (reset_reason)
- 0: Power-on
- 1: Hardware Watchdog Reset
- 2: Software Watchdog Reset
- 3: Software Reset
- 4: Deep Sleep Wake
- 5: External Reset

### ESP8266 vs ESP32 Empfänger - Unterschiede

| Feature | ESP8266 | ESP32-C3 |
|---------|---------|----------|
| Bibliothek | `#include <espnow.h>` | `#include <esp_now.h>` |
| WiFi Library | `ESP8266WiFi.h` | `WiFi.h` |
| Baudrate | 9600 | 115200 |
| API | Älter, einfacher | Neuer, mehr Features |
| Callback Signatur | `(uint8_t*, uint8_t*, uint8_t)` | `(const uint8_t*, const uint8_t*, int)` |
| Rollen | CONTROLLER/SLAVE | Keine Rollen |
| Peer Management | Einfach | Komplexer (peer_info) |
| Performance | Gut | Besser |
| RSSI Info | Schwierig | Einfach verfügbar |
| Preis | Günstiger | Etwas teurer |

**Empfehlung:**
- **ESP8266** als Empfänger: Wenn Sie schon einen haben oder Kosten sparen möchten
- **ESP32-C3** als Empfänger: Für bessere Performance, mehr Features und einfacheres RSSI-Monitoring

**WICHTIG:** Der Sender (ESP8266 ESP1) bleibt gleich - nur der Empfänger kann variieren!

## Lizenz

Basierend auf dem Original ThingSpeak-Code, umgeschrieben für ESP-NOW.
Frei verwendbar für private und kommerzielle Projekte.

## Support

Bei Fragen oder Problemen:
1. DEBUG Mode aktivieren und Serial Output prüfen
2. Verkabelung kontrollieren (besonders GPIO16→RST!)
3. WiFi Kanal bei Sender und Empfänger gleich?

Viel Erfolg mit Ihrem stromsparenden Sensor! 🔋⚡
