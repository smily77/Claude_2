# Desk_Top_Widget_III - Verdrahtung

## Hardware-Komponenten
- **ESP32-C3 Super Mini** (Mikrocontroller)
- **A7670E LTE-Modul** (SIMCom Cat-1, eingebautes Mobilfunkmodem)
- **Adafruit ST7735 1.8" TFT Display** (160x128, SPI)
- **BMP180** Druck-/Temperatursensor (I2C)
- **LDR** (Fotowiderstand) fuer automatische Helligkeitssteuerung
- **SIM-Karte** (Nano oder Micro, je nach A7670E-Board)

---

## Pin-Belegung ESP32-C3 Super Mini

### SPI -> TFT Display (Adafruit ST7735 1.8")

| ESP32-C3 GPIO | TFT Pin    | Funktion        |
|---------------|------------|-----------------|
| GPIO 6        | SDA / MOSI | SPI Daten       |
| GPIO 4        | SCK / CLK  | SPI Takt        |
| GPIO 7        | CS         | Chip Select     |
| GPIO 0        | DC / RS    | Data / Command  |
| GPIO 1        | RST        | Reset           |
| GPIO 5        | LED        | Hintergrundbeleuchtung (PWM) |
| 3.3V          | VCC        | Versorgung      |
| GND           | GND        | Masse           |

### I2C -> BMP180 Sensor

| ESP32-C3 GPIO | BMP180 Pin | Funktion    |
|---------------|------------|-------------|
| GPIO 8        | SDA        | I2C Daten   |
| GPIO 9        | SCL        | I2C Takt    |
| 3.3V          | VCC        | Versorgung  |
| GND           | GND        | Masse       |

**Hinweis:** 4.7k Ohm Pull-up Widerstaende an SDA und SCL nach 3.3V
(sind auf den meisten BMP180-Breakout-Boards bereits vorhanden).

### UART -> A7670E LTE-Modul

| ESP32-C3 GPIO | A7670E Pin | Funktion               |
|---------------|------------|------------------------|
| GPIO 21       | RXD        | ESP32 TX -> A7670E RX  |
| GPIO 20       | TXD        | ESP32 RX <- A7670E TX  |
| GPIO 10       | PWRKEY     | Ein-/Ausschalten       |
| GPIO 2        | RST        | Reset (Active Low)     |
| GND           | GND        | Gemeinsame Masse       |

**WICHTIG - Stromversorgung A7670E:**
- Das A7670E-Modul wird **NICHT** ueber den ESP32-C3 versorgt!
- Eigene Stromquelle noetig: 3.7-4.2V (LiPo) oder 5V ueber Board-LDO
- Spitzenstrom bis **2A** bei LTE-Verbindungsaufbau
- USB-Versorgung des ESP32-C3 reicht **nicht** fuer beide!

**WICHTIG - UART Pegel:**
- A7670E UART arbeitet nativ mit **1.8V** Logikpegel
- Die meisten A7670E-Breakout-Boards haben einen Level-Shifter auf **3.3V**
- Falls kein Level-Shifter vorhanden: bidirektionalen 1.8V <-> 3.3V
  Level-Shifter verwenden (z.B. BSS138 oder TXS0102)

### ADC -> LDR (Lichtsensor)

| ESP32-C3 GPIO | Verbindung | Funktion          |
|---------------|------------|-------------------|
| GPIO 3        | LDR        | Analoger Eingang  |

**Schaltung (Spannungsteiler):**
```
3.3V ---[LDR]--- GPIO 3 ---[10k Ohm]--- GND
```
- Bei Helligkeit: LDR niederohmig -> hohe Spannung an GPIO 3 -> hoher ADC-Wert
- Bei Dunkelheit: LDR hochohmig -> niedrige Spannung an GPIO 3 -> niedriger ADC-Wert
- ESP32-C3 ADC: 12-bit (0-4095), Eingangsbereich 0-3.3V

**Hinweis:** Die LDR-Werte muessen ggf. an den konkreten LDR-Typ angepasst
werden. Standardwerte im Code: `LDR_HELL=1600`, `LDR_DUNKEL=4095`.

---

## Verdrahtungsplan (ASCII-Darstellung)

```
                ESP32-C3 Super Mini
                +------------------+
                |              3.3V|---+--- VCC (TFT) --- VCC (BMP180)
                |               GND|---+--- GND (alle Komponenten)
                |                  |
  TFT MOSI <---|GPIO 6            |
  TFT SCK  <---|GPIO 4            |
  TFT CS   <---|GPIO 7            |
  TFT DC   <---|GPIO 0            |
  TFT RST  <---|GPIO 1            |
  TFT LED  <---|GPIO 5   (PWM)    |
                |                  |
  BMP SDA  <-->|GPIO 8   (I2C)    |
  BMP SCL  <-->|GPIO 9   (I2C)    |
                |                  |
  A7670 RXD <--|GPIO 21  (UART TX)|
  A7670 TXD -->|GPIO 20  (UART RX)|
  A7670 PWR <--|GPIO 10           |
  A7670 RST <--|GPIO 2            |
                |                  |
  LDR -------->|GPIO 3   (ADC)    |
                +------------------+


                A7670E LTE-Modul
                +------------------+
  ESP GPIO 21-->|RXD               |
  ESP GPIO 20<--|TXD               |
  ESP GPIO 10-->|PWRKEY            |
  ESP GPIO 2 -->|RST               |
                |                  |
                |         VCC (4V) |<--- LiPo 3.7V oder 5V Netzteil
                |              GND |<--- Gemeinsame Masse mit ESP32
                |                  |
                |         SIM-Slot |<--- Nano/Micro SIM (PIN deaktiviert!)
                |          Antenne |<--- LTE-Antenne (SMA/u.FL)
                +------------------+
```

---

## Stromversorgung

### ESP32-C3 Super Mini
- USB-C Anschluss: 5V
- Oder direkt 3.3V an den 3.3V-Pin (bei externer Versorgung)
- Stromverbrauch: ca. 50-100mA

### A7670E LTE-Modul
- **Separate Versorgung erforderlich!**
- Eingangsspannung: 3.4V - 4.2V (direkt) oder 5V (ueber Board-LDO)
- Durchschnittlicher Strom: ca. 100-300mA
- Spitzenstrom (LTE TX): bis **2A**
- Empfehlung: Stabilisiertes 5V/2A Netzteil oder LiPo-Akku 3.7V

### Gemeinsame Masse
- **GND aller Komponenten muessen verbunden sein!**
- ESP32-C3 GND, A7670E GND, TFT GND, BMP180 GND, LDR GND

---

## SIM-Karten Konfiguration

1. **PIN-Sperre DEAKTIVIEREN** (vorher in einem Handy die PIN entfernen)
2. **Datentarif** muss aktiv sein (SMS/Telefonie nicht noetig)
3. **APN** im Code anpassen (`Desk_Top_Widget_III.ino`):
   ```cpp
   const char* APN = "internet";  // An Mobilfunkanbieter anpassen
   ```
   Beispiele:
   - Swisscom: `"gprs.swisscom.ch"`
   - Sunrise: `"internet"`
   - Salt: `"internet"`
   - Telekom DE: `"internet.telekom"`
   - Vodafone DE: `"web.vodafone.de"`
   - O2 DE: `"internet"`
   - A1 AT: `"A1.net"`

---

## Arduino IDE Einstellungen

- **Board:** ESP32C3 Dev Module
- **USB CDC On Boot:** Enabled
- **CPU Frequency:** 160MHz
- **Flash Size:** 4MB
- **Partition Scheme:** Default 4MB with spiffs
- **Upload Speed:** 921600

### Benoetigte Bibliotheken
- TimeLib (Paul Stoffregen)
- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- SparkFun BMP180 (SFE_BMP180)
- Ticker (im ESP32 Core enthalten)

---

## Hinweise zur Inbetriebnahme

1. Alle Verbindungen pruefen, besonders gemeinsame Masse
2. A7670E separat mit Strom versorgen (NICHT ueber ESP32!)
3. SIM-Karte einlegen (PIN deaktiviert)
4. LTE-Antenne anschliessen
5. APN im Code anpassen
6. Programm hochladen (ESP32-C3 in Boot-Modus: BOOT-Taste halten)
7. Serial Monitor (115200 Baud) fuer Debug-Ausgaben oeffnen
8. Beim ersten Start wird die LTE-Initialisierung auf dem TFT angezeigt
9. Nach erfolgreicher Verbindung: NTP-Sync, Wechselkurse, dann Hauptanzeige
