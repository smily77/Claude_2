# M5Stack Core Ink - Daily Reminder

Ein Arduino-Programm für das M5Stack Core Ink Development Kit, das Sie alle 2 Tage an eine wiederkehrende Aufgabe erinnert.

## Features

- **Wechselnde Yes/No Anzeige**: Zeigt jeden 2. Tag ein anderes Icon (Yes oder No)
- **Datums- und Zeitanzeige**: Aktuelles Datum und Uhrzeit auf dem E-Ink Display
- **Batteriestand**: Zeigt die verbleibende Batterieladung in Prozent und Volt
- **Automatische Zeitsynchronisation**: Beim ersten Start wird die Zeit vom Internet (NTP) synchronisiert (Zeitzone Zürich)
- **Ultra-Low-Power**: Geht nach der Anzeige in Deep Sleep und wacht um Mitternacht wieder auf
- **Test-Modus**: Für schnelles Testen wechselt die Anzeige jede gerade Minute (0, 2, 4, ...)

## Hardware

- M5Stack Core Ink Development Kit
- 1.54" E-Ink Display (200x200 Pixel)
- ESP32-PICO-D4
- BM8563 RTC (Real Time Clock)
- USB-C zum Programmieren und Laden

**Kompatibilität:**
- ✅ ESP32 Core 3.x (empfohlen)
- ✅ M5GFX Bibliothek
- ✅ Direkter I2C-Zugriff auf BM8563 RTC

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

3. **M5GFX Bibliothek installieren**
   - Gehe zu: `Sketch` → `Bibliothek einbinden` → `Bibliotheken verwalten`
   - Suche nach "M5GFX"
   - Installiere "M5GFX by M5Stack"
   - **WICHTIG**: Diese Version ist kompatibel mit ESP32 Core 3.x

## Installation

1. **Projekt klonen oder herunterladen**
   ```bash
   git clone <repository-url>
   cd Daily_Reminder
   ```

2. **WiFi-Zugangsdaten konfigurieren**
   - Öffne die Datei `Credentials.h`
   - Ersetze die Platzhalter mit deinen echten WiFi-Zugangsdaten:
     ```cpp
     const char* ssid = "DeinWiFiName";
     const char* password = "DeinWiFiPasswort";
     ```
   - **WICHTIG**: Diese Datei sollte NICHT in ein öffentliches Repository hochgeladen werden!

3. **Board-Einstellungen in Arduino IDE**
   - Öffne `Daily_Reminder.ino` in der Arduino IDE
   - `Werkzeuge` → `Board` → `ESP32 Arduino` → `M5Stack-CoreInk`
   - `Werkzeuge` → `Upload Speed` → `1500000`
   - `Werkzeuge` → `Port` → Wähle den entsprechenden COM-Port

4. **Programm hochladen**
   - Verbinde das M5Stack Core Ink mit deinem Computer
   - Klicke auf den Upload-Button (→) in der Arduino IDE
   - Warte bis "Hochladen abgeschlossen" erscheint

## Verwendung

### Erster Start

Beim ersten Start nach dem Upload:
1. Das Gerät verbindet sich automatisch mit WiFi
2. Die aktuelle Zeit wird vom Internet synchronisiert (Zeitzone Zürich)
3. Das Display zeigt:
   - Titel "Daily Reminder"
   - Aktuelles Datum
   - Aktuelle Uhrzeit
   - Yes oder No Icon (je nach aktuellem Tag)
   - Batteriestand
4. Das Gerät berechnet die Zeit bis Mitternacht und geht in Deep Sleep

### Normaler Betrieb

- Um Mitternacht wacht das Gerät automatisch auf
- Das Display wird aktualisiert (Yes/No Icon wechselt jeden 2. Tag)
- Das Gerät geht wieder in Deep Sleep bis zur nächsten Mitternacht
- Die Batterie hält durch den Deep Sleep mehrere Wochen/Monate

### Test-Modus

Für schnelleres Testen ohne bis Mitternacht warten zu müssen:

1. Öffne `Daily_Reminder.ino`
2. Suche die Zeile (normalerweise Zeile 23):
   ```cpp
   #define Test
   ```
3. Aktiviere den Test-Modus durch Entfernen der Kommentierung (falls auskommentiert)
4. Lade das Programm erneut hoch

Im Test-Modus:
- Das Display wird alle geraden Minuten aktualisiert (0, 2, 4, 6, 8, ...)
- Dies ermöglicht schnelles Testen der Funktionalität
- Das Yes/No Icon wechselt trotzdem nur jeden 2. Tag

**Für den Produktivbetrieb**: Kommentiere `#define Test` aus:
```cpp
//#define Test
```

## Funktionsweise

### Zeitsynchronisation
- Beim ersten Start wird die Zeit vom NTP-Server (pool.ntp.org) geholt
- Die Zeit wird in der internen RTC gespeichert
- Die RTC läuft auch im Deep Sleep weiter (sehr geringe Drift)
- Keine erneute Synchronisation nötig (die RTC-Genauigkeit reicht aus)

### Yes/No Logik
- Der Starttag wird beim ersten Lauf gespeichert
- Jeden 2. Tag wechselt die Anzeige:
  - Tag 0, 2, 4, 6, ... → YES Icon
  - Tag 1, 3, 5, 7, ... → NO Icon
- Die Berechnung berücksichtigt Jahreswechsel

### Deep Sleep
- Nach der Anzeige-Aktualisierung geht das Gerät in Deep Sleep
- Der ESP32 verbraucht im Deep Sleep nur ~10µA
- Die RTC läuft weiter und weckt das Gerät zur richtigen Zeit
- Dies maximiert die Batterielaufzeit

## Dateien

- `Daily_Reminder.ino` - Hauptprogramm
- `Credentials.h` - WiFi-Zugangsdaten (muss angepasst werden!)
- `icons.h` - Yes/No Icon-Bitmaps (64x64 Pixel)
- `README.md` - Diese Datei

## Anpassungen

### Andere Zeitzone
Ändere in `Daily_Reminder.ino`:
```cpp
const long gmtOffset_sec = 3600;        // UTC Offset in Sekunden
const int daylightOffset_sec = 3600;    // Sommerzeit-Offset
```

Beispiele:
- Berlin/Zürich: `gmtOffset_sec = 3600` (UTC+1)
- London: `gmtOffset_sec = 0` (UTC+0)
- New York: `gmtOffset_sec = -18000` (UTC-5)

### Andere Icons
Die Icons sind 64x64 Pixel Bitmaps in der Datei `icons.h`. Du kannst sie mit einem Bitmap-Editor ersetzen.

Online-Tool zum Konvertieren: [Image to C Array](https://notisrac.github.io/FileToCArray/)

## Fehlerbehebung

### WiFi-Verbindung schlägt fehl
- Überprüfe die Zugangsdaten in `Credentials.h`
- Stelle sicher, dass das WiFi-Netzwerk ein 2.4 GHz Netzwerk ist (ESP32 unterstützt kein 5 GHz)
- Prüfe, ob das Netzwerk sichtbar ist und keine MAC-Filter aktiv sind

### Zeit ist falsch
- Überprüfe die Zeitzone-Einstellungen (`gmtOffset_sec` und `daylightOffset_sec`)
- Stelle sicher, dass die WiFi-Verbindung beim ersten Start funktioniert hat

### Display zeigt nichts an
- Drücke den Reset-Button am M5Stack Core Ink
- Überprüfe, ob die Batterie geladen ist
- Stelle sicher, dass das Programm erfolgreich hochgeladen wurde

### Batterie entlädt sich zu schnell
- Überprüfe, ob das Gerät wirklich in Deep Sleep geht (Serial Monitor zeigt "Gehe in Deep Sleep...")
- Im Test-Modus ist der Verbrauch höher (wacht alle 2 Minuten auf)
- Stelle sicher, dass für den Produktivbetrieb `#define Test` auskommentiert ist

## Serielle Ausgabe

Für Debugging kannst du den Serial Monitor öffnen (115200 Baud):
- Zeigt WiFi-Verbindungsstatus
- Zeigt synchronisierte Zeit
- Zeigt Batteriestand
- Zeigt ob YES oder NO angezeigt wird
- Zeigt Deep Sleep Dauer

## Lizenz

Dieses Projekt steht unter freier Verwendung für private und Lernzwecke.

## Credits

- M5Stack für die hervorragenden Hardware-Bibliotheken
- Icon-Design optimiert für E-Ink Display
