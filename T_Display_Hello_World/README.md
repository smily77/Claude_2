# LILYGO T-Display - Hallo Welt

Ein einfaches Hello World Programm für das LILYGO T-Display (ESP32 mit integriertem 135x240 Pixel TFT-Display).

## Hardware

- **LILYGO T-Display** (ESP32)
- Display: ST7789 Controller, 135x240 Pixel
- 2 Buttons (GPIO 0 und GPIO 35)
- USB-C Anschluss

## Voraussetzungen

### 1. Arduino IDE installieren

- Arduino IDE Version 1.8.x oder 2.x
- Download von [arduino.cc](https://www.arduino.cc/en/software)

### 2. ESP32 Board Support installieren

1. Arduino IDE öffnen
2. Gehe zu: `Datei` → `Voreinstellungen`
3. Füge diese URL bei "Zusätzliche Boardverwalter-URLs" hinzu:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Gehe zu: `Werkzeuge` → `Board` → `Boardverwalter`
5. Suche nach "ESP32" und installiere "esp32 by Espressif Systems"

### 3. TFT_eSPI Bibliothek installieren

1. Gehe zu: `Sketch` → `Bibliothek einbinden` → `Bibliotheken verwalten`
2. Suche nach "TFT_eSPI"
3. Installiere "TFT_eSPI by Bodmer"

### 4. TFT_eSPI für T-Display konfigurieren

**WICHTIG:** Die TFT_eSPI Bibliothek muss für das T-Display konfiguriert werden!

#### Option A: User_Setup.h bearbeiten (empfohlen)

1. Finde den Speicherort der TFT_eSPI Bibliothek:
   - **Windows**: `C:\Users\[Username]\Documents\Arduino\libraries\TFT_eSPI\`
   - **Mac**: `~/Documents/Arduino/libraries/TFT_eSPI/`
   - **Linux**: `~/Arduino/libraries/TFT_eSPI/`

2. Öffne die Datei `User_Setup.h` in einem Texteditor

3. Kommentiere alle aktiven Display-Treiber aus und aktiviere nur:
   ```cpp
   #define ST7789_DRIVER
   ```

4. Setze die Display-Größe:
   ```cpp
   #define TFT_WIDTH  135
   #define TFT_HEIGHT 240
   ```

5. Konfiguriere die Pins (füge hinzu oder ändere):
   ```cpp
   #define TFT_MOSI 19
   #define TFT_SCLK 18
   #define TFT_CS   5
   #define TFT_DC   16
   #define TFT_RST  23
   #define TFT_BL   4  // Backlight
   ```

#### Option B: Setup-Datei auswählen

Einige TFT_eSPI Versionen haben vordefinierte Setups:

1. Öffne `User_Setup_Select.h`
2. Kommentiere die Standard-Zeile aus
3. Suche und aktiviere: `#include <User_Setups/Setup25_TTGO_T_Display.h>`

### 5. Board-Einstellungen in Arduino IDE

- **Board**: `Werkzeuge` → `Board` → `ESP32 Arduino` → `ESP32 Dev Module`
- **Upload Speed**: `921600` (oder `115200` bei Problemen)
- **Flash Frequency**: `80MHz`
- **Flash Mode**: `QIO`
- **Flash Size**: `4MB (32Mb)`
- **Partition Scheme**: `Default 4MB with spiffs`
- **Port**: Wähle den entsprechenden COM-Port

## Installation und Upload

1. Öffne die Datei `T_Display_Hello_World.ino` in der Arduino IDE
   - **Wichtig**: Die .ino-Datei muss im gleichnamigen Ordner `T_Display_Hello_World/` liegen

2. Verbinde das T-Display über USB-C mit deinem Computer

3. Wähle die richtigen Board-Einstellungen (siehe oben)

4. Klicke auf den Upload-Button (→)

5. Warte bis "Hochladen abgeschlossen" erscheint

## Was macht das Programm?

- Zeigt "Hallo Welt!" auf dem Display an
- Verwendet farbige Bereiche und verschiedene Textgrößen
- Implementiert einen Zähler, der mit den Buttons gesteuert wird:
  - **Linker Button** (GPIO 0): Zähler verringern
  - **Rechter Button** (GPIO 35): Zähler erhöhen
- Der aktuelle Zählerwert wird im Header-Bereich angezeigt
- Ausgabe über den seriellen Monitor (115200 Baud)

## Display-Layout

```
┌─────────────────────────────┐
│  Zaehler: 0     (Header)    │ <- Blau, interaktiv
├─────────────────────────────┤
│                             │
│   Hallo Welt!               │ <- Cyan, groß
│                             │
│   LILYGO T-Display          │ <- Gelb
│                             │
│   ESP32 mit ST7789          │ <- Grün
│   Druecke Buttons zum...    │ <- Weiß
└─────────────────────────────┘
```

## Fehlerbehebung

### Display bleibt schwarz oder zeigt nur Müll

- **Problem**: TFT_eSPI ist nicht korrekt konfiguriert
- **Lösung**: Überprüfe die `User_Setup.h` Konfiguration (siehe oben)
- Stelle sicher, dass `ST7789_DRIVER` aktiviert ist
- Überprüfe die Pin-Definitionen

### Port wird nicht erkannt

- Installiere den USB-Treiber:
  - **Windows**: CP210x USB to UART Bridge Driver
  - **Mac**: Normalerweise nicht nötig
  - **Linux**: Füge deinen User zur `dialout` Gruppe hinzu: `sudo usermod -a -G dialout $USER`

### Upload schlägt fehl

- Versuche eine niedrigere Upload-Geschwindigkeit (115200)
- Halte den linken Button (GPIO 0) während des Upload-Starts gedrückt
- Überprüfe das USB-Kabel (muss Datenübertragung unterstützen)

### Buttons funktionieren nicht

- Die Buttons sind "active LOW" (gedrückt = LOW)
- Überprüfe, ob die richtigen GPIOs verwendet werden (0 und 35)

## Serielle Ausgabe

Öffne den seriellen Monitor (115200 Baud) um die Debug-Ausgaben zu sehen:
```
T-Display Hello World startet...
Display bereit!
Counter: 1
Counter: 2
...
```

## Display-Rotation

Das Display kann in verschiedene Richtungen gedreht werden:
- `tft.setRotation(0)`: Portrait (135x240)
- `tft.setRotation(1)`: Landscape (240x135) <- Standard
- `tft.setRotation(2)`: Portrait umgekehrt
- `tft.setRotation(3)`: Landscape umgekehrt

## Nächste Schritte

Erweitere das Programm mit:
- WiFi-Verbindung und Webserver
- Sensordaten anzeigen (Temperatur, etc.)
- Grafiken und Animationen
- Touch-Buttons auf dem Display (wenn vorhanden)
- Deep Sleep für Batteriebetrieb

## Pin-Referenz T-Display

```
Display Pins:
- TFT_MOSI:  GPIO 19
- TFT_SCLK:  GPIO 18
- TFT_CS:    GPIO 5
- TFT_DC:    GPIO 16
- TFT_RST:   GPIO 23
- TFT_BL:    GPIO 4 (Backlight)

Buttons:
- Button 1:  GPIO 0 (links)
- Button 2:  GPIO 35 (rechts)

I2C (falls vorhanden):
- SDA:       GPIO 21
- SCL:       GPIO 22
```

## Ressourcen

- [LILYGO T-Display GitHub](https://github.com/Xinyuan-LilyGO/TTGO-T-Display)
- [TFT_eSPI Bibliothek](https://github.com/Bodmer/TFT_eSPI)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)

## Lizenz

Dieses Projekt steht unter freier Verwendung für Lern- und Testzwecke.
