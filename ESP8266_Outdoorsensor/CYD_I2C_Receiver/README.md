# CYD I2C Sensor Display System

## Übersicht

Dieses System verwendet einen **ESP32-C3** als Bridge zwischen ESP-NOW Sensoren und einem **CYD Display** (ESP32 mit Display). Die Sensordaten werden via ESP-NOW empfangen und über I2C an das Display übertragen.

## System-Architektur

```
[ESP8266 Indoor Sensor]  ─ESP-NOW─┐
                                    ├─> [ESP32-C3 Bridge] ─I2C─> [CYD Display]
[ESP8266 Outdoor Sensor] ─ESP-NOW─┘
```

**Warum ESP32-C3 als Bridge?**
- CYD benötigt WiFi für NTP (Zeitanzeige)
- ESP-NOW und WiFi können nicht auf dem gleichen ESP32 koexistieren
- ESP32-C3 empfängt ESP-NOW Daten und stellt sie via I2C bereit

## Hardware Setup

### ESP32-C3 Bridge (Slave)
- **ESP-NOW:** Empfängt Sensordaten auf Kanal 1
- **I2C Slave:** Adresse 0x20
- **Pins:** SDA=GPIO 8, SCL=GPIO 9 (Default)

### CYD Display (Master)
- **I2C Master:** Pins SDA=22, SCL=27
- **WiFi:** Für NTP Zeitsynchronisation
- **Display:** 320x240 oder 480x320

**Pull-up Widerstände:** 4.7kΩ auf SDA und SCL erforderlich!

## Programme

### 1. ESP32-C3_Bridge_Slave ⭐ HAUPTPROGRAMM
**Zweck:** ESP-NOW zu I2C Bridge (produktiv)

**Features:**
- Empfängt Indoor/Outdoor Sensordaten via ESP-NOW
- Stellt Daten über I2C bereit (mit I2CSensorBridge Library)
- Unterscheidet automatisch Indoor/Outdoor anhand Paketgröße
- Trackt Sensor-Status (aktiv/inaktiv basierend auf Timeout)

**I2C Daten-Strukturen:**
- `0x01`: Indoor Daten (Temperatur, Luftfeuchtigkeit, Druck, Batterie)
- `0x02`: Outdoor Daten (Temperatur, Druck, Batterie)
- `0x03`: System Status (Sensor aktiv/inaktiv, Paket-Counter)

**Timeouts:**
- Indoor: 5 Minuten (sendet alle 60 Sekunden)
- Outdoor: 20 Minuten (sendet alle 15 Minuten)

### 2. CYD_I2C_Master ⭐ HAUPTPROGRAMM
**Zweck:** Display für Sensordaten

**Features:**
- Liest Sensordaten von ESP32-C3 Bridge via I2C
- Zeigt Indoor/Outdoor Daten auf Display an
- WiFi für NTP Zeitsynchronisation
- Batterie-Warnung bei niedrigem Ladezustand
- RSSI-Anzeige (Signal-Qualität)

**Display Layout:**
- Links: Indoor Sensor (Temperatur, Luftfeuchtigkeit, Druck)
- Rechts: Outdoor Sensor (Temperatur, Druck)
- Oben: WiFi-Status, Uhrzeit, Bridge-Status

### 3. ESP32-C3_Bridge_Direct
**Zweck:** Vereinfachte Bridge OHNE Library (für Debugging)

**Unterschied zu Bridge_Slave:**
- Verwendet direkt Wire.begin() statt I2CSensorBridge
- Vereinfachtes Daten-Handling
- Nützlich zum Testen und Debuggen

### 4. CYD_I2C_Scanner
**Zweck:** I2C Bus Scanner für Diagnose

**Verwendung:**
```
1. Auf CYD flashen
2. Prüfen ob Bridge bei 0x20 sichtbar ist
3. Wenn NEIN: Hardware-Problem oder Bridge läuft nicht
```

## ⚠️ KRITISCH: ESP32-C3 I2C Slave Initialisierung

### Das Problem

ESP32-C3 pre-initialisiert I2C mit Default-Pins **VOR** `setup()`.

`Wire.begin(sda, scl, address)` mit Pin-Parametern schlägt fehl!

### Die Lösung ✅

**Im ESP32-C3_Bridge_Slave:**
```cpp
// FALSCH (funktioniert NICHT):
i2cBridge.beginSlave(I2C_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN);

// RICHTIG (funktioniert):
i2cBridge.beginSlave(I2C_SLAVE_ADDRESS);  // OHNE Pins!
```

Die Library ruft dann intern `Wire.begin((uint8_t)address)` auf.

### Setup-Reihenfolge

```cpp
void setup() {
    // 1. WiFi/ESP-NOW ZUERST
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_now_init();
    esp_now_register_recv_cb(onDataReceive);

    delay(500);  // WiFi stabilisieren

    // 2. I2C DANACH - OHNE Pin-Parameter!
    i2cBridge.beginSlave(I2C_SLAVE_ADDRESS);
    i2cBridge.registerStruct(0x01, &indoorData, 1, "Indoor");
    i2cBridge.registerStruct(0x02, &outdoorData, 1, "Outdoor");
    i2cBridge.registerStruct(0x03, &systemStatus, 1, "Status");
}
```

## I2CSensorBridge Library

### Konzept

Multi-Struct I2C Kommunikation zwischen mehreren ESP32 Geräten.

**Features:**
- Mehrere Daten-Strukturen pro Slave
- Automatische "New Data" Flags
- Master fragt gezielt nach geänderten Daten
- Flexible Struct-Größen bis 128 Bytes

### Master-Verwendung

```cpp
I2CSensorBridge i2cBridge;

void setup() {
    // Als Master initialisieren
    i2cBridge.beginMaster(SDA_PIN, SCL_PIN, 100000);

    // Structs registrieren (lokal)
    i2cBridge.registerStruct(0x01, &indoorData, 1, "Indoor");
    i2cBridge.registerStruct(0x02, &outdoorData, 1, "Outdoor");
}

void loop() {
    // Neue Daten prüfen
    uint8_t newDataMask = i2cBridge.checkNewData(SLAVE_ADDRESS);

    // Indoor Daten lesen (wenn neu)
    if (newDataMask & 0x02) {  // Bit 1 für Struct ID 0x01
        i2cBridge.readStruct(SLAVE_ADDRESS, 0x01, indoorData);
    }

    // Outdoor Daten lesen (wenn neu)
    if (newDataMask & 0x04) {  // Bit 2 für Struct ID 0x02
        i2cBridge.readStruct(SLAVE_ADDRESS, 0x02, outdoorData);
    }
}
```

### Slave-Verwendung

```cpp
I2CSensorBridge i2cBridge;

void setup() {
    // Als Slave initialisieren (OHNE Pins!)
    i2cBridge.beginSlave(SLAVE_ADDRESS);

    // Structs registrieren
    i2cBridge.registerStruct(0x01, &indoorData, 1, "Indoor");
    i2cBridge.registerStruct(0x02, &outdoorData, 1, "Outdoor");
}

void loop() {
    // Daten aktualisieren
    indoorData.temperature = 23.5;
    i2cBridge.updateStruct(0x01, indoorData);  // Setzt "new data" Flag
}
```

## Installation & Setup

### 1. ESP32-C3 Bridge flashen

```
1. ESP32-C3_Bridge_Slave.ino öffnen
2. Board: "ESP32C3 Dev Module"
3. Upload
4. Serial Monitor: Sollte zeigen "[I2C Bridge] Slave mode initialized on address 0x20"
```

### 2. CYD Display flashen

```
1. CYD_I2C_Master.ino öffnen
2. Credentials.h mit WiFi SSID/Passwort anpassen
3. Board: "ESP32 Dev Module" (oder spezifisches CYD Board)
4. Upload
5. Display sollte Sensordaten anzeigen
```

### 3. Hardware verbinden

```
ESP32-C3 Pin 8 (SDA)  ─┬─ 4.7kΩ ─ 3.3V
                       └─────────── CYD Pin 22 (SDA)

ESP32-C3 Pin 9 (SCL)  ─┬─ 4.7kΩ ─ 3.3V
                       └─────────── CYD Pin 27 (SCL)

ESP32-C3 GND ──────────────────── CYD GND
```

## Troubleshooting

### Bridge nicht gefunden (CYD zeigt "Bridge not responding!")

**Diagnose:**
```
1. CYD_I2C_Scanner auf CYD flashen
2. Prüfen ob 0x20 gefunden wird
```

**Wenn NICHT gefunden:**
1. ✅ ESP32-C3 Bridge läuft?
2. ✅ Serial Output zeigt "Slave mode initialized"?
3. ✅ Pull-up Widerstände vorhanden?
4. ✅ Kabel korrekt verbunden?
5. ✅ GND verbunden?

### Outdoor-Daten verschwinden

**Ursache:** Timeout zu kurz (Standard: 5 Minuten)

**Lösung:** In ESP32-C3_Bridge_Slave.ino ist bereits auf 20 Minuten erhöht:
```cpp
if (systemStatus.outdoor_last_seen > 1200) {  // 20 Minuten
    systemStatus.outdoor_active = 0;
}
```

### WiFi funktioniert nicht auf CYD

**Lösung:** In CYD_I2C_Master.ino Credentials.h prüfen:
```cpp
const char* ssid = "DEIN_WIFI_SSID";
const char* password = "DEIN_WIFI_PASSWORT";
```

## Daten-Strukturen

### IndoorData (0x01)
```cpp
struct IndoorData {
    float temperature;      // °C
    float humidity;        // %
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
} __attribute__((packed));
```

### OutdoorData (0x02)
```cpp
struct OutdoorData {
    float temperature;      // °C
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
} __attribute__((packed));
```

### SystemStatus (0x03)
```cpp
struct SystemStatus {
    uint8_t indoor_active;
    uint8_t outdoor_active;
    unsigned long indoor_last_seen;   // Sekunden
    unsigned long outdoor_last_seen;  // Sekunden
    uint16_t esp_now_packets;
    uint8_t wifi_channel;
} __attribute__((packed));
```

## Erweiterte Konfiguration

### NTP Zeitzone anpassen

In CYD_I2C_Master.ino:
```cpp
#define GMT_OFFSET_SEC 3600           // GMT+1 für Deutschland/Schweiz
#define DAYLIGHT_OFFSET_SEC 3600      // Sommerzeit
```

### Display-Rotation ändern

In CYD_I2C_Master.ino:
```cpp
#define DISPLAY_ROTATION 3  // 0=0°, 1=90°, 2=180°, 3=270°
```

### Sensor-Timeouts anpassen

In ESP32-C3_Bridge_Slave.ino:
```cpp
if (systemStatus.indoor_last_seen > 300) {   // 5 Minuten
    systemStatus.indoor_active = 0;
}
if (systemStatus.outdoor_last_seen > 1200) {  // 20 Minuten
    systemStatus.outdoor_active = 0;
}
```

## Quellen

- [Arduino ESP32 I2C Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html)
- [Stack Overflow: ESP32-C3 I2C Pre-Initialization](https://stackoverflow.com/questions/77020126/arduino-ide-esp32-c3-hidden-i2c-initialisation-hidden-wire-begin)

## Fazit

✅ **Funktionierendes ESP-NOW zu I2C Bridge System**

**Kernerkenntnisse:**
- ESP32-C3 kann WiFi/ESP-NOW UND I2C Slave gleichzeitig
- Korrekte Initialisierung: `Wire.begin(address)` OHNE Pin-Parameter
- I2CSensorBridge Library ermöglicht elegante Multi-Struct Kommunikation
- Outdoor-Timeout muss an Sende-Intervall angepasst werden (20 Min)
