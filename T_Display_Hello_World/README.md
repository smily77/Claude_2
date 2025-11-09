# LILYGO T-Display - Hallo Welt

Ein einfaches Hello World Programm für das LILYGO T-Display (ESP32 mit integriertem 135x240 Pixel TFT-Display).

**Verwendet LovyanGFX** - Keine Änderung von Bibliotheks-Dateien nötig! Alle Einstellungen sind in `config.h` im Projektordner.

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

### 3. LovyanGFX Bibliothek installieren

1. Gehe zu: `Sketch` → `Bibliothek einbinden` → `Bibliotheken verwalten`
2. Suche nach "LovyanGFX"
3. Installiere "LovyanGFX by lovyan03"

**Das war's!** Keine Konfigurationsdateien in der Bibliothek ändern - alle Einstellungen sind in der `config.h` im Projektordner!

### 4. Board-Einstellungen in Arduino IDE

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
   - Die `config.h` muss im gleichen Ordner sein (enthält Display-Konfiguration)

2. Verbinde das T-Display über USB-C mit deinem Computer

3. Wähle die richtigen Board-Einstellungen (siehe oben)

4. Klicke auf den Upload-Button (→)

5. Warte bis "Hochladen abgeschlossen" erscheint

## Projekt-Dateien

```
T_Display_Hello_World/
├── T_Display_Hello_World.ino  # Hauptprogramm
└── config.h                   # Display-Konfiguration (LovyanGFX)
```

Beide Dateien müssen im gleichen Ordner sein!

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

### Display bleibt schwarz

- **Problem**: Backlight nicht aktiviert oder falsche Pin-Konfiguration
- **Lösung**:
  - Überprüfe die `config.h` - Backlight Pin sollte 4 sein
  - Stelle sicher, dass `tft.setBrightness(255);` im Code ist
  - Versuche das Programm neu zu starten (Reset-Button)

### Display zeigt Müll oder falsche Farben

- **Problem**: Offset-Werte oder Konfiguration falsch
- **Lösung**: Überprüfe in `config.h`:
  - `cfg.offset_x = 52;`
  - `cfg.offset_y = 40;`
  - `cfg.invert = true;`

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

## Vorteile von LovyanGFX

- **Keine Library-Änderungen**: Alle Einstellungen im Projekt
- **Schneller**: Optimierte Grafikausgabe
- **Flexibler**: Einfache Anpassung verschiedener Displays
- **Bessere API**: Mehr Funktionen als TFT_eSPI
- **Hardware SPI**: Volle Geschwindigkeit

## Ressourcen

- [LILYGO T-Display GitHub](https://github.com/Xinyuan-LilyGO/TTGO-T-Display)
- [LovyanGFX Bibliothek](https://github.com/lovyan03/LovyanGFX)
- [LovyanGFX Dokumentation](https://github.com/lovyan03/LovyanGFX/blob/master/doc/Panel_config.md)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)

## Lizenz

Dieses Projekt steht unter freier Verwendung für Lern- und Testzwecke.
