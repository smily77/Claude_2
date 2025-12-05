# I2C Test Programme für ESP32 und ESP32-C3

## Übersicht

Dieses Verzeichnis enthält I2C-Test-Programme zur Diagnose und Validierung der I2C-Kommunikation zwischen ESP32 Dev Board (Master) und ESP32-C3 (Slave).

### Hardware Setup

**ESP32 Dev Board (Master):**
- SDA: Pin 22
- SCL: Pin 27

**ESP32-C3 (Slave):**
- SDA: Pin 8 (Default)
- SCL: Pin 9 (Default)

**Wichtig:** Pull-up Widerstände (4.7kΩ) auf SDA und SCL erforderlich!

## Programme

### 1. ESP32_I2C_Scanner
**Zweck:** Scannt den I2C-Bus (0x01-0x7F) um verbundene Geräte zu finden

**Verwendung:**
```
1. Auf ESP32 Dev Board flashen
2. Serial Monitor öffnen (115200 Baud)
3. Prüfen, welche Geräte gefunden werden
```

**Was es zeigt:**
- Liste aller gefundenen I2C-Adressen
- Hilfreich zur Diagnose von Hardware-Problemen

### 2. ESP32_I2C_Master
**Zweck:** Master Test-Programm

**Features:**
- Sendet alle 2 Sekunden einen Zähler-Wert an Slave
- Liest 4 Bytes vom Slave zurück
- Detaillierte Fehler-Diagnose mit Fehlercodes

### 3. ESP32C3_I2C_Slave_Alt ⭐ EMPFOHLEN
**Zweck:** Funktionierendes Slave-Programm für ESP32-C3

**Features:**
- ✅ Verwendet `Wire.begin(address)` OHNE Pin-Parameter
- Zeigt Chip-Informationen beim Start
- Detailliertes Logging aller I2C-Events
- Status-Updates alle 10 Sekunden

**Wichtig:** Dieses Programm funktioniert auch MIT WiFi!

### 4. ESP32C3_I2C_Slave_WiFiTest
**Zweck:** Beweis-Test dass I2C und WiFi zusammen funktionieren

**Was es zeigt:**
- WiFi und I2C können gleichzeitig auf ESP32-C3 laufen
- Verwendet die gleiche Initialisierung wie Slave_Alt

## ⚠️ KRITISCHE ERKENNTNIS: ESP32-C3 I2C Slave Initialisierung

### Das Problem

ESP32-C3 pre-initialisiert I2C automatisch mit den Default-Pins (8/9) **VOR** `setup()`.

Wenn man dann `Wire.begin(sda, scl, address)` **MIT** Pin-Parametern aufruft, schlägt die Initialisierung fehl!

### Die Lösung ✅

**FALSCH (funktioniert NICHT):**
```cpp
Wire.begin(8, 9, 0x08);  // ❌ Schlägt fehl nach WiFi-Init!
```

**RICHTIG (funktioniert):**
```cpp
Wire.begin((uint8_t)0x08);  // ✅ Verwendet automatisch Default-Pins 8/9
```

### Mit WiFi/ESP-NOW

ESP32-C3 kann I2C Slave UND WiFi/ESP-NOW gleichzeitig verwenden, **aber nur wenn die richtige Initialisierung verwendet wird:**

```cpp
void setup() {
    // 1. WiFi/ESP-NOW ZUERST
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_now_init();

    delay(500);  // WiFi stabilisieren lassen

    // 2. I2C DANACH - OHNE Pin-Parameter!
    Wire.begin((uint8_t)SLAVE_ADDRESS);  // ✅ Funktioniert!
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}
```

## Test-Ablauf

### Schritt 1: Hardware prüfen
```
1. ESP32_I2C_Scanner auf ESP32 Dev Board flashen
2. Serial Monitor öffnen (115200 Baud)
3. Prüfen ob andere I2C-Geräte gefunden werden
```

### Schritt 2: Slave testen (OHNE WiFi)
```
1. ESP32C3_I2C_Slave_Alt auf ESP32-C3 flashen
2. Serial Monitor öffnen (115200 Baud)
3. Prüfen ob "✓ I2C Slave ready!" erscheint
```

### Schritt 3: Scanner erneut ausführen
```
1. ESP32_I2C_Scanner sollte jetzt 0x08 finden
2. Wenn JA: ✓ Hardware OK, weiter zu Schritt 4
3. Wenn NEIN: ✗ Hardware-Problem prüfen
```

### Schritt 4: Kommunikation testen
```
1. ESP32_I2C_Master auf ESP32 flashen
2. ESP32C3_I2C_Slave_Alt auf ESP32-C3 lassen
3. Beide Serial Monitors öffnen
4. Beobachten:
   - Master: "✓ Write successful"
   - Slave: "← RECEIVE #X"
   - Master: "✓ Received"
   - Slave: "→ REQUEST #X"
```

### Schritt 5: Mit WiFi testen
```
1. ESP32C3_I2C_Slave_WiFiTest auf ESP32-C3 flashen
2. ESP32_I2C_Scanner sollte WEITERHIN 0x08 finden
3. Beweis: WiFi interferiert NICHT mit I2C!
```

## Troubleshooting

### Problem: "NACK on transmit of address" (Error 2 oder 4)

**Ursache:** Slave antwortet nicht auf I2C-Anfragen

**Checkliste:**
1. ✅ Arduino ESP32 Core Version >= 2.0.1?
2. ✅ `Wire.begin(address)` OHNE Pins verwendet?
3. ✅ Pull-up Widerstände (4.7kΩ) vorhanden?
4. ✅ GND zwischen Master und Slave verbunden?
5. ✅ Kabel korrekt (SDA↔SDA, SCL↔SCL)?

### Problem: Scanner findet Slave nur OHNE WiFi

**Lösung:** `Wire.begin(address)` statt `Wire.begin(sda, scl, address)` verwenden!

Siehe ESP32C3_I2C_Slave_WiFiTest für funktionierendes Beispiel.

## Hardware-Checkliste

- [ ] Pull-up Widerstände 4.7kΩ auf SDA
- [ ] Pull-up Widerstände 4.7kΩ auf SCL
- [ ] GND zwischen ESP32 und ESP32-C3 verbunden
- [ ] SDA-Verbindung: ESP32 Pin 22 ↔ ESP32-C3 Pin 8
- [ ] SCL-Verbindung: ESP32 Pin 27 ↔ ESP32-C3 Pin 9
- [ ] Beide Geräte mit 3.3V versorgt

## Quellen

- [Arduino ESP32 I2C Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html)
- [Stack Overflow: ESP32-C3 I2C Pre-Initialization](https://stackoverflow.com/questions/77020126/arduino-ide-esp32-c3-hidden-i2c-initialisation-hidden-wire-begin)
- [Random Nerd Tutorials - ESP32 I2C](https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/)

## Fazit

✅ **ESP32-C3 I2C Slave funktioniert zuverlässig mit WiFi/ESP-NOW**

**Voraussetzung:** Korrekte Initialisierung mit `Wire.begin(address)` OHNE Pin-Parameter!
