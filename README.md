# M5Stack Core2 - Hallo Welt

Ein einfaches Arduino-Programm für den M5Stack Core2, das "Hallo Welt" auf dem Display anzeigt.

## Voraussetzungen

### Arduino IDE Setup

1. **Arduino IDE installieren** (Version 1.8.x oder 2.x)
   - Download von [arduino.cc](https://www.arduino.cc/en/software)

2. **ESP32 Board Support installieren**
   - Arduino IDE öffnen
   - Gehe zu: `Datei` → `Voreinstellungen`
   - Füge diese URL bei "Zusätzliche Boardverwalter-URLs" hinzu:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Gehe zu: `Werkzeuge` → `Board` → `Boardverwalter`
   - Suche nach "ESP32" und installiere "esp32 by Espressif Systems"

3. **M5Core2 Bibliothek installieren**
   - Gehe zu: `Sketch` → `Bibliothek einbinden` → `Bibliotheken verwalten`
   - Suche nach "M5Core2"
   - Installiere "M5Core2 by M5Stack"

### Hardware

- M5Stack Core2
- USB-C Kabel zum Programmieren und mit Strom versorgen

## Installation

1. Öffne `M5Stack_Hello_World.ino` in der Arduino IDE

2. **Board-Einstellungen konfigurieren:**
   - `Werkzeuge` → `Board` → `ESP32 Arduino` → `M5Stack-Core2`
   - `Werkzeuge` → `Upload Speed` → `921600` (oder `115200` bei Problemen)
   - `Werkzeuge` → `Port` → Wähle den entsprechenden COM-Port

3. **Programm hochladen:**
   - Verbinde den M5Stack Core2 mit deinem Computer
   - Klicke auf den Upload-Button (→) in der Arduino IDE
   - Warte bis "Hochladen abgeschlossen" erscheint

## Was macht das Programm?

- Initialisiert den M5Stack Core2
- Setzt den Hintergrund auf schwarz
- Zeigt "Hallo Welt!" in weißer Schrift an
- Zeigt "M5Stack Core2" in grüner Schrift an

## Fehlerbehebung

### Port wird nicht erkannt
- Stelle sicher, dass der USB-Treiber installiert ist (CP210x oder CH9102)
- Windows: Geräte-Manager prüfen
- Mac/Linux: Terminal-Befehl `ls /dev/tty.*` oder `ls /dev/ttyUSB*`

### Upload schlägt fehl
- Versuche eine niedrigere Upload-Geschwindigkeit (115200)
- Drücke den Reset-Button am M5Stack während des Uploads
- Überprüfe das USB-Kabel (manche Kabel unterstützen nur Laden, nicht Daten)

### Display bleibt schwarz
- Überprüfe, ob das Programm erfolgreich hochgeladen wurde
- Drücke den Reset-Button am M5Stack
- Überprüfe die Batterieladung

## Nächste Schritte

Du kannst das Programm erweitern mit:
- Touch-Button-Funktionen
- Sensordaten anzeigen (IMU, etc.)
- Grafiken und Animationen
- WiFi-Verbindungen

## Lizenz

Dieses Projekt steht unter freier Verwendung für Lern- und Testzwecke.
