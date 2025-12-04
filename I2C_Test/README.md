# I2C Test Programme für ESP32

## Übersicht

Dieses Verzeichnis enthält I2C-Test-Programme für ESP32 Dev Board und ESP32-C3.

### Hardware Setup

**ESP32 Dev Board (Master):**
- SDA: Pin 22
- SCL: Pin 27

**ESP32-C3 (Slave):**
- SDA: Pin 8
- SCL: Pin 9

**Wichtig:** Pull-up Widerstände (4.7kΩ) auf SDA und SCL erforderlich!

## Programme

### 1. ESP32_I2C_Scanner
**Zweck:** Scannt den I2C-Bus, um verbundene Geräte zu finden

**Verwendung:**
1. Auf ESP32 Dev Board flashen
2. Serial Monitor öffnen (115200 Baud)
3. Prüfen, ob Slave bei Adresse 0x08 sichtbar ist

**Was es zeigt:**
- ✓ Wenn Slave sichtbar: Hardware-Verbindung ist OK
- ✗ Wenn Slave nicht sichtbar: Problem mit Hardware oder Slave-Software

### 2. ESP32_I2C_Master
**Zweck:** Master-Programm, sendet Daten an Slave und liest Antworten

**Features:**
- Sendet alle 2 Sekunden einen Zähler-Wert
- Liest 4 Bytes vom Slave zurück
- Detaillierte Fehler-Diagnose

### 3. ESP32C3_I2C_Slave
**Zweck:** Original Slave-Programm

### 4. ESP32C3_I2C_Slave_Alt
**Zweck:** Alternative Slave-Implementierung mit mehr Debugging

**Features:**
- Zeigt Chip-Informationen beim Start
- Detailliertes Logging aller I2C-Events
- Status-Updates alle 10 Sekunden

## Troubleshooting

### Problem: "NACK on transmit of address" (Error 2 oder 4)

**Mögliche Ursachen:**

#### 1. ESP32-C3 I2C Slave wird nicht initialisiert
**Lösung:** Prüfe Arduino ESP32 Core Version
- Tools → Board → Boards Manager → "esp32"
- **Mindestversion: 2.0.1** (für I2C Slave Support)
- Empfohlen: 2.0.14 oder neuer

#### 2. Wire.begin() Syntax für ESP32-C3
**Verschiedene Varianten ausprobieren:**

```cpp
// Variante 1 (Standard)
Wire.begin(SLAVE_ADDRESS);

// Variante 2 (mit Pins)
Wire.begin(SDA_PIN, SCL_PIN, SLAVE_ADDRESS);

// Variante 3 (nur Pins, dann Callbacks)
Wire.setPins(SDA_PIN, SCL_PIN);
Wire.begin(SLAVE_ADDRESS);
```

#### 3. Hardware-Probleme
**Checkliste:**
- [ ] Pull-up Widerstände vorhanden? (4.7kΩ auf SDA und SCL)
- [ ] GND zwischen ESP32 und ESP32-C3 verbunden?
- [ ] Kabel-Verbindungen korrekt?
  - ESP32 Pin 22 → ESP32-C3 Pin 8 (SDA)
  - ESP32 Pin 27 → ESP32-C3 Pin 9 (SCL)
- [ ] Versorgungsspannung stabil? (beide 3.3V)

#### 4. ESP32-C3 I2C Slave Mode Bug
**Bekanntes Problem:** Manche ESP32-C3 Boards/Versionen haben Probleme mit I2C Slave Mode

**Lösung:** ESP-IDF I2C Slave API verwenden (außerhalb von Arduino)

## Test-Ablauf

### Schritt 1: Hardware prüfen
1. **ESP32_I2C_Scanner** auf ESP32 Dev Board flashen
2. Serial Monitor öffnen
3. Prüfen ob andere I2C-Geräte gefunden werden (z.B. Display)
4. ESP32-C3 mit Strom versorgen (aber noch kein Programm nötig)
5. Prüfen ob Scanner überhaupt Geräte findet

### Schritt 2: Slave testen
1. **ESP32C3_I2C_Slave_Alt** auf ESP32-C3 flashen
2. Serial Monitor öffnen (115200 Baud)
3. Chip-Informationen prüfen:
   - Sollte zeigen: "Chip Model: ESP32-C3"
   - SDK Version notieren
4. Prüfen ob "✓ I2C Slave ready!" erscheint

### Schritt 3: Scanner erneut ausführen
1. **ESP32_I2C_Scanner** sollte jetzt 0x08 finden
2. Wenn JA: ✓ Hardware OK, weiter zu Schritt 4
3. Wenn NEIN: ✗ ESP32-C3 I2C Slave Mode funktioniert nicht

### Schritt 4: Kommunikation testen
1. **ESP32_I2C_Master** auf ESP32 flashen
2. **ESP32C3_I2C_Slave_Alt** auf ESP32-C3 lassen
3. Beide Serial Monitors öffnen
4. Beobachten:
   - Master sollte: "✓ Write successful" zeigen
   - Slave sollte: "← RECEIVE" zeigen
   - Master sollte: "✓ Received" zeigen
   - Slave sollte: "→ REQUEST" zeigen

## Bekannte Einschränkungen

### ESP32-C3 I2C Slave Support

Laut Arduino-ESP32 Dokumentation:
- I2C Slave wird seit Version 2.0.1 unterstützt
- Einfachere Implementierung als beim Original ESP32
- Keine `slaveWrite()` Funktion nötig

**Aber:** In der Praxis gibt es Berichte über Probleme mit I2C Slave Mode auf ESP32-C3 in bestimmten Konstellationen.

## Quellen

- [Arduino ESP32 I2C Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html)
- [ESP32 I2C Slave Library (veraltet, aber informativ)](https://github.com/gutierrezps/ESP32_I2C_Slave)
- [Random Nerd Tutorials - ESP32 I2C](https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/)

## Alternativen

Falls ESP32-C3 I2C Slave nicht funktioniert:

1. **Software I2C Emulation** verwenden
2. **Andere Kommunikations-Protokolle** (UART, SPI)
3. **ESP-IDF statt Arduino** verwenden (besserer I2C Support)
4. **Anderes Board als Slave** (ESP32, ESP32-S2)
